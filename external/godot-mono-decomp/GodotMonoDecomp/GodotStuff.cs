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

	public static string TrimPrefix(string path, string prefix)
	{
		if (!string.IsNullOrEmpty(path) && !string.IsNullOrEmpty(prefix) && path.StartsWith(prefix))
		{
			return path.Substring(prefix.Length);
		}
		return path;
	}


	public static string GetScriptPathAttributeValue(MetadataReader metadata, TypeDefinitionHandle h)
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


	public static Dictionary<string, TypeDefinitionHandle> CreateFileMap(MetadataFile module,
		IEnumerable<TypeDefinitionHandle> typesToDecompile,
		IEnumerable<string> filesInOriginal,
		bool useNestedDirectoriesForNamespaces)
	{
		var fileMap = new Dictionary<string, TypeDefinitionHandle>();
		var typeMap = new Dictionary<TypeDefinitionHandle, TypeDefinitionHandle>();
		var metadata = module.Metadata;
		var paths_found_in_attributes = new HashSet<string>();

		var processAgain = new HashSet<TypeDefinitionHandle>();
		var namespaceToFile = new Dictionary<string, List<string>>();
		var namespaceToDirectory = new Dictionary<string, HashSet<string>>();
		void addToNamespaceToFile(string ns, string file)
		{
			if (namespaceToFile.ContainsKey(ns))
			{
				namespaceToFile[ns].Add(file);
				namespaceToDirectory[ns].Add(Path.GetDirectoryName(file));
			}
			else
			{
				namespaceToFile[ns] = new List<string> { file };
				namespaceToDirectory[ns] = new HashSet<string> { Path.GetDirectoryName(file) };
			}
		}
		foreach (var h in typesToDecompile)
		{
			var type = metadata.GetTypeDefinition(h);
			var scriptPath = GodotStuff.TrimPrefix(GodotStuff.GetScriptPathAttributeValue(metadata, h), "res://");
			if (!string.IsNullOrEmpty(scriptPath))
			{
				addToNamespaceToFile(metadata.GetString(type.Namespace), scriptPath);
				fileMap[scriptPath] = h;
			}
			else
			{
				processAgain.Add(h);
			}
		}


		string GetAutoFileNameForHandle(TypeDefinitionHandle h)
		{
			var type = metadata.GetTypeDefinition(h);

			string file = GodotProjectDecompiler.CleanUpFileName(metadata.GetString(type.Name), ".cs");
			string ns = metadata.GetString(type.Namespace);
			string path;
			if (string.IsNullOrEmpty(ns))
			{
				return file;
			}
			else
			{
				string dir = useNestedDirectoriesForNamespaces ? GodotProjectDecompiler.CleanUpPath(ns) : GodotProjectDecompiler.CleanUpDirectoryName(ns);
				return Path.Combine(dir, file).Replace(Path.DirectorySeparatorChar, '/');
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
				)
				.ToList();

			if (possibles.Count == 0)
			{
				possibles = filesInOriginal.Where(f =>
					!fileMap.ContainsKey(f) &&
					Path.GetFileName(f).ToLower() == Path.GetFileName(file_path).ToLower()
				).ToList();
			}

			if (possibles.Count == 1)
			{
				scriptPath = possibles[0];
			}
			else if (possibles.Count > 1)
			{
				possibles = possibles.Where(f => f.EndsWith(file_path)).ToList();
				if (possibles.Count == 1)
				{
					scriptPath = possibles[0];
				}
			}
			if (scriptPath == "" && possibles.Count > 1){
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
			if (real_path != "" && real_path != "<multiple>")
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

		foreach (var h in processAgainAgain)
		{
			var type = metadata.GetTypeDefinition(h);
			var ns = metadata.GetString(type.Namespace);
			var auto_path = GetAutoFileNameForHandle(h);
			string p = "";
			var namespaceParts = ns.Split('.');
			var parentNamespace = ns.Contains('.') ? ns.Split('.')[0] : "";
			if (ns != "" && ns != null)
			{
				// pop off the first part of the path, if necessary
				var fileStem = GodotStuff.RemoveNamespacePartOfPath(auto_path, ns);
				var directories = namespaceToDirectory.ContainsKey(ns)
					? namespaceToDirectory[ns]
					: new HashSet<string>();

				if (directories.Count == 1 && (directories.First() != "" && directories.First() != null))
				{
					p = Path.Combine(directories.First(), fileStem);
				}
				// check if the namespace has a parent
				else if (directories.Count == 0 && parentNamespace.Length != 0 &&
						namespaceToFile.ContainsKey(parentNamespace))
				{
					var parentDirectories = namespaceToDirectory.ContainsKey(parentNamespace)
						? namespaceToDirectory[parentNamespace]
						: new HashSet<string>();
					var child = ns.Substring(parentNamespace.Length + 1).Replace('.', '/');
					fileStem = Path.Combine(child, Path.GetFileName(auto_path));
					if (parentDirectories.Count == 1 && (parentDirectories.First() != "" &&
														parentDirectories.First() != null))
					{
						p = Path.Combine(parentDirectories.First(), fileStem);
					}

					directories = parentDirectories;
				}

				if (p == "" && directories.Count > 1)
				{
					var commonRoot = GodotStuff.FindCommonRoot(directories);
					if (commonRoot != "")
					{
						p = Path.Combine(commonRoot, fileStem);
					}
				}
			}
			if (p == "" || fileMap.ContainsKey(p))
			{
				p = auto_path;
			}
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

		return fileMap;
	}


	public static List<PartialTypeInfo> GetPartialGodotTypes(MetadataFile module,
		IEnumerable<TypeDefinitionHandle> typesToDecompile,
		DecompilerTypeSystem ts)
	{

		var partialTypes = new List<PartialTypeInfo>();

		var allTypeDefs = ts.GetAllTypeDefinitions();
		foreach (var type in typesToDecompile)
		{
			// get the type definition from allTypeDefs where the metadata token matches
			var typeDef = allTypeDefs
				.Select(td => td)
				.FirstOrDefault(td => td.MetadataToken.Equals(type));

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
		}

		return partialTypes;
	}


	public static void RemoveExtraneousFiles(List<ProjectItemInfo> files, string TargetDirectory)
	{
		// get every file that contains GodotPlugins.Game
		var gameFiles = files
			.Where(f => f.FileName.Contains("GodotPlugins.Game") || f.FileName.Contains("GodotPlugins/Game") ||
			            f.FileName.Contains("AssemblyInfo.cs"))
			.ToList();
		// remove them from the output
		foreach (var file in gameFiles)
		{
			var path = Path.Combine(TargetDirectory, file.FileName);
			// delete them from the disk
			try
			{
				File.Delete(path);
			}
			catch (IOException)
			{
				// ignore
			}

			files.Remove(file);
		}

		RemoveDirIfEmpty(Path.Combine(TargetDirectory, "GodotPlugins.Game"));
		RemoveDirIfEmpty(Path.Combine(TargetDirectory, "Properties"));
	}

	private static void RemoveDirIfEmpty(string dir)
	{
		if (Directory.Exists(dir) && Directory.GetFiles(dir).Length == 0)
		{
			try
			{
				Directory.Delete(dir);
			}
			catch (IOException)
			{
				// ignore
			}
		}
	}

	public static bool IsGodotPartialClass(ITypeDefinition entity)
	{
		// check if the entity is a member of a type that derives from GodotObject
		return entity != null && entity.GetAllBaseTypes().Any(t => t.Name == "GodotObject");
	}

	public static string FindScriptNamespaceInChildren(IEnumerable<AstNode> children)
	{
		// using StreamWriter w = new StreamWriter(Path.Combine(TargetDirectory, file.Key));
		foreach (var child in children)
		{
			switch (child)
			{
				case NamespaceDeclaration namespaceDeclaration:
				{
					return namespaceDeclaration.FullName;
				}
			}
		}

		return "";
	}


	// list all .cs files in the directory and subdirectories
	public static IEnumerable<string> ListCSharpFiles(string directory, bool absolute)
	{
		try
		{
			// check if the directory exists
			if (!Directory.Exists(directory))
			{
				return Enumerable.Empty<string>();
			}

			var files = Directory.GetFiles(directory, "*.cs", SearchOption.AllDirectories);
			if (!absolute)
			{
				// if not absolute, return the relative paths
				files = files.Select(f => FileUtility.GetRelativePath(directory, f)).ToArray();
			}

			return files;
		}
		catch (IOException)
		{
			// if the directory doesn't exist, return an empty list
			return Enumerable.Empty<string>();
		}
	}

	public static string RemoveNamespacePartOfPath(string path, string ns)
	{
		// remove the namespace part of the path
		if (ns == "")
		{
			return path;
		}

		// find the ns in the path
		if (!path.StartsWith(ns))
		{
			ns = ns.Replace('.', '/');
		}

		if (path.StartsWith(ns))
		{
			return path.Substring(ns.Length + 1);
		}

		return path;
	}

	public static string FindCommonRoot(IEnumerable<string> paths)
	{
		if (paths == null || !paths.Any())
		{
			return "";
		}

		// sort by length to find the shortest path first
		paths = paths.OrderBy(p => p.Length);

		var commonRoot = paths.First();
		foreach (var path in paths.Skip(1))
		{
			while (!path.StartsWith(commonRoot, StringComparison.OrdinalIgnoreCase))
			{
				commonRoot = Path.GetDirectoryName(commonRoot);
				if (commonRoot == null)
				{
					return "";
				}
			}
		}

		return commonRoot;
	}

	public static void EnsureDir(string TargetDirectory)
	{
		// ensure the directory exists for new_path
		if (!Directory.Exists(TargetDirectory))
		{
			try
			{
				Directory.CreateDirectory(TargetDirectory);
			}
			catch (IOException)
			{
				// File.Delete(dir);
				try
				{
					Directory.CreateDirectory(TargetDirectory);
				}
				catch (IOException)
				{
					try
					{
						Directory.CreateDirectory(TargetDirectory);
					}
					catch (IOException)
					{
						// ignore
					}
				}
			}
		}
	}

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
			return field.Name.StartsWith(BACKING_FIELD_PREFIX) &&
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

	public static bool IsBannedGodotTypeMember(IEntity entity)
	{
		if (!IsGodotPartialClass(entity.DeclaringTypeDefinition))
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
				if (GetGodotSourceGeneratorMethodNames().Contains(method.Name))
				{
					return true;
				}

				break;
			case IEvent @event:
				if (GetSignalsInClass(@event.DeclaringTypeDefinition).Contains(@event.ReturnType.GetDefinition()) &&
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
				if (enclosingClass != null && bannedEmbeddedClasses.Contains(type.Name) &&
				    type.DirectBaseTypes.Any(t => t.FullName.Contains(enclosingClassBase.First().Name)))
				{
					return true;
				}

				break;
			default:
				break;
		}

		var attributes = entity.GetAttributes();
		// I think we got all the banned methods and generated classes, so we don't need this anymore
		// check to see if any of the attributes are System.ComponentModel.EditorBrowsableAttribute with a System.ComponentModel.EditorBrowsableState.Never argument
		// if (attributes.Any(a =>
		// 	    a.AttributeType.FullName == "System.ComponentModel.EditorBrowsableAttribute"
		// 	    && a.FixedArguments is [{ Value: (int)EditorBrowsableState.Never or EditorBrowsableState.Never } _]))
		// {
		// 	return true;
		// }


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

	/// Get a list of method names that are generated by the Godot source generator
	public static IEnumerable<string> GetGodotSourceGeneratorMethodNames()
	{
		List<string> banned_godot_methods = new List<string> {
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
		};
		return banned_godot_methods;

		// TODO: this, but in the meantime, just return a list of static string
		// In ScriptManagerBridge.cs, there are a number of type.GetMethod calls with the first parameter being a string
		// Get the GodotSharp module dependency from the main module
		// var godotSharpModule = module.Compilation.Modules.FirstOrDefault(m => m.AssemblyName == "GodotSharp");
		// if (godotSharpModule == null)
		// {
		// 	// We can't find it, just return true;
		// 	return Enumerable.Empty<string>();
		// }
		// // Find any top-level type definitions in the GodotSharp module that contain "SourceGenerators" in their name
		// var source_gens = godotSharpModule.TopLevelTypeDefinitions.Where(t => t.FullName.Contains("SourceGenerators"));
		//
		// var returned_collection = new List<string>();
		//
		// // Find any top-level type definitions in the GodotSharp module that contain "Bridge" in their name
		// var bridge = godotSharpModule.TopLevelTypeDefinitions.FirstOrDefault(t => t.Name == "ScriptManagerBridge");
		// if (bridge == null)
		// {
		// 	return Enumerable.Empty<string>();
		// }
		//
		// // get all the non-compiler generated methods in the bridge type
		// var methods = bridge.Methods.Where(m => !m.IsCompilerGenerated());
		//
		// foreach (IMethod method in methods)
		// {
		// 	// check to see if the method has a body
		// 	if (method.HasBody)
		// 	{
		// 		var method_def = godotSharpModule.MetadataFile.Metadata.GetMethodDefinition((MethodDefinitionHandle)method.MetadataToken);
		// 		var methodBody = godotSharpModule.MetadataFile.GetMethodBody(method_def.RelativeVirtualAddress);
		// 		// Read the IL and get the ILFunction
		// 		ILReader ilReader = new ILReader(methodBody);
		// 		var ilFunction =
		//
		//
		//
		//
		// 		var thing = "";
		// 		if (!thing.Contains(".getMethod"))
		// 		{
		// 			continue;
		// 		}
		// 		// check the method body for any strings (i.e. surrounded by quotes)
		// 		if (thing.Contains("\""))
		// 		{
		// 			// get all the strings using the regex '"(.*?)"'
		// 			var matches = Regex.Matches(thing, "\"(.*?)\"");
		// 			foreach (Match match in matches)
		// 			{
		// 				returned_collection.Add(match.Value);
		// 			}
		// 		}
		//
		// 	}
		//
		// }
		// return returned_collection;
	}
}
