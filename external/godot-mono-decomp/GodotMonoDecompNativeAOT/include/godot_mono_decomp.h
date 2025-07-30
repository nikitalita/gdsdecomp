#ifndef GODOT_MONO_DECOMP_H
#define GODOT_MONO_DECOMP_H

#ifdef __cplusplus
extern "C" {
#endif

// Function declaration for the NativeAOT decompile function
// This matches the UnmanagedCallersOnly function in EntryPoint.cs
int GodotMonoDecomp_DecompileProject(
    const char* assemblyPath,
    const char* outputCSProjectPath,
    const char* projectPath,
    const char** assemblyReferenceDirs,
    int referencePathsCount
);

void* GodotMonoDecomp_CreateGodotModuleDecompiler(
    const char* assemblyPath,
    const char** originalProjectFiles,
    int originalProjectFilesCount,
    const char** referencePaths,
    int referencePathsCount
);

int GodotMonoDecomp_DecompileModule(
    void* decompilerHandle,
    const char* outputCSProjectPath,
	const char** excludeFiles,
	int excludeFilesCount
);

const char* GodotMonoDecomp_DecompileIndividualFile(
	void* decompilerHandle,
	const char* file
);

int GodotMonoDecomp_GetNumberOfFilesNotPresentInFileMap(
	void* decompilerHandle
);

const char** GodotMonoDecomp_GetFilesNotPresentInFileMap(
	void* decompilerHandle
);

void GodotMonoDecomp_FreeObjectHandle(void* handle);

void GodotMonoDecomp_FreeArray(void* array, int length);

void GodotMonoDecomp_FreeString(void* str);


#ifdef __cplusplus
}
#endif

#endif // GODOT_MONO_DECOMP_H