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
	public Dictionary<string, TypeDefinitionHandle> fileMap;




	public GodotModule(PEFile module, DotNetCoreDepInfo? depInfo)
	{
		Module = module ?? throw new ArgumentNullException(nameof(module));
		this.depInfo = depInfo;
		debugInfoProvider = DebugInfoUtils.LoadSymbols(module);
		languageVersion = Common.GetDefaultCSharpLanguageLevel(module);
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
	public readonly List<string> originalProjectFiles;
	public readonly Version godotVersion;
	public readonly Dictionary<string, GodotScriptMetadata>? godot3xMetadata;


	public GodotModuleDecompiler(string assemblyPath, string[]? originalProjectFiles, string[]? ReferencePaths = null,
		GodotMonoDecompSettings? settings = default(GodotMonoDecompSettings))
	{
		AdditionalModules = [];
		this.originalProjectFiles = [.. (originalProjectFiles ?? []).Where(file => !string.IsNullOrEmpty(file)).Select(file => Common.TrimPrefix(file, "res://")).OrderBy(file => file, StringComparer.OrdinalIgnoreCase)];
		var mod = new PEFile(assemblyPath);
		var mainDepInfo = DotNetCoreDepInfo.LoadDepInfoFromFile(DotNetCoreDepInfo.GetDepPath(assemblyPath), mod.Name);
		MainModule = new GodotModule(mod, mainDepInfo);
		AssemblyResolver = new UniversalAssemblyResolver(assemblyPath, false, MainModule.Metadata.DetectTargetFrameworkId());
		foreach (var path in (ReferencePaths ?? System.Array.Empty<string>()))
		{
			AssemblyResolver.AddSearchDirectory(path);
		}

		Settings = settings ?? new GodotMonoDecompSettings();

		List<string> names = [];
		if (Settings.CreateAdditionalProjectsForProjectReferences && mainDepInfo != null)
		{
			foreach (var dep in mainDepInfo.deps.Where(d => d is {Type : "project"}).OrderBy(d => d.Name, StringComparer.OrdinalIgnoreCase))
			{
				if (names.Contains(dep.Name))
				{
					Console.Error.WriteLine($"Warning: Duplicate project reference '{dep.Name}' found in assembly '{MainModule.Name}'.");
					continue;
				}

				var assemblynameRef = dep.AssemblyRef;
				var supposedPath = Path.Combine(Path.GetDirectoryName(assemblyPath) ?? "", assemblynameRef.Name + ".dll");
				MetadataFile reference = File.Exists(supposedPath) ? new PEFile(supposedPath) : null;

				if (reference == null)
				{
					reference = AssemblyResolver.Resolve(assemblynameRef);
				}
				if (reference == null)
				{
					Console.Error.WriteLine($"Warning: Could not resolve project reference '{dep.Name}' for assembly '{MainModule.Name}'.");
					continue;
				}
				if (reference is PEFile module)
				{
					var fullName = module.FullName;;
					AdditionalModules.Add(new GodotModule(module, dep));
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

		HashSet<string> excludeSubdirs = AdditionalModules.Select(module => module.Name).ToHashSet();

		var typesToDecompile = CreateProjectDecompiler(MainModule).GetTypesToDecompile(MainModule.Module).ToHashSet();
		MainModule.fileMap = GodotStuff.CreateFileMap(MainModule.Module, typesToDecompile, this.originalProjectFiles, godot3xMetadata, excludeSubdirs, true);
		var additionalModuleCount = 0;
		var dupeCount = 0;
		var alreadyExistsCount = 0;
		var fileToModuleMap = MainModule.fileMap.ToDictionary(
			pair => pair.Key,
			pair => MainModule,//.Module.FileName,
			StringComparer.OrdinalIgnoreCase);
		// var moduleFileNameToMouduleMap = new Dictionary<string, GodotModule>(StringComparer.OrdinalIgnoreCase);
		foreach (var module in AdditionalModules)
		{
			// TODO: make CreateFileMap() work with multiple modules
			typesToDecompile = CreateProjectDecompiler(MainModule).GetTypesToDecompile(module.Module).ToHashSet();

			var nfileMap = GodotStuff.CreateFileMap(module.Module, typesToDecompile, this.originalProjectFiles, godot3xMetadata, null, true);
			additionalModuleCount += nfileMap.Count;

			string moduleName = module.Module.FileName;
			module.fileMap = [];

			foreach (var pair in nfileMap.ToList())
			{
				if (module.Name == "ThirdParty")
				{
					var fdsagdas = "";
				}
				// TODO: right now we're force appending module name to the file path
				string path = pair.Key;
				string fixedPath = path;
				if (!path.StartsWith(module.Name + "/", StringComparison.CurrentCultureIgnoreCase))
				{
					fixedPath = module.Name + "/" + pair.Key;
				}

				// only append it if it doesn't already have a module name
				if (!fileToModuleMap.ContainsKey(fixedPath))
				{
					fileToModuleMap.Add(fixedPath, module);
					module.fileMap.Add(fixedPath, pair.Value);
				}
				else
				{
					if (path == fixedPath || originalProjectFiles.Contains(fixedPath) )
					{
						// remove it from the previous module's file map
						var prevmod = fileToModuleMap[fixedPath];
						prevmod.fileMap.Remove(fixedPath);
						module.fileMap.Add(fixedPath, pair.Value);
					} // otherwise don't add it
					alreadyExistsCount++;
					var far = "";
				}
			}

			if (module.fileMap.Count == 0)
			{
				Console.Error.WriteLine($"Warning: Module '{moduleName}' has no files to decompile. It may not be a Godot module or it may not contain any scripts.");
			}
			else
			{
				Console.WriteLine($"Module '{moduleName}' has {module.fileMap.Count} files to decompile.");
			}
		}

	}

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
			Common.EnsureDir(targetDirectory);

			ProjectItem decompileFile(GodotModule module, string csprojPath)
			{
				var godotProjectDecompiler = CreateProjectDecompiler(module, progress_reporter);
				Common.EnsureDir(Path.GetDirectoryName(csprojPath));

				removeIfExists(csprojPath);

				ProjectId projectId;
				var typesToExclude = excludeFiles?.Select(file => Common.TrimPrefix(file, "res://")).Where(module.fileMap.ContainsKey).Select(file => module.fileMap[file]).ToHashSet() ?? [];

				using (var projectFileWriter = new StreamWriter(File.OpenWrite(csprojPath)))
				{
					projectId = godotProjectDecompiler.DecompileGodotProject(
						module.Module, targetDirectory, projectFileWriter, typesToExclude, module.fileMap.ToDictionary(pair => pair.Value, pair => pair.Key), module.depInfo, token);
				}

				ProjectItem item = new ProjectItem(csprojPath, projectId.PlatformName, projectId.Guid, projectId.TypeGuid);
				return item;

			}

			var projectIDs = new List<ProjectItem>();
			projectIDs.Add(decompileFile(MainModule, outputCSProjectPath));
			foreach (var module in AdditionalModules)
			{
				var csProjPath = Path.Combine(targetDirectory, module.Name, module.Name + ".csproj");
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
		var path = Common.TrimPrefix(file, "res://");
		if (!string.IsNullOrEmpty(path))
		{
			GodotModule? module = MainModule;
			TypeDefinitionHandle foundType;

			if (!module.fileMap.TryGetValue(path, out foundType))
			{
				module = null;
				foreach (var m in AdditionalModules)
				{
					if (m.fileMap.TryGetValue(path, out foundType))
					{
						module = m;
						break;
					}
				}
			}

			if (module != null)
			{
				var projectDecompiler = CreateProjectDecompiler(module);

				if (foundType == null)
				{
					return string.Format(error_message, file, MainModule.Name) + "\n// We screwed up somewhere.";
				}
				var decompiler = projectDecompiler.CreateDecompilerWithPartials(module.Module, [foundType]);
				return decompiler.DecompileTypesAsString([foundType]);
			}
		}
		return string.Format(error_message, file, MainModule.Name) + (
			originalProjectFiles.Contains(path) ? "\n// The associated class(es) may have not been compiled into the assembly." : "\n// The file is not present in the original project."
		);
	}


	public bool anyFileMapsContainsFile(string file)
	{
		var path = Common.TrimPrefix(file, "res://");
		if (!string.IsNullOrEmpty(path))
		{
			if (MainModule.fileMap.ContainsKey(path))
			{
				return true;
			}
			foreach (var module in AdditionalModules)
			{
				if (module.fileMap.ContainsKey(path))
				{
					return true;
				}
			}
		}
		return false;
	}

	public int GetNumberOfFilesNotPresentInFileMap()
	{
		return this.originalProjectFiles.Count(file => !anyFileMapsContainsFile(file));
	}

	public string[] GetFilesNotPresentInFileMap()
	{
		return this.originalProjectFiles.Where(file => !anyFileMapsContainsFile(file)).ToArray();
	}

	public int GetNumberOfFilesInFileMap()
	{
		return MainModule.fileMap.Count + AdditionalModules.Sum(module => module.fileMap.Count);
	}

	public string[] GetFilesInFileMap()
	{
		return MainModule.fileMap.Keys
			.Concat(AdditionalModules.SelectMany(module => module.fileMap.Keys))
			.ToArray();
	}


}
