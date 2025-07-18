using System.Runtime.InteropServices;
using System.Text;

namespace GodotMonoDecomp.NativeLibrary;

static public class Lib
{
    // nativeAOT function; oneshot, intended to be used by a CLI

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


	[UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_FreeObjectHandle")]
	public static void FreeObjectHandle(IntPtr v)
	{
		GCHandle h = GCHandle.FromIntPtr(v);
		h.Free();
	}

    // wrapper methods for GodotModuleDecompiler; constructor, destructor, all the public methods
	[UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_CreateGodotModuleDecompiler")]
	public static IntPtr AOTCreateGodotModuleDecompiler(
		IntPtr assemblyPath,
		IntPtr originalProjectFiles,
		int originalProjectFilesCount,
		IntPtr referencePaths,
		int referencePathsCount
	)
	{
		string assemblyFileNameStr = Marshal.PtrToStringAnsi(assemblyPath) ?? string.Empty;
		string[] originalProjectFilesStrs = new string[originalProjectFilesCount];
		for (int i = 0; i < originalProjectFilesCount; i++)
		{
			IntPtr pathPtr = Marshal.ReadIntPtr(originalProjectFiles, i * IntPtr.Size);
			originalProjectFilesStrs[i] = Marshal.PtrToStringAnsi(pathPtr) ?? string.Empty;
		}
		string[] referencePathsStrs = referencePathsCount == 0
			? Array.Empty<string>()
			: new string[referencePathsCount];
		for (int i = 0; i < referencePathsCount; i++)
		{
			IntPtr pathPtr = Marshal.ReadIntPtr(referencePaths, i * IntPtr.Size);
			referencePathsStrs[i] = Marshal.PtrToStringAnsi(pathPtr) ?? string.Empty;
		}
		var decompiler = new GodotModuleDecompiler(assemblyFileNameStr, originalProjectFilesStrs, referencePathsStrs);
		var handle = GCHandle.Alloc(decompiler);
		return GCHandle.ToIntPtr(handle);
	}

	[UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_DecompileModule")]
	public static int AOTDecompileModule(
		IntPtr decompilerHandle,
		IntPtr outputCSProjectPath
	)
	{
		var decompiler = GCHandle.FromIntPtr(decompilerHandle).Target as GodotModuleDecompiler;
		if (decompiler == null)
		{
			return -1;
		}
		var outputCSProjectPathStr = Marshal.PtrToStringAnsi(outputCSProjectPath) ?? string.Empty;
		return decompiler.DecompileModule(outputCSProjectPathStr);
	}

	[UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_DecompileIndividualFile")]
	public static IntPtr AOTDecompileIndividualFile(
		IntPtr decompilerHandle,
		IntPtr file
	)
	{
		var decompiler = GCHandle.FromIntPtr(decompilerHandle).Target as GodotModuleDecompiler;
		if (decompiler == null)
		{
			return IntPtr.Zero;
		}
		var fileStr = Marshal.PtrToStringAnsi(file) ?? string.Empty;
		var code = decompiler.DecompileIndividualFile(fileStr);
		return Marshal.StringToHGlobalAnsi(code);
	}

	[UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_GetNumberOfFilesNotPresentInFileMap")]
	public static int AOTGetNumberOfFilesNotPresentInFileMap(
		IntPtr decompilerHandle
	)
	{
		var decompiler = GCHandle.FromIntPtr(decompilerHandle).Target as GodotModuleDecompiler;
		if (decompiler == null)
		{
			return -1;
		}
		return decompiler.GetNumberOfFilesNotPresentInFileMap();
	}

	[UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_GetFilesNotPresentInFileMap")]
	public static IntPtr AOTGetFilesNotPresentInFileMap(
		IntPtr decompilerHandle
	)
	{
		var decompiler = GCHandle.FromIntPtr(decompilerHandle).Target as GodotModuleDecompiler;
		if (decompiler == null)
		{
			return IntPtr.Zero;
		}
		var files = decompiler.GetFilesNotPresentInFileMap();
		var arrayPtr = Marshal.AllocHGlobal(files.Length * IntPtr.Size);
		for (int i = 0; i < files.Length; i++)
		{
			Marshal.WriteIntPtr(arrayPtr + i * IntPtr.Size, Marshal.StringToHGlobalAnsi(files[i]));
		}
		return arrayPtr;
	}

	[UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_FreeArray")]
	public static void FreeArray(IntPtr v, int length)
	{
		for (int i = 0; i < length; i++)
		{
			Marshal.FreeHGlobal(Marshal.ReadIntPtr(v, i * IntPtr.Size));
		}
		Marshal.FreeHGlobal(v);
	}

	[UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_FreeString")]
	public static void FreeString(IntPtr v)
	{
		Marshal.FreeHGlobal(v);
	}
}
