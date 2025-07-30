

using System.Reflection.Metadata;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.CSharp;
using ICSharpCode.Decompiler.CSharp.OutputVisitor;
using ICSharpCode.Decompiler.Metadata;

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
			var decompiler = new CSharpDecompiler(module, assemblyResolver, decompilerSettings);
			var syntaxTree = decompiler.DecompileTypes([type]);
			using var w = new StringWriter();
			syntaxTree.AcceptVisitor(new CSharpOutputVisitor(w, decompilerSettings.CSharpFormattingOptions));
			return w.GetStringBuilder().ToString();
		}
		return "<ERROR: Could not find file " + file + " in assembly " + module.Name + ">";
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
