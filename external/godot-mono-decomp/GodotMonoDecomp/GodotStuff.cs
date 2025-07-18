using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection.Metadata;
using System.Threading;
using System.Threading.Tasks;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.CSharp.OutputVisitor;
using ICSharpCode.Decompiler.CSharp.ProjectDecompiler;
using ICSharpCode.Decompiler.CSharp.Syntax;
using ICSharpCode.Decompiler.TypeSystem;
using ICSharpCode.Decompiler.Util;

namespace GodotMonoDecomp;

public static class GodotStuff
{
	public const string BACKING_FIELD_PREFIX = "backing_";


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

	public static string FindScriptPathInChildren(IEnumerable<AstNode> children)
	{
		// using StreamWriter w = new StreamWriter(Path.Combine(TargetDirectory, file.Key));
		foreach (var child in children)
		{
			switch (child)
			{
				case TypeDeclaration typeDeclaration:
				{
					// check for the "ScriptPath" attribute"
					foreach (var attrSection in typeDeclaration.Attributes)
					{
						foreach (var attr in attrSection.Attributes)
						{
							if (attr.Type.ToString() == "ScriptPath")
							{
								// get the value of the attribute
								string scriptPath = attr.Arguments.First().ToString();

								// remove the quotes
								scriptPath = scriptPath.Substring(1, scriptPath.Length - 2);
								// remove "res://" from the beginning
								scriptPath = scriptPath.Substring(6);
								return scriptPath;
							}
						}
					}

					break;
				}
				case NamespaceDeclaration namespaceDeclaration:
				{
					var scriptPath = FindScriptPathInChildren(namespaceDeclaration.Children);
					if (scriptPath != "")
					{
						return scriptPath;
					}

					break;
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

	public static string GetNamespaceFromSyntaxTree(SyntaxTree syntaxTree)
	{
		var firstChild = syntaxTree.FirstChild;
		if (firstChild == null)
		{
			return "";
		}

		return FindScriptNamespaceInChildren(syntaxTree.Children);
	}

	public static string GetPathMinusFirstDirectory(string path)
	{
		// get the first directory in the path
		var slashPos = path.IndexOf('/');
		if (slashPos == -1)
		{
			slashPos = path.IndexOf('\\');
		}

		if (slashPos == -1)
		{
			return path; // no directory, return the path as is
		}

		// get the new path without the first directory	
		return path.Substring(slashPos + 1);
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

	public static void ProcessUnprocessedFiles(
		ConcurrentDictionary<IGrouping<string, TypeDefinitionHandle>, SyntaxTree> toProcess,
		string TargetDirectory, IEnumerable<string> odirfiles, int MaxDegreeOfParallelism,
		CancellationToken cancellationToken, DecompilerSettings Settings,
		ConcurrentDictionary<string, SyntaxTree> alreadyProcessed)
	{
		var pathSyntaxTreePairs = new List<KeyValuePair<string, SyntaxTree>>();
		var originalDirFiles = odirfiles.ToArray();
		var dupes = new Dictionary<string, IEnumerable<String>>();

		var alreadyProcessedList = alreadyProcessed.Select(f => f).ToList();
		var renamedFiles = alreadyProcessed.Keys.ToHashSet();
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

		for (int i = 0; i < alreadyProcessedList.Count; i++)
		{
			var syntaxTree = alreadyProcessedList[i].Value;
			var file = alreadyProcessedList[i].Key;
			var ns = GetNamespaceFromSyntaxTree(syntaxTree);
			addToNamespaceToFile(ns, file);
		}

		string GetPathFromOriginalDir(KeyValuePair<IGrouping<string, TypeDefinitionHandle>, SyntaxTree> pair,
			bool first = true)
		{
			var file = pair.Key;
			// otherwise, try to find it in the original directory files
			string scriptPath = "";
			//"/Users/nikita/Workspace/godot-ws/test-decomps/Taverna-decomp2"
			// empty vector of strings
			var possibles = originalDirFiles.Where(f =>
					!renamedFiles.Contains(f) &&
					Path.GetFileName(f) == Path.GetFileName(file.Key)
				)
				.ToList();

			if (scriptPath == "" && possibles.Count == 1)
			{
				scriptPath = possibles[0];
			}
			else if (scriptPath == "" && possibles.Count > 1)
			{
				possibles = possibles.Where(f => f.EndsWith(file.Key)).ToList();
				if (possibles.Count == 1)
				{
					scriptPath = possibles[0];
				}
			}

			if (scriptPath == "")
			{
				if (possibles.Count > 0 && !dupes.ContainsKey(file.Key))
				{
					dupes.Add(file.Key, possibles);
				}
			}

			return scriptPath;
		}

		var processAgain = new List<KeyValuePair<IGrouping<string, TypeDefinitionHandle>, SyntaxTree>>();
		var dupesProcess = new List<KeyValuePair<IGrouping<string, TypeDefinitionHandle>, SyntaxTree>>();

		foreach (var file_syntax_tree in toProcess)
		{
			var syntaxTree = file_syntax_tree.Value;
			var scriptPath = GetPathFromOriginalDir(file_syntax_tree);

			if (scriptPath == "" || renamedFiles.Contains(scriptPath))
			{
				if (!dupes.ContainsKey(file_syntax_tree.Key.Key))
				{
					processAgain.Add(file_syntax_tree);
				}
				else
				{
					dupesProcess.Add(file_syntax_tree);
				}

				continue;
			}

			var ns = GetNamespaceFromSyntaxTree(syntaxTree);
			addToNamespaceToFile(ns, scriptPath);
			// add a grouping of the file and syntax tree to the list
			pathSyntaxTreePairs.Add(
				new KeyValuePair<string, SyntaxTree>(scriptPath, syntaxTree));
			renamedFiles.Add(scriptPath);
		}

		foreach (var file_syntax_tree in processAgain)
		{
			string p = "";
			var ns = GetNamespaceFromSyntaxTree(file_syntax_tree.Value);
			var namespaceParts = ns.Split('.');
			var parentNamespace = ns.Contains('.') ? ns.Split('.')[0] : "";
			if (ns != "" && ns != null)
			{
				// pop off the first part of the path, if necessary
				var fileStem = RemoveNamespacePartOfPath(file_syntax_tree.Key.Key, ns);
				var directories = namespaceToDirectory.ContainsKey(ns)
					? namespaceToDirectory[ns]
					: new HashSet<string>();


				if (directories.Count > 1)
				{
					var w = "foo";
				}

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
					fileStem = Path.Combine(child, Path.GetFileName(file_syntax_tree.Key.Key));
					if (parentDirectories.Count == 1 && (parentDirectories.First() != "" &&
					                                     parentDirectories.First() != null))
					{
						p = Path.Combine(parentDirectories.First(), fileStem);
					}

					directories = parentDirectories;
				}

				if (p == "" && directories.Count > 1)
				{
					var commonRoot = FindCommonRoot(directories);

					if (commonRoot != "")
					{
						p = Path.Combine(commonRoot, fileStem);
					}
				}
			}

			if (p == "" || renamedFiles.Contains(p))
			{
				p = file_syntax_tree.Key.Key;
			}

			pathSyntaxTreePairs.Add(
				new KeyValuePair<string, SyntaxTree>(p, file_syntax_tree.Value));
			renamedFiles.Add(p);
		}

		foreach (var file_syntax_tree in dupesProcess)
		{
			var syntaxTree = file_syntax_tree.Value;
			var scriptPath = GetPathFromOriginalDir(file_syntax_tree, false);

			if (scriptPath == "" || renamedFiles.Contains(scriptPath))
			{
				scriptPath = file_syntax_tree.Key.Key;
			}

			// add a grouping of the file and syntax tree to the list
			pathSyntaxTreePairs.Add(
				new KeyValuePair<string, SyntaxTree>(scriptPath, syntaxTree));
			renamedFiles.Add(scriptPath);
		}


		Parallel.ForEach(
			Partitioner.Create(pathSyntaxTreePairs, loadBalance: true),
			new ParallelOptions {
				MaxDegreeOfParallelism = MaxDegreeOfParallelism,
				CancellationToken = cancellationToken
			},
			delegate(KeyValuePair<string, SyntaxTree> file_and_syntax_tree) {
				try
				{
					string path = Path.Combine(TargetDirectory, file_and_syntax_tree.Key);
					var syntaxTree = file_and_syntax_tree.Value;
					EnsureDir(Path.GetDirectoryName(path));
					using StreamWriter w = new StreamWriter(path);
					syntaxTree.AcceptVisitor(new CSharpOutputVisitor(w, Settings.CSharpFormattingOptions));
				}
				catch (Exception innerException) when (!(innerException is OperationCanceledException ||
				                                         innerException is DecompilerException))
				{
					Console.WriteLine("SHIT!!!!!!!!");
				}
			});
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

	// function to get a list of method names that are compiler generated, returns a collection of strings
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