using System;
using System.Collections;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.IO;
using System.Linq;
using System.Reflection.Metadata;
using System.Threading;
using System.Threading.Tasks;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.CSharp.OutputVisitor;
using ICSharpCode.Decompiler.CSharp.ProjectDecompiler;
using ICSharpCode.Decompiler.CSharp.Syntax;
using ICSharpCode.Decompiler.CSharp.Transforms;
using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.Decompiler.Semantics;
using ICSharpCode.Decompiler.TypeSystem;
using ICSharpCode.Decompiler.Util;

namespace GodotMonoDecomp;

/// <summary>
/// This transform is used to remove Godot.ScriptPathAttribute from the AST.
/// </summary>
public class RemoveGodotScriptPathAttribute : IAstTransform
{

	public void Run(AstNode rootNode, TransformContext context)
	{

		foreach (var section in rootNode.Children.OfType<NamespaceDeclaration>())
		{
			Run(section, context);
		}
		foreach (var section in rootNode.Children.OfType<TypeDeclaration>())
		{
			Run(section, context);
		}
		foreach (var section in rootNode.Children.OfType<AttributeSection>())
		{
			foreach (var attribute in section.Attributes)
			{
				var trr = attribute.Type.Annotation<TypeResolveResult>();
				if (trr == null)
					continue;

				string fullName = trr.Type.FullName;
				var arguments = attribute.Arguments;
				switch (fullName)
				{
					case "Godot.ScriptPathAttribute":
					{
						attribute.Remove();
						break;
					}
				}
			}

			if (section.Attributes.Count == 0)
			{
				section.Remove();
			}
		}
	}
}


public static class GodotStuff
{
	public const string BACKING_FIELD_PREFIX = "backing_";


	public static string? GetScriptPathAttributeValue(MetadataReader metadata, TypeDefinitionHandle h)
	{
		var type = metadata.GetTypeDefinition(h);
		var attrs = type.GetCustomAttributes();
		foreach (var attrHandle in attrs)
		{
			var customAttr = metadata.GetCustomAttribute(attrHandle);
			var str = customAttr.ToString();
			var attrName = customAttr.GetAttributeType(metadata).GetFullTypeName(metadata).ToString();
			if (attrName != "Godot.ScriptPathAttribute")
			{
				continue;
			}
			var value = customAttr.DecodeValue(MetadataExtensions.MinimalAttributeTypeProvider);
			if (value.FixedArguments.Length == 0)
			{
				continue;
			}
			return value.FixedArguments[0].Value as string;
		}

		return null;
	}

	// change this to a generator that yields the script paths
	public static IEnumerable<string> GetCanonicalGodotScriptPaths(MetadataFile module,
		IEnumerable<TypeDefinitionHandle> typesToDecompile,
		Dictionary<string, GodotScriptMetadata>? scriptMetadata)
	{
		Dictionary<string, string>? metadataFQNToFileMap = scriptMetadata?.ToDictionary(
			pair => pair.Value.Class.GetFullClassName(),
			pair => pair.Key,
			StringComparer.OrdinalIgnoreCase);

		var metadata = module.Metadata;
		foreach (var h in typesToDecompile)
		{
			var scriptPath = Common.TrimPrefix(GetScriptPathAttributeValue(metadata, h) ?? "", "res://");
			if (!string.IsNullOrEmpty(scriptPath))
			{
				yield return scriptPath;
			}
			else if (metadataFQNToFileMap != null)
			{
				var fqn = metadata.GetTypeDefinition(h).GetFullTypeName(metadata).ToString();
				if (metadataFQNToFileMap.TryGetValue(fqn, out var filePath))
				{
					yield return Common.TrimPrefix(filePath, "res://");
				}
			}
		}
	}


	public static Dictionary<string, TypeDefinitionHandle> CreateFileMap(MetadataFile module,
		IEnumerable<TypeDefinitionHandle> typesToDecompile,
		List<string> filesInOriginal,
		Dictionary<string, GodotScriptMetadata>? scriptMetadata,
		IEnumerable<string>? excludedSubdirectories,
		bool useNestedDirectoriesForNamespaces)
	{
		var fileMap = new Dictionary<string, TypeDefinitionHandle>();
		var metadata = module.Metadata;
		Dictionary<string, string> metadataFQNToFileMap = null;
		if (scriptMetadata != null)
		{
			// create a map of metadata FQN to file path
			metadataFQNToFileMap = scriptMetadata.ToDictionary(
				pair => pair.Value.Class.GetFullClassName(),
				pair => pair.Key,
				StringComparer.OrdinalIgnoreCase);
		}

		excludedSubdirectories = excludedSubdirectories?.Select(e => {
			if (e.EndsWith('\\'))
			{
				// replace it
				return e.Substring(0, e.Length - 1) + '/';
			} else if (e.EndsWith('/')) {
				return e;
			}
			return e + '/';
		}).ToList();

		bool nosubdirs = excludedSubdirectories == null || !excludedSubdirectories.Any();

		// ensure that all paths use forward slashes in fileMap to match Godot's path style
		string _NormalizePath(string path)
		{
			return path.Replace(Path.DirectorySeparatorChar, '/');
		}

		string _PathCombine(string a, string b)
		{
			return _NormalizePath(Path.Combine(a, b));
		}

		bool IsExcludedSubdir(string? dir)
		{
			if (nosubdirs || string.IsNullOrEmpty(dir))
			{
				return false;
			}
			return excludedSubdirectories!.Any(e => dir.StartsWith(e, StringComparison.OrdinalIgnoreCase));
		}

		bool IsInExcludedSubdir(string? file)
		{
			if (nosubdirs || string.IsNullOrEmpty(file))
			{
				return false;
			}
			return IsExcludedSubdir(Path.GetDirectoryName(file));
		}

		var processAgain = new HashSet<TypeDefinitionHandle>();
		var namespaceToFile = new Dictionary<string, List<string>>();
		var namespaceToDirectory = new Dictionary<string, HashSet<string>>();
		void addToNamespaceToFile(string ns, string file)
		{
			var dir = Path.GetDirectoryName(file) ?? "";
			if (namespaceToFile.ContainsKey(ns))
			{
				namespaceToFile[ns].Add(file);
				namespaceToDirectory[ns].Add(dir);
			}
			else
			{
				namespaceToFile[ns] = new List<string> { file };
				namespaceToDirectory[ns] = new HashSet<string> { dir };
			}
		}
		foreach (var h in typesToDecompile)
		{
			var type = metadata.GetTypeDefinition(h);
			var scriptPath = Common.TrimPrefix(GetScriptPathAttributeValue(metadata, h) ?? "", "res://");

			// we explicitly don't check if it's in an excluded subdirectory here because a script path attribute means
			// that the file is referenced by other files in the project, so the path MUST match.
			if (!string.IsNullOrEmpty(scriptPath))
			{
				addToNamespaceToFile(metadata.GetString(type.Namespace),scriptPath);
				fileMap[scriptPath] = h;
			}
			else
			{
				// Same here.
				if (metadataFQNToFileMap != null)
				{
					// check if the type has a metadata FQN in the script metadata
					var fqn = type.GetFullTypeName(metadata).ToString();
					if (metadataFQNToFileMap.TryGetValue(fqn, out var filePath))
					{
						filePath = Common.TrimPrefix(filePath, "res://");
						addToNamespaceToFile(metadata.GetString(type.Namespace), filePath);
						fileMap[filePath] = h;
						continue;
					}
				}
				processAgain.Add(h);
			}
		}

		string default_dir = "src";
		while (IsExcludedSubdir(default_dir)){
			default_dir = "_" + default_dir;
		}


		string GetAutoFileNameForHandle(TypeDefinitionHandle h)
		{
			var type = metadata.GetTypeDefinition(h);

			string file = GodotProjectDecompiler.CleanUpFileName(metadata.GetString(type.Name), ".cs");
			string ns = metadata.GetString(type.Namespace);
			if (string.IsNullOrEmpty(ns))
			{
				return file;
			}
			else
			{
				string dir = useNestedDirectoriesForNamespaces ? GodotProjectDecompiler.CleanUpPath(ns) : GodotProjectDecompiler.CleanUpDirectoryName(ns);
				// ensure dir separator is '/'
				dir = dir.Replace(Path.DirectorySeparatorChar, '/');
				// TODO: come back to this
				if (IsExcludedSubdir(dir) /*|| dir == ""*/)
				{
					dir = default_dir;
				}
				return _PathCombine(dir, file);
			}
		}

		string GetPathFromOriginalFiles(string file_path)
		{
			// otherwise, try to find it in the original directory files
			string scriptPath = "";
			// empty vector of strings
			var possibles = filesInOriginal.Where(f =>
					!fileMap.ContainsKey(f) &&
					Path.GetFileName(f) == Path.GetFileName(file_path)
					&& !IsInExcludedSubdir(f)
				)
				.ToList();

			if (possibles.Count == 0)
			{
				possibles = filesInOriginal.Where(f =>
					!fileMap.ContainsKey(f) &&
					Path.GetFileName(f).ToLower() == Path.GetFileName(file_path).ToLower()
					&& !IsInExcludedSubdir(f)
				).ToList();
			}

			if (possibles.Count == 1)
			{
				scriptPath = possibles[0];
			}
			else if (possibles.Count > 1)
			{
				possibles = possibles.Where(f => f.EndsWith(file_path, StringComparison.OrdinalIgnoreCase)).ToList();
				if (possibles.Count == 1)
				{
					scriptPath = possibles[0];
				}
			}
			if (string.IsNullOrEmpty(scriptPath) && possibles.Count > 1){
				return "<multiple>";
			}
			return scriptPath;
		}

		var potentialMap = new Dictionary<string, List<TypeDefinitionHandle>>();


		var processAgainAgain = new HashSet<TypeDefinitionHandle>();
		var dupes = new HashSet<TypeDefinitionHandle>();

		foreach (var h in processAgain)
		{
			var path = GetAutoFileNameForHandle(h);
			var real_path = GetPathFromOriginalFiles(path);
			if (real_path != "" && real_path != "<multiple>" && !IsInExcludedSubdir(real_path))
			{
				if (!potentialMap.ContainsKey(real_path))
				{
					potentialMap[real_path] = new List<TypeDefinitionHandle>();
				}
				potentialMap[real_path].Add(h);
			} else {
				if (real_path == "<multiple>" && !dupes.Contains(h))
				{
					dupes.Add(h);
				}
				else
				{
					processAgainAgain.Add(h);
				}
			}
		}

		foreach (var pair in potentialMap)
		{
			if (pair.Value.Count == 1)
			{
				var type = metadata.GetTypeDefinition(pair.Value[0]);
				addToNamespaceToFile(metadata.GetString(type.Namespace), pair.Key);
				fileMap[pair.Key] = pair.Value[0];
			} else {
				foreach (var h in pair.Value)
				{
					processAgainAgain.Add(h);
				}
			}
		}


		HashSet<string> GetNamespaceDirectories(string ns)
		{
			return namespaceToDirectory.TryGetValue(ns, out var v1)
				? v1
				: [];
		}

		foreach (var h in processAgainAgain)
		{
			var type = metadata.GetTypeDefinition(h);
			var ns = metadata.GetString(type.Namespace);
			var auto_path = GetAutoFileNameForHandle(h);
			string? p = null;
			var namespaceParts = ns.Split('.');
			var parentNamespace = ns.Contains('.') ? ns.Split('.')[0] : "";
			if (ns != "" && ns != null)
			{
				// pop off the first part of the path, if necessary
				var fileStem = Common.RemoveNamespacePartOfPath(auto_path, ns);
				var directories = GetNamespaceDirectories(ns).Where(d => !string.IsNullOrEmpty(d) && !IsInExcludedSubdir(d)).ToHashSet();

				if (directories.Count == 1)
				{
					p = _PathCombine(directories.First(), fileStem);
				}
				// check if the namespace has a parent
				else if (string.IsNullOrEmpty(p) && directories.Count <= 1 && parentNamespace.Length != 0 &&
						namespaceToFile.ContainsKey(parentNamespace))
				{
					var parentDirectories = GetNamespaceDirectories(parentNamespace).Where(d => !string.IsNullOrEmpty(d) && !IsInExcludedSubdir(d)).ToHashSet();
					var child = ns.Substring(parentNamespace.Length + 1).Replace('.', '/');
					fileStem = _PathCombine(child, Path.GetFileName(auto_path));
					if (parentDirectories.Count == 1)
					{
						p = _PathCombine(parentDirectories.First(), fileStem);
					}

					directories = parentDirectories;
				}

				else if (string.IsNullOrEmpty(p) && directories.Count > 1)
				{
					var commonRoot = Common.FindCommonRoot(directories);
					if (!string.IsNullOrEmpty(commonRoot))
					{
						p = _PathCombine(commonRoot, fileStem);
					}
				}
			}
			if (string.IsNullOrEmpty(p) || fileMap.ContainsKey(p))
			{
				p = auto_path;
			}
			p = _NormalizePath(p);
			fileMap[p] = h;
		}

		foreach (var h in dupes)
		{
			var auto_path = GetAutoFileNameForHandle(h);
			var scriptPath = GetPathFromOriginalFiles(auto_path);

			if (scriptPath == "" || scriptPath == "<multiple>" || fileMap.ContainsKey(scriptPath))
			{
				scriptPath = auto_path;
			}

			fileMap[scriptPath] = h;
		}
		return fileMap.ToDictionary(
			pair => pair.Key,
			pair => pair.Value,
			StringComparer.OrdinalIgnoreCase);
	}


	public static List<PartialTypeInfo> GetPartialGodotTypes(DecompilerTypeSystem ts,
		IEnumerable<TypeDefinitionHandle> typesToDecompile)
	{

		var partialTypes = new List<PartialTypeInfo>();

		var allTypeDefs = ts.GetAllTypeDefinitions();
		void addPartialTypeInfo(ITypeDefinition typeDef)
		{
			if (GodotStuff.IsGodotPartialClass(typeDef))
			{
				IEnumerable<IMember> fieldsAndProperties = typeDef.Fields.Concat<IMember>(typeDef.Properties);

				IEnumerable<IMember> allOrderedMembers =
					fieldsAndProperties.Concat(typeDef.Events).Concat(typeDef.Methods);

				var allOrderedEntities = typeDef.NestedTypes.Concat<IEntity>(allOrderedMembers).ToArray();

				var partialTypeInfo = new PartialTypeInfo(typeDef);
				foreach (var member in allOrderedEntities)
				{
					if (GodotStuff.IsBannedGodotTypeMember(member))
					{
						partialTypeInfo.AddDeclaredMember(member.MetadataToken);
					}
				}

				partialTypes.Add(partialTypeInfo);
			}
			// embedded types, too
			for (int i = 0; i < typeDef.NestedTypes.Count; i++)
			{
				var nestedType = typeDef.NestedTypes[i];
				if (GodotStuff.IsGodotPartialClass(nestedType))
				{
					addPartialTypeInfo(nestedType);
				}
			}
		}
		foreach (var type in typesToDecompile)
		{
			// get the type definition from allTypeDefs where the metadata token matches
			var typeDef = allTypeDefs
				.Select(td => td)
				.FirstOrDefault(td => td.MetadataToken.Equals(type));
			if (typeDef == null)
			{
				continue;
			}
			addPartialTypeInfo(typeDef);

		}

		return partialTypes;
	}

	public static bool ModuleDependsOnGodotSharp(MetadataFile module)
	{
		return module.AssemblyReferences.Any(r => r.Name == "GodotSharp");
	}

	public static bool ModuleDependsOnGodotSourceGenerators(MetadataFile module)
	{
		return module.AssemblyReferences.Any(r => r.Name == "Godot.SourceGenerators");
	}

	public static bool IsGodotPartialClass(ITypeDefinition entity)
	{
		if (entity == null)
		{
			return false;
		}
		// check if the entity is a member of a type that derives from GodotObject
		if (!entity.GetAllBaseTypes().Any(t => t.Name == "GodotObject")) return false;

		// check if it's version 3 or lower; version 3 had no partial classes
		if (entity.ParentModule?.MetadataFile != null)
		{
			if (GetGodotVersion(entity.ParentModule.MetadataFile)?.Major <= 3){
				// Just in case the creator somehow built GodotSharp themselves without a version number
				return ModuleDependsOnGodotSourceGenerators(entity.ParentModule.MetadataFile);
			}
		}

		return true;
	}


	// list all .cs files in the directory and subdirectories

	public static bool IsSignalDelegate(IEntity entity)
	{
		var attributes = entity.GetAttributes();

		// check if any of the attributes are "Signal"
		if (attributes.Any(a => a.AttributeType.FullName == "Godot.SignalAttribute"))
		{
			return true;
		}

		if (attributes.Any(a => a.AttributeType.Name == "SignalAttribute"))
		{
			return true;
		}

		return false;
	}

	public static IEnumerable<IType> GetSignalsInClass(ITypeDefinition entity)
	{
		return entity.NestedTypes.Where(IsSignalDelegate);
	}

	public static bool IsBackingSignalDelegateField(IEntity entity)
	{
		if (entity is IField field)
		{
			return field.Name.StartsWith(BACKING_FIELD_PREFIX) && field.DeclaringTypeDefinition != null &&
			       GetSignalsInClass(field.DeclaringTypeDefinition).Contains(field.Type.GetDefinition());
		}

		return false;
	}

	public static IEnumerable<IEntity> GetBackingSignalDelegateFieldsInClass(ITypeDefinition entity)
	{
		return entity.Fields.Where(IsBackingSignalDelegateField);
	}

	public static IEnumerable<string> GetBackingSignalDelegateFieldNames(ITypeDefinition entity)
	{
		return GetBackingSignalDelegateFieldsInClass(entity).Select(f => f.Name);
	}

	public static Version? GetGodotVersion(MetadataFile file)
	{
		if (file == null)
		{
			return null;
		}
		// look through all the assembly references in the file until we find one named "GodotSharp"
		var godotSharpReference = file.AssemblyReferences.FirstOrDefault(r => r.Name == "GodotSharp");
		return godotSharpReference?.Version;
	}

	public static Version? ParseGodotVersionFromString(string versionString)
	{
		if (string.IsNullOrWhiteSpace(versionString))
		{
			return null;
		}

		if (Version.TryParse(versionString, out var v))
		{
			return v;
		}
		// else, split the string by '.' and parse each part
		var parts = versionString.Split('.');
		int verMajor = 0;
		int verMinor = 0;
		int verBuild = 0;
		int verRevision = -1;
		int len = parts.Length > 4 ? 4 : parts.Length;
		for (int i = 0; i < len; i++)
		{
			var part = parts[i];
			// check if it's a valid integer
			if (int.TryParse(part, out var intPart))
			{
				switch (i)
				{
					case 0:
						verMajor = intPart;
						break;
					case 1:
						verMinor = intPart;
						break;
					case 2:
						verBuild = intPart;
						break;
					case 3:
						verRevision = intPart;
						break;
				}
			}
			else
			{
				break;
			}
		}

		if (verMajor == 0)
		{
			return null;
		}
		return new Version(verMajor, verMinor, verBuild, verRevision);
	}

	/// <summary>
	/// Check to see if any of the attributes are System.ComponentModel.EditorBrowsableAttribute
	/// with a System.ComponentModel.EditorBrowsableState.Never argument
	/// </summary>
	public static bool HasEditorNonBrowsableAttribute(IEntity entity)
	{
		return entity.GetAttributes().Any(a =>
			    a.AttributeType.FullName == "System.ComponentModel.EditorBrowsableAttribute"
			    && a.FixedArguments is [
			    {
				    Value: (int)System.ComponentModel.EditorBrowsableState.Never or System.ComponentModel.EditorBrowsableState.Never
			    } _]);
	}


	public static bool IsBannedGodotTypeMember(IEntity entity)
	{
		if (entity.DeclaringTypeDefinition == null || !IsGodotPartialClass(entity.DeclaringTypeDefinition))
		{
			return false;
		}

		switch (entity)
		{
			case IField field:
				if (IsBackingSignalDelegateField(field))
				{
					return true;
				}

				break;
			case IProperty property:

				break;
			case IMethod method:
				// check if the method is a method that is generated by the Godot source generator
				if (BANNED_GODOT_METHODS.Contains(method.Name))
				{
					return true;
				}

				// if the method name is EmitSignal<SignalName> and it's a protected or private void method, then it's an auto-generated signal emitter
				if (
					method is { IsVirtual: false, Accessibility: Accessibility.Internal or Accessibility.Protected or Accessibility.ProtectedOrInternal or Accessibility.ProtectedAndInternal or Accessibility.Private } &&
					method.Name.StartsWith("EmitSignal") && method.ReturnType.FullName == "System.Void")
				{
					return true;
				}
				if (method.IsVirtual)
				{
					break;
				}

				// auto-generated getter methods for properties of parent classes
				bool isGetter = method.Name.Contains(".get_");
				bool isSetter = method.Name.Contains(".set_");
				if (isGetter || isSetter)
				{
					var parts = method.Name.Split(isGetter ? ".get_" : ".set_");
					if (parts.Length <= 1)
					{
						break;
					}
					var parentClass = parts[0].Split("<")[0];
					var memberName = parts[1];
					var baseTypes = method.DeclaringType.GetAllBaseTypes();
					var baseType = baseTypes.FirstOrDefault(t => t.Name == parentClass);
					if (baseType == null)
					{
						break;
					}
					IMember member = baseType.GetMembers().FirstOrDefault(m => m.Name == memberName);
					if (member != null && member is IProperty prop)
					{
						var memberAccessorName = isGetter ? prop.Getter?.Name : prop.Setter?.Name;

						if (memberAccessorName == method.Name.Split('.').Last())
						{
							return true;
						}

						break;
					}
				}

				break;
			case IEvent @event:
				if (@event.DeclaringTypeDefinition != null && GetSignalsInClass(@event.DeclaringTypeDefinition).Contains(@event.ReturnType.GetDefinition()) &&
				    GetBackingSignalDelegateFieldNames(@event.DeclaringTypeDefinition)
					    .Contains(BACKING_FIELD_PREFIX + @event.Name))
				{
					return true;
				}

				break;
			case ITypeDefinition type:
				var bannedEmbeddedClasses = new List<string> { "MethodName", "PropertyName", "SignalName" };
				var enclosingClass = type.DeclaringTypeDefinition;
				// check if the type is a nested type
				var enclosingClassBase = enclosingClass?.DirectBaseTypes;
				// check if the type is one of the banned embedded classes, and also derives from the base class's embedded class
				if (enclosingClass != null && enclosingClassBase != null && bannedEmbeddedClasses.Contains(type.Name) &&
				    type.DirectBaseTypes.Any(t => t.FullName.Contains(enclosingClassBase.First().Name)))
				{
					return true;
				}

				break;
			default:
				break;
		}

		// I think we got all the banned methods and generated classes, so we don't need this anymore
		// return HasEditorNonBrowsableAttribute(entity)

		return false;
	}

	/// <summary>
	/// Detects whether or not the type is the auto-generated GodotPlugins.Game.Main class.
	/// </summary>
	/// <param name="module"></param>
	/// <param name="type"></param>
	/// <returns></returns>
	public static bool IsGodotGameMainClass(MetadataFile module, TypeDefinitionHandle type)
	{
		// check if the entity is a member of a type that derives from Godot.GameMain
		var fullTypeName = module.Metadata.GetTypeDefinition(type).GetFullTypeName(module.Metadata).ToString();
		return fullTypeName == "GodotPlugins.Game.Main";
	}

	public static readonly ImmutableHashSet<string> BANNED_GODOT_METHODS = [
		"GetGodotSignalList",
		"GetGodotMethodList",
		"GetGodotPropertyList",
		"GetGodotPropertyDefaultValues",
		"InvokeGodotClassStaticMethod",
		"InvokeGodotClassMethod",
		"AddEditorConstructors",
		"InternalCreateInstance",
		"HasGodotClassSignal",
		"HasGodotClassMethod",
		"GetGodotClassPropertyValue",
		"SetGodotClassPropertyValue",
		"SaveGodotObjectData",
		"RestoreGodotObjectData",
		"RaiseGodotClassSignalCallbacks"
	];


}
