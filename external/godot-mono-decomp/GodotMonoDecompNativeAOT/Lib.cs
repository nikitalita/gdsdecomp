using System.Runtime.InteropServices;

namespace GodotMonoDecomp.NativeLibrary;

static public class Lib
{
    // nativeAOT function
        
    [UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_DecompileProject")]
    public static int AOTDecompileProject(  
        IntPtr assemblyPath,
        IntPtr outputCSProjectPath,
        IntPtr projectPath,
        IntPtr AssemblyReferenceDirs,
        int referencePathsCount
    )
    {
        string assemblyFileNameStr = Marshal.PtrToStringAnsi(assemblyPath) ?? string.Empty;
        string outputPathStr = Marshal.PtrToStringAnsi(outputCSProjectPath) ?? string.Empty;
        string  projectFileNameStr = Marshal.PtrToStringAnsi(projectPath) ?? string.Empty;  
        string[] referencePathsStrs =  referencePathsCount == 0
            ? Array.Empty<string>()
            : new string[referencePathsCount];
        for (int i = 0; i < referencePathsCount; i++)
        {
            IntPtr pathPtr = Marshal.ReadIntPtr(AssemblyReferenceDirs, i * IntPtr.Size);
            referencePathsStrs[i] = Marshal.PtrToStringAnsi(pathPtr) ?? string.Empty;
        }
        return GodotMonoDecomp.Lib.DecompileProject(assemblyFileNameStr, outputPathStr,projectFileNameStr, referencePathsStrs);
    }

}
