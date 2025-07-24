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
    int referencePathsCount,
	const char* godotVersionOverride
);

int GodotMonoDecomp_DecompileModule(
    void* decompilerHandle,
    const char* outputCSProjectPath,
	const char** excludeFiles,
	int excludeFilesCount
);


// userdata, current step, total steps, status string, cancellation token source
typedef int (*progressCallbackFunc_t)(void*, int, int, const char*);

/**
 * @brief Decompile a module with progress callback.
 *
 * The progress callback is a function that will be called with the progress of the decompilation.
 * The function must return true if the decompilation should continue, false otherwise.
 *
 * The progress callback must be a function that takes a void pointer as an argument.
 *
 * @param decompilerHandle The handle to the decompiler
 * @param outputCSProjectPath The path to the output CS project
 * @param excludeFiles The files to exclude from the decompilation
 * @param excludeFilesCount The number of files to exclude
 * @param progressCallback The progress callback (a function pointer that takes in a void*, an int, an int, a const char*, and a void* (cancellation token))
 * @param userData The user data to pass to the progress callback
 */
int GodotMonoDecomp_DecompileModuleWithProgress(
	void* decompilerHandle,
	const char* outputCSProjectPath,
	const char** excludeFiles,
	int excludeFilesCount,
	progressCallbackFunc_t progressCallback,
	void* userData
);

int GodotMonoDecomp_CancelDecompilation(void* cancelSource);

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

int GodotMonoDecomp_GetNumberOfFilesInFileMap(void* decompilerHandle);

const char** GodotMonoDecomp_GetFilesInFileMap(void* decompilerHandle);

char32_t** GodotMonoDecomp_GetAllUtf32StringsInModule(
	void* decompilerHandle,
	int* r_num_strings
);

const char* GodotMonoDecomp_GetScriptInfo(
	void* decompilerHandle,
	const char* file
);

void GodotMonoDecomp_FreeObjectHandle(void* handle);

void GodotMonoDecomp_FreeArray(void* array, int length);

void GodotMonoDecomp_FreeString(void* str);


#ifdef __cplusplus
}
#endif

#endif // GODOT_MONO_DECOMP_H
