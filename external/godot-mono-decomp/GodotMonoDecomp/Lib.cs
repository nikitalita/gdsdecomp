// See https://aka.ms/new-console-template for more information

using System.Runtime.InteropServices;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.CSharp.ProjectDecompiler;
using ICSharpCode.Decompiler.Metadata;

namespace GodotMonoDecomp
{
    public static class Lib
    {
        public static int DecompileProject(string assemblyPath, string outputCSProjectPath, string projectPath, string[]? ReferencePaths = null, string[]? excludeFiles = null)
        {
            try
            {
                var files = GodotStuff.ListCSharpFiles(projectPath, false);
				GodotModuleDecompiler decompiler = new GodotModuleDecompiler(assemblyPath, [.. files], ReferencePaths);
                return decompiler.DecompileModule(outputCSProjectPath, excludeFiles);
            }
            catch (Exception e)
            {
                Console.WriteLine("Decompilation failed: " + e.Message);
                return -1;
            }
        }
    }

}
