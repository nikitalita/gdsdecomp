#ifndef GODOT_MONO_DECOMP_H
#define GODOT_MONO_DECOMP_H

#ifdef __cplusplus
extern "C" {
#endif

// Unless documented otherwise, all char pointers passed across this API are
// null-terminated UTF-8 strings. Strings returned by this API must be released
// with GodotMonoDecomp_FreeString or GodotMonoDecomp_FreeArray, as applicable.

typedef enum {
AUTO_LANGUAGE_VERSION = 0,
LANG_VER_CSharp1 = 1,
LANG_VER_CSharp2 = 2,
LANG_VER_CSharp3 = 3,
LANG_VER_CSharp4 = 4,
LANG_VER_CSharp5 = 5,
LANG_VER_CSharp6 = 6,
LANG_VER_CSharp7 = 7,
LANG_VER_CSharp7_1 = 701,
LANG_VER_CSharp7_2 = 702,
LANG_VER_CSharp7_3 = 703,
LANG_VER_CSharp8_0 = 800,
LANG_VER_CSharp9_0 = 900,
LANG_VER_CSharp10_0 = 1000,
LANG_VER_CSharp11_0 = 1100,
LANG_VER_Preview = 1100,
LANG_VER_CSharp12_0 = 1200,
LANG_VER_CSharp13_0 = 1300,
LANG_VER_CSharp14_0 = 1400,
LANG_VER_CSharp15_0 = 1500,
LANG_VER_Latest = 0x7FFFFFFF
} LanguageVersion;


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
	const char* godotVersionOverride,
	const char* settingsJSON
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

int* GodotMonoDecomp_GetLanguageVersions(int* r_num_versions);

int GodotMonoDecomp_IsCustomVersionDetected(void *decompilerHandle);

bool GodotMonoDecomp_CheckFileIsAssembly(const char* file);

const char** GodotMonoDecomp_GetGlobalClassFiles(void* decompilerHandle, int* r_num_files);

const char* GodotMonoDecomp_GetDefaultSettingsJSON();

const char* GodotMonoDecomp_GetSettingsJSON(void* decompilerHandle);

typedef enum {
GODOT_MONO_DECOMP_LOG_DEFAULT = -1, // resets to the default log level
GODOT_MONO_DECOMP_LOG_VERBOSE = 0,
GODOT_MONO_DECOMP_LOG_DEBUG = 1,
GODOT_MONO_DECOMP_LOG_INFO = 2,
GODOT_MONO_DECOMP_LOG_WARNING = 3,
GODOT_MONO_DECOMP_LOG_ERROR = 4
} GodotMonoDecompLogLevel;

// userdata, log level (GodotMonoDecompLogLevel), null-terminated UTF-8 message,
// function (method name of the call site), file (repo-relative path of the call site), line
typedef void (*logCallbackFunc_t)(void*, int, const char*, const char*, const char*, int);

/**
 * @brief Set the callback that receives all log messages from the library.
 *
 * By default, messages are written to stdout (debug/info) and stderr (warning/error).
 * Passing a null callback restores the default behavior.
 *
 * The callback may be invoked from any thread, including worker threads, so it must be thread-safe.
 * The message, function and file pointers are only valid for the duration of the call; copy them if you need to keep them.
 * The function, file and line describe where in the C# library the message was logged (like __FUNCTION__, __FILE__
 * and __LINE__). They are only provided for GODOT_MONO_DECOMP_LOG_WARNING and GODOT_MONO_DECOMP_LOG_ERROR;
 * for lower levels, function and file are NULL and line is 0.
 * The callback must not throw/longjmp across the call, and must not be freed while it is registered
 * (unregister it by passing null first).
 *
 * @param callback The log callback (a function pointer that takes in a void* (userdata), an int (log level), a const char* (message), a const char* (function), a const char* (file), and an int (line))
 * @param userData The user data to pass to the log callback
 * @param includeCallSiteForNonWarningMessages Whether to include the call site for non-warning messages
 */
void GodotMonoDecomp_SetLogCallback(logCallbackFunc_t callback, void* userData, GodotMonoDecompLogLevel logLevel, bool includeCallSiteForNonWarningMessages);

#ifdef TESTS_ENABLED
// for testing
int GodotMonoDecomp_GetPropertyHintMax();
int GodotMonoDecomp_GetPropertyHintValue(const char* hintName);
const char* GodotMonoDecomp_GetPropertyHintString(int hint);
int GodotMonoDecomp_GetVariantTypeMax();
int GodotMonoDecomp_GetVariantTypeValue(const char* variantTypeName);
const char* GodotMonoDecomp_GetVariantTypeString(int variantType);
int GodotMonoDecomp_GetPropertyUsageDefault();
int GodotMonoDecomp_GetPropertyUsageValue(const char* usageName);
const char* GodotMonoDecomp_GetPropertyUsageString(int usage);
#endif

#ifdef __cplusplus
}
#endif

#endif // GODOT_MONO_DECOMP_H
