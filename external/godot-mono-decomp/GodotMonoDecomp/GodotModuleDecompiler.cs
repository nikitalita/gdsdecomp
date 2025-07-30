

using System.Reflection.Metadata;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.Metadata;

namespace GodotMonoDecomp;

public class GodotModuleDecompiler
{
	public readonly PEFile module;
	public readonly GodotProjectDecompiler godotProjectDecompiler;
	public readonly Dictionary<string, TypeDefinitionHandle> fileMap;
	public readonly IEnumerable<string> originalProjectFiles;

	public string outputCSProjectPath;

	public GodotModuleDecompiler(string assemblyPath, string outputCSProjectPath, string[] originalProjectFiles, string[]? ReferencePaths = null){
		this.outputCSProjectPath = outputCSProjectPath;
		this.originalProjectFiles = originalProjectFiles;
		this.module = new PEFile(assemblyPath);
		var resolver = new UniversalAssemblyResolver(assemblyPath, false, module.Metadata.DetectTargetFrameworkId());
		foreach (var path in (ReferencePaths ?? System.Array.Empty<string>()))
		{
			resolver.AddSearchDirectory(path);
		}

		this.godotProjectDecompiler = new GodotProjectDecompiler(new DecompilerSettings(), resolver, ProjectFileWriterGodotStyle.Create(), resolver, null, originalProjectFiles);
		var typesToDecompile = godotProjectDecompiler.GetTypesToDecompile(module);
		this.fileMap = GodotStuff.CreateFileMap(module, typesToDecompile, this.originalProjectFiles, true);
	}

	public void DecompileModule()
	{
		var targetDirectory = Path.GetDirectoryName(outputCSProjectPath);
		GodotStuff.EnsureDir(targetDirectory);

		using (var projectFileWriter = new StreamWriter(File.OpenWrite(outputCSProjectPath)))
			godotProjectDecompiler.DecompileProject(module, targetDirectory, projectFileWriter);

	}


}
