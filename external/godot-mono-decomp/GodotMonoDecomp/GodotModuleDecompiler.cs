

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

	public DecompilerSettings decompilerSettings;

	public GodotModuleDecompiler(string assemblyPath, string[] originalProjectFiles, string[]? ReferencePaths = null){
		this.decompilerSettings = new DecompilerSettings();
		this.originalProjectFiles = originalProjectFiles.Select(file => GodotStuff.TrimPrefix(file, "res://")).ToList();
		this.module = new PEFile(assemblyPath);
		this.assemblyResolver = new UniversalAssemblyResolver(assemblyPath, false, module.Metadata.DetectTargetFrameworkId());
		foreach (var path in (ReferencePaths ?? System.Array.Empty<string>()))
		{
			this.assemblyResolver.AddSearchDirectory(path);
		}

		this.godotProjectDecompiler = new GodotProjectDecompiler(this.decompilerSettings, this.assemblyResolver, ProjectFileWriterGodotStyle.Create(), this.assemblyResolver, null, this.originalProjectFiles);
		var typesToDecompile = godotProjectDecompiler.GetTypesToDecompile(module);
		this.fileMap = GodotStuff.CreateFileMap(module, typesToDecompile, this.originalProjectFiles, true);
	}

	public int DecompileModule(string outputCSProjectPath)
	{
		try{
			var targetDirectory = Path.GetDirectoryName(outputCSProjectPath);
			GodotStuff.EnsureDir(targetDirectory);

			using (var projectFileWriter = new StreamWriter(File.OpenWrite(outputCSProjectPath)))
				godotProjectDecompiler.DecompileProject(module, targetDirectory, projectFileWriter);
		}
		catch (Exception e)
		{
			Console.WriteLine("Decompilation failed: " + e.Message);
			return -1;

		}
		return 0;
	}

	public string DecompileIndividualFile(string file)
	{
		var path = GodotStuff.TrimPrefix(file, "res://");
		if (fileMap.TryGetValue(path, out var type)){
			DecompilerTypeSystem ts = new DecompilerTypeSystem(module, assemblyResolver, decompilerSettings);
			var partialTypes = GodotStuff.GetPartialGodotTypes(module, [type], ts);
			var decompiler = godotProjectDecompiler.CreateDecompiler(ts);
			foreach (var partialType in partialTypes){
				decompiler.AddPartialTypeDefinition(partialType);
			}
			var syntaxTree = decompiler.DecompileTypes([type]);
			GodotStuff.RemoveScriptPathAttribute(syntaxTree.Children);
			using var w = new StringWriter();
			syntaxTree.AcceptVisitor(new CSharpOutputVisitor(w, decompilerSettings.CSharpFormattingOptions));
			return w.GetStringBuilder().ToString();
		}
		return "// ERROR: Could not find file " + file + " in assembly " + module.Name + ".dll.\n// The associated class(es) may have not been compiled into the assembly.";
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
