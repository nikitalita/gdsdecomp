using System.Reflection.Metadata;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.Metadata;

namespace GodotMonoDecomp;

public class GodotModuleDecompiler
{
	public readonly PEFile module;
	public readonly GodotProjectDecompiler godotProjectDecompiler;
	public readonly Dictionary<string, TypeDefinitionHandle> fileMap;
	public readonly List<string> originalProjectFiles;


	public GodotModuleDecompiler(string assemblyPath, string[]? originalProjectFiles, string[]? ReferencePaths = null) {
		var decompilerSettings = new DecompilerSettings();
		decompilerSettings.UseNestedDirectoriesForNamespaces = true;
		this.originalProjectFiles = [.. (originalProjectFiles ?? []).Where(file => !string.IsNullOrEmpty(file)).Select(file => GodotStuff.TrimPrefix(file, "res://")).OrderBy(file => file, StringComparer.OrdinalIgnoreCase)];
		this.module = new PEFile(assemblyPath);
		var assemblyResolver = new UniversalAssemblyResolver(assemblyPath, false, module.Metadata.DetectTargetFrameworkId());
		foreach (var path in (ReferencePaths ?? System.Array.Empty<string>()))
		{
			assemblyResolver.AddSearchDirectory(path);
		}

		this.godotProjectDecompiler = new GodotProjectDecompiler(decompilerSettings, assemblyResolver, ProjectFileWriterGodotStyle.Create(), assemblyResolver, null, this.originalProjectFiles);
		var typesToDecompile = godotProjectDecompiler.GetTypesToDecompile(module);
		this.fileMap = GodotStuff.CreateFileMap(module, typesToDecompile, this.originalProjectFiles, true);
	}

	public int DecompileModule(string outputCSProjectPath, string[]? excludeFiles = null)
	{
		try{
			outputCSProjectPath = Path.GetFullPath(outputCSProjectPath);
			var targetDirectory = Path.GetDirectoryName(outputCSProjectPath);
			if (string.IsNullOrEmpty(targetDirectory))
			{
				Console.Error.WriteLine("Error: Output path is invalid.");
				return -1;
			}
			GodotStuff.EnsureDir(targetDirectory);

			var typesToExclude = excludeFiles?.Select(file => GodotStuff.TrimPrefix(file, "res://")).Where(fileMap.ContainsKey).Select(file => fileMap[file]).ToHashSet() ?? [];

			using (var projectFileWriter = new StreamWriter(File.OpenWrite(outputCSProjectPath))) {
				godotProjectDecompiler.DecompileGodotProject(module, targetDirectory, projectFileWriter, typesToExclude, fileMap.ToDictionary(pair => pair.Value, pair => pair.Key));
			}
		}
		catch (Exception e)
		{
			Console.Error.WriteLine($"Decompilation failed: {e.Message}");
			return -1;

		}
		return 0;
	}
	public const string error_message = "// ERROR: Could not find file '{0}' in assembly '{1}.dll'.";

	public string DecompileIndividualFile(string file)
	{
		var path = GodotStuff.TrimPrefix(file, "res://");
		if (!string.IsNullOrEmpty(path) && fileMap.TryGetValue(path, out var type)){
			var decompiler = godotProjectDecompiler.CreateDecompilerWithPartials(module, [type]);
			return decompiler.DecompileTypesAsString([type]);
		}
		return string.Format(error_message, file, module.Name) + (
			originalProjectFiles.Contains(file) ? "\n// The associated class(es) may have not been compiled into the assembly." : "\n// The file is not present in the original project."
		);
	}

	public int GetNumberOfFilesNotPresentInFileMap()
	{
		return this.originalProjectFiles.Count(file => !fileMap.ContainsKey(file));
	}

	public string[] GetFilesNotPresentInFileMap()
	{
		return this.originalProjectFiles.Where(file => !fileMap.ContainsKey(file)).ToArray();
	}


}
