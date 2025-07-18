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
                var files = GodotStuff.ListCSharpFiles(projectPath, false);
				GodotModuleDecompiler decompiler = new GodotModuleDecompiler(assemblyPath, outputCSProjectPath, [.. files], ReferencePaths);
                decompiler.DecompileModule();
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
