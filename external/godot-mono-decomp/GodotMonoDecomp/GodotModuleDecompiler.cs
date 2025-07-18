

using System.Reflection.Metadata;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.CSharp;
using ICSharpCode.Decompiler.CSharp.OutputVisitor;
using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.Decompiler.TypeSystem;

namespace GodotMonoDecomp;

public class GodotModuleDecompiler
{
	public readonly PEFile module;

	public readonly UniversalAssemblyResolver assemblyResolver;
	public readonly GodotProjectDecompiler godotProjectDecompiler;
	public readonly Dictionary<string, TypeDefinitionHandle> fileMap;
	public readonly List<string> originalProjectFiles;


	public GodotModuleDecompiler(string assemblyPath, string[] originalProjectFiles, string[]? ReferencePaths = null){
		var decompilerSettings = new DecompilerSettings();
		decompilerSettings.UseNestedDirectoriesForNamespaces = true;
		this.originalProjectFiles = originalProjectFiles.Where(file => !string.IsNullOrEmpty(file)).Select(file => GodotStuff.TrimPrefix(file, "res://")).ToList();
		this.module = new PEFile(assemblyPath);
		this.assemblyResolver = new UniversalAssemblyResolver(assemblyPath, false, module.Metadata.DetectTargetFrameworkId());
		foreach (var path in (ReferencePaths ?? System.Array.Empty<string>()))
		{
			this.assemblyResolver.AddSearchDirectory(path);
		}

		this.godotProjectDecompiler = new GodotProjectDecompiler(decompilerSettings, this.assemblyResolver, ProjectFileWriterGodotStyle.Create(), this.assemblyResolver, null, this.originalProjectFiles);
		var typesToDecompile = godotProjectDecompiler.GetTypesToDecompile(module);
		this.fileMap = GodotStuff.CreateFileMap(module, typesToDecompile, this.originalProjectFiles, true);
	}

	public int DecompileModule(string outputCSProjectPath)
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

			using (var projectFileWriter = new StreamWriter(File.OpenWrite(outputCSProjectPath)))
				godotProjectDecompiler.DecompileProject(module, targetDirectory, projectFileWriter);
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
			DecompilerTypeSystem ts = new DecompilerTypeSystem(module, assemblyResolver, godotProjectDecompiler.Settings);
			var partialTypes = GodotStuff.GetPartialGodotTypes(module, [type], ts);
			var decompiler = godotProjectDecompiler.CreateDecompiler(ts);
			foreach (var partialType in partialTypes){
				decompiler.AddPartialTypeDefinition(partialType);
			}
			var syntaxTree = decompiler.DecompileTypes([type]);
			using var w = new StringWriter();
			syntaxTree.AcceptVisitor(new CSharpOutputVisitor(w, godotProjectDecompiler.Settings.CSharpFormattingOptions));
			return w.ToString();
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
