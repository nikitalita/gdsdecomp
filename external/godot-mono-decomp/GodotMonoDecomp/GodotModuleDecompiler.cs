using System.Reflection.Metadata;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.CSharp;
using ICSharpCode.Decompiler.CSharp.ProjectDecompiler;
using ICSharpCode.Decompiler.DebugInfo;
using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.Decompiler.Solution;
using ICSharpCode.ILSpyX.PdbProvider;

namespace GodotMonoDecomp;

public class GodotModule
{
	public readonly PEFile Module;
	public readonly DotNetCoreDepInfo? depInfo;
	public readonly LanguageVersion languageVersion;
	public readonly IDebugInfoProvider? debugInfoProvider;


// Target 	Version 	C# language version default
// .NET 			10.x 	C# 14
// .NET 			9.x 	C# 13
// .NET 			8.x 	C# 12
// .NET 			7.x 	C# 11
// .NET 			6.x 	C# 10
// .NET 			5.x 	C# 9.0
// .NET Core 		3.x 	C# 8.0
// .NET Core 		2.x 	C# 7.3
// .NET Standard 	2.1 	C# 8.0
// .NET Standard 	2.0 	C# 7.3
// .NET Standard 	1.x 	C# 7.3
// .NET Framework 	all 	C# 7.3

	public static LanguageVersion GetDefaultCSharpLanguageLevel(MetadataFile module){

		// determine the dotnet version
		var dotnetVersion = TargetServices.DetectTargetFramework(module);
		if (dotnetVersion == null)
		{
			return LanguageVersion.CSharp7_3;
		}
		int verMajor = dotnetVersion.VersionNumber / 100;
		int verMinor = dotnetVersion.VersionNumber % 100 / 10;

		// .NET Framework
		// ".NETFramework" applies to all the newer ".NET" as well as the legacy ".NET Framework", so check that the version is less than 5
		if (dotnetVersion.Identifier == ".NETFramework" && verMajor < 5){
			return LanguageVersion.CSharp7_3;
		}

		// .NET Core
		// ".NETCoreApp" applies to all the newer ".NET" if the module is an app, so check if the version number is less than 4
		if ((dotnetVersion.Identifier == ".NETCoreApp" || dotnetVersion.Identifier == ".NETCore") && verMajor < 4){
			if (verMajor == 3) {
				return LanguageVersion.CSharp8_0;
			}
			return LanguageVersion.CSharp7_3;
		}

		// .NET Standard
		if (dotnetVersion.Identifier == ".NETStandard" && verMajor <= 2){
			if (verMinor >= 1){
				return LanguageVersion.CSharp8_0;
			}
			return LanguageVersion.CSharp7_3;
		}
		// .NET
		switch (verMajor){
			case 10:
			// not yet supported
			// return LanguageVersion.CSharp14_0;
			case 9:
			// not yet supported
			// return LanguageVersion.CSharp13_0;
			case 8:
				return LanguageVersion.CSharp12_0;
			case 7:
				return LanguageVersion.CSharp11_0;
			case 6:
				return LanguageVersion.CSharp10_0;
			case 5:
				return LanguageVersion.CSharp9_0;
			default:
			{
				if (verMajor > 8){
					return LanguageVersion.CSharp12_0;
				}
			}
				return LanguageVersion.CSharp7_3;
		}
	}

	public GodotModule(PEFile module, DotNetCoreDepInfo? depInfo)
	{
		Module = module ?? throw new ArgumentNullException(nameof(module));
		this.depInfo = depInfo;
		debugInfoProvider = DebugInfoUtils.LoadSymbols(module);
		languageVersion = GetDefaultCSharpLanguageLevel(module);
	}

	public MetadataReader Metadata => Module.Metadata;
	public string Name => Module.Name;
}

public class GodotModuleDecompiler
{
	public readonly GodotModule MainModule;
	public readonly List<GodotModule> AdditionalModules;
	public readonly UniversalAssemblyResolver AssemblyResolver;
	public readonly GodotMonoDecompSettings Settings;
	public readonly Dictionary<string, TypeDefinitionHandle> fileMap;
	public readonly List<string> originalProjectFiles;
	public readonly Version godotVersion;
	public readonly Dictionary<string, GodotScriptMetadata>? godot3xMetadata;


	public GodotModuleDecompiler(string assemblyPath, string[]? originalProjectFiles, string[]? ReferencePaths = null,
		GodotMonoDecompSettings? settings = default(GodotMonoDecompSettings))
	{
		AdditionalModules = [];
		this.originalProjectFiles = [.. (originalProjectFiles ?? []).Where(file => !string.IsNullOrEmpty(file)).Select(file => GodotStuff.TrimPrefix(file, "res://")).OrderBy(file => file, StringComparer.OrdinalIgnoreCase)];
		var mod = new PEFile(assemblyPath);
		var mainDepInfo = DotNetCoreDepInfo.LoadDepInfoFromFile(DotNetCoreDepInfo.GetDepPath(assemblyPath), mod.Name);
		MainModule = new GodotModule(mod, mainDepInfo);
		AssemblyResolver = new UniversalAssemblyResolver(assemblyPath, false, MainModule.Metadata.DetectTargetFrameworkId());
		foreach (var path in (ReferencePaths ?? System.Array.Empty<string>()))
		{
			AssemblyResolver.AddSearchDirectory(path);
		}

		Settings = settings ?? new GodotMonoDecompSettings();

		if (Settings.CreateAdditionalProjectsForProjectReferences && mainDepInfo != null)
		{
			foreach (var reference in MainModule.Module.AssemblyReferences)
			{
				var dep = mainDepInfo.deps.ToList().Find(dep => dep.Name == reference.Name);
				if (dep is { Type: "project" })
				{
					var depModule = AssemblyResolver.Resolve(reference);
					if (depModule == null)
					{
						Console.Error.WriteLine($"Warning: Could not resolve project reference '{dep.Name}' for assembly '{MainModule.Name}'.");
						continue;
					}
					if (depModule is PEFile module)
					{
						AdditionalModules.Add(new GodotModule(module, dep));
					}
				}
			}
		}


		godotVersion = Settings.GodotVersionOverride ?? GodotStuff.GetGodotVersion(MainModule.Module) ?? new Version(0, 0, 0, 0);
		if (godotVersion.Major <= 3)
		{
			// check for "script_metadata.{release,debug}" files
			var godot3xMetadataFile = GodotScriptMetadataLoader.FindGodotScriptMetadataFile(assemblyPath);
			if (godot3xMetadataFile != null && File.Exists(godot3xMetadataFile))
			{
				godot3xMetadata = GodotScriptMetadataLoader.LoadFromFile(godot3xMetadataFile);
			}
		}

		var godotProjectDecompiler = new GodotProjectDecompiler(Settings, AssemblyResolver, AssemblyResolver, MainModule.debugInfoProvider);
		var typesToDecompile = godotProjectDecompiler.GetTypesToDecompile(MainModule.Module).ToHashSet();
		fileMap = GodotStuff.CreateFileMap(MainModule.Module, typesToDecompile, this.originalProjectFiles, godot3xMetadata, true);
		foreach (var module in AdditionalModules)
		{
			// TODO: make CreateFileMap() work with multiple modules
			typesToDecompile = godotProjectDecompiler.GetTypesToDecompile(module.Module).ToHashSet();

			var addtlFileMap = GodotStuff.CreateFileMap(module.Module, typesToDecompile, this.originalProjectFiles, godot3xMetadata, true);
			foreach (var pair in addtlFileMap)
			{
				// TODO: right now we're force appending module name to the file path
				string path = module.Name + "/" + pair.Key;
				if (!fileMap.ContainsKey(path))
				{
					fileMap.Add(path, pair.Value);
				}
			}
		}

	}

	// Dictionary<TypeDefinitionHandle, string> MakeHandleToFileMap(MetadataFile module)
	// {
	// 	Version godotVersion = GodotStuff.GetGodotVersion(module) ?? new Version(0, 0, 0, 0);
	// 	Dictionary<string, GodotScriptMetadata>? metadata = null;
	// 	if (godotVersion.Major <= 3)
	// 	{
	// 		metadata = GodotScriptMetadataLoader.LoadFromFile(GodotScriptMetadataLoader.FindGodotScriptMetadataFile(module.FileName));
	// 	}
	// 	return GodotStuff.CreateFileMap(module, GetTypesToDecompile(module), FilesInOriginal, metadata, Settings.UseNestedDirectoriesForNamespaces).ToDictionary(
	// 		pair => pair.Value,
	// 		pair => pair.Key,
	// 		null);
	// }

	GodotProjectDecompiler CreateProjectDecompiler(GodotModule module, IProgress<DecompilationProgress>? progress_reporter = null)
	{
		var moduleSettings = Settings.Clone();
		moduleSettings.SetLanguageVersion(module.languageVersion);
		var decompiler = new GodotProjectDecompiler(moduleSettings, AssemblyResolver, AssemblyResolver, module.debugInfoProvider);
		decompiler.ProgressIndicator = progress_reporter;
		return decompiler;
	}

	void removeIfExists(string path)
	{
		if (File.Exists(path))
		{
			try
			{
				File.Delete(path);
			}
			catch (Exception e)
			{
				Console.Error.WriteLine($"Error: Failed to delete existing file {path}: {e.Message}");
			}
		}

	}

	public int DecompileModule(string outputCSProjectPath, string[]? excludeFiles = null, IProgress<DecompilationProgress>? progress_reporter = null, CancellationToken token = default(CancellationToken))
	{
		try
		{
			outputCSProjectPath = Path.GetFullPath(outputCSProjectPath);
			var targetDirectory = Path.GetDirectoryName(outputCSProjectPath);
			if (string.IsNullOrEmpty(targetDirectory))
			{
				Console.Error.WriteLine("Error: Output path is invalid.");
				return -1;
			}
			GodotStuff.EnsureDir(targetDirectory);

			var typesToExclude = excludeFiles?.Select(file => GodotStuff.TrimPrefix(file, "res://")).Where(fileMap.ContainsKey).Select(file => fileMap[file]).ToHashSet() ?? [];
			ProjectItem decompileFile(GodotModule module, string csprojPath)
			{
				var godotProjectDecompiler = CreateProjectDecompiler(module, progress_reporter);

				removeIfExists(csprojPath);

				ProjectId projectId;

				using (var projectFileWriter = new StreamWriter(File.OpenWrite(csprojPath)))
				{
					projectId = godotProjectDecompiler.DecompileGodotProject(
						module.Module, targetDirectory, projectFileWriter, typesToExclude, fileMap.ToDictionary(pair => pair.Value, pair => pair.Key), module.depInfo, token);
				}

				ProjectItem item = new ProjectItem(csprojPath, projectId.PlatformName, projectId.Guid, projectId.TypeGuid);
				return item;

			}

			var projectIDs = new List<ProjectItem>();
			projectIDs.Add(decompileFile(MainModule, outputCSProjectPath));
			foreach (var module in AdditionalModules)
			{
				var csProjPath = Path.Combine(targetDirectory, module.Name + ".csproj");
				projectIDs.Add(decompileFile(module, csProjPath));
			}
			var solutionPath = Path.ChangeExtension(outputCSProjectPath, ".sln");
			removeIfExists(solutionPath);

			GodotMonoDecomp.SolutionCreator.WriteSolutionFile(solutionPath, projectIDs);
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
		if (!string.IsNullOrEmpty(path) && fileMap.TryGetValue(path, out var type))
		{
			GodotModule? module = MainModule;
			var projectDecompiler = CreateProjectDecompiler(module);
			if (!projectDecompiler.GetTypesToDecompile(module.Module).Contains(type))
			{
				module = null;
				foreach (var m in AdditionalModules)
				{
					if (projectDecompiler.GetTypesToDecompile(module.Module).Contains(type))
					{
						module = m;
						break;
					}
				}
			}
			if (module != null)
			{
				var decompiler = projectDecompiler.CreateDecompilerWithPartials(module.Module, [type]);
				return decompiler.DecompileTypesAsString([type]);
			} else {
				return string.Format(error_message, file, MainModule.Name) + "\n// We screwed up somewhere.";
			}
		}
		return string.Format(error_message, file, MainModule.Name) + (
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
