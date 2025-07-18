// See https://aka.ms/new-console-template for more information

using System.Runtime.InteropServices;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.CSharp.ProjectDecompiler;
using ICSharpCode.Decompiler.Metadata;

namespace GodotMonoDecomp
{
    public static class Lib
    {
        public static int DecompileProject(string assemblyPath, string outputCSProjectPath, string projectPath, string[]? ReferencePaths = null)
        {
            try
            {
                DecompilerSettings settings = new DecompilerSettings();
                var module = new PEFile(assemblyPath);
                var resolver =
                    new UniversalAssemblyResolver(assemblyPath, false, module.Metadata.DetectTargetFrameworkId());
                foreach (var path in (ReferencePaths ?? System.Array.Empty<string>()))
                {
                    resolver.AddSearchDirectory(path);
                }
                var files = GodotStuff.ListCSharpFiles(projectPath, false);
                var decompiler = new GodotProjectDecompiler(settings, resolver, null, resolver, null, files);
                GodotStuff.EnsureDir(Path.GetDirectoryName(outputCSProjectPath));

                using (var projectFileWriter = new StreamWriter(File.OpenWrite(outputCSProjectPath)))
                    decompiler.DecompileProject(module, Path.GetDirectoryName(outputCSProjectPath), projectFileWriter);
            }
            catch (Exception e)
            {
                Console.WriteLine("Decompilation failed: ", e.Message);
                return -1;
            }

            return 0;
        }
    }

}
