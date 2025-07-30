using System.Runtime.InteropServices;
using System.Text;

namespace GodotMonoDecomp.NativeLibrary;

static public class Lib
{
	static string[]? GetStringArray(IntPtr ptr, int count)
	{
		if (count == 0 || ptr == IntPtr.Zero) return null;
		string[] strs = new string[count];
		for (int i = 0; i < count; i++)
		{
			IntPtr pathPtr = Marshal.ReadIntPtr(ptr, i * IntPtr.Size);
			strs[i] = Marshal.PtrToStringAnsi(pathPtr) ?? string.Empty;
		}
		return strs;
	}

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
        string[]? referencePathsStrs = GetStringArray(AssemblyReferenceDirs, referencePathsCount);
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
		var originalProjectFilesStrs = GetStringArray(originalProjectFiles, originalProjectFilesCount);
		var referencePathsStrs = GetStringArray(referencePaths, referencePathsCount);
		try {
			var decompiler = new GodotModuleDecompiler(assemblyFileNameStr, originalProjectFilesStrs, referencePathsStrs);
			var handle = GCHandle.Alloc(decompiler);
			return GCHandle.ToIntPtr(handle);
		}
		catch (Exception e)
		{
			Console.Error.WriteLine("Failed to create GodotModuleDecompiler: " + e.Message);
			return IntPtr.Zero;
		}
	}

	[UnmanagedCallersOnly(EntryPoint = "GodotMonoDecomp_DecompileModule")]
	public static int AOTDecompileModule(
		IntPtr decompilerHandle,
		IntPtr outputCSProjectPath,
		IntPtr excludeFiles,
		int excludeFilesCount
	)
	{
		var decompiler = GCHandle.FromIntPtr(decompilerHandle).Target as GodotModuleDecompiler;
		if (decompiler == null)
		{
			return -1;
		}
		var outputCSProjectPathStr = Marshal.PtrToStringAnsi(outputCSProjectPath) ?? string.Empty;
		var excludeFilesStrs = GetStringArray(excludeFiles, excludeFilesCount);
		return decompiler.DecompileModule(outputCSProjectPathStr, excludeFilesStrs);
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
