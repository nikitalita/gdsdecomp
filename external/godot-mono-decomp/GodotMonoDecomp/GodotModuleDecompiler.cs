using System.Reflection.Metadata;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.DebugInfo;
using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.ILSpyX.PdbProvider;

namespace GodotMonoDecomp;

public class GodotModuleDecompiler
{
	public readonly PEFile module;
	public readonly GodotProjectDecompiler godotProjectDecompiler;
	public readonly Dictionary<string, TypeDefinitionHandle> fileMap;
	public readonly List<string> originalProjectFiles;
	public readonly Version godotVersion;
	public readonly Dictionary<string, GodotScriptMetadata>? godot3xMetadata;
	public readonly DotNetCoreDepInfo? depInfo;


	public GodotModuleDecompiler(string assemblyPath, string[]? originalProjectFiles, string[]? ReferencePaths = null) {
		var decompilerSettings = new DecompilerSettings();
		decompilerSettings.UseNestedDirectoriesForNamespaces = true;
		this.originalProjectFiles = [.. (originalProjectFiles ?? []).Where(file => !string.IsNullOrEmpty(file)).Select(file => GodotStuff.TrimPrefix(file, "res://")).OrderBy(file => file, StringComparer.OrdinalIgnoreCase)];
		module = new PEFile(assemblyPath);
		depInfo = DotNetCoreDepInfo.LoadDepInfoFromFile(DotNetCoreDepInfo.GetDepPath(assemblyPath), module.Name);
		IDebugInfoProvider? debugInfoProvider = DebugInfoUtils.LoadSymbols(this.module);
		var assemblyResolver = new UniversalAssemblyResolver(assemblyPath, false, module.Metadata.DetectTargetFrameworkId());
		foreach (var path in (ReferencePaths ?? System.Array.Empty<string>()))
		{
			assemblyResolver.AddSearchDirectory(path);
		}

		godotVersion = GodotStuff.GetGodotVersion(module) ?? new Version(0, 0, 0, 0);
		if (godotVersion.Major <= 3)
		{
			// check for "script_metadata.{release,debug}" files
			var godot3xMetadataFile = GodotScriptMetadataLoader.FindGodotScriptMetadataFile(assemblyPath);
			if (godot3xMetadataFile != null && File.Exists(godot3xMetadataFile))
			{
				godot3xMetadata = GodotScriptMetadataLoader.LoadFromFile(godot3xMetadataFile);
			}
		}

		godotProjectDecompiler = new GodotProjectDecompiler(decompilerSettings, assemblyResolver, assemblyResolver, debugInfoProvider, this.originalProjectFiles);
		var typesToDecompile = godotProjectDecompiler.GetTypesToDecompile(module);
		fileMap = GodotStuff.CreateFileMap(module, typesToDecompile, this.originalProjectFiles, godot3xMetadata, true);
	}

	public int DecompileModule(string outputCSProjectPath, string[]? excludeFiles = null, IProgress<DecompilationProgress>? progress_reporter = null, CancellationToken token = default(CancellationToken))
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

			godotProjectDecompiler.ProgressIndicator = progress_reporter;
			if (File.Exists(outputCSProjectPath))
			{
				try {
					File.Delete(outputCSProjectPath);
				}
				catch (Exception e)
				{
					Console.Error.WriteLine($"Error: Failed to delete existing project file: {e.Message}");
				}
			}
			using (var projectFileWriter = new StreamWriter(File.OpenWrite(outputCSProjectPath))) {
				godotProjectDecompiler.DecompileGodotProject(module, targetDirectory, projectFileWriter, typesToExclude, fileMap.ToDictionary(pair => pair.Value, pair => pair.Key), depInfo, token);
			}
			godotProjectDecompiler.ProgressIndicator = null;
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

	public int GetNumberOfFilesInFileMap()
	{
		return fileMap.Count;
	}

	public string[] GetFilesInFileMap()
	{
		return fileMap.Keys.ToArray();
	}


}
