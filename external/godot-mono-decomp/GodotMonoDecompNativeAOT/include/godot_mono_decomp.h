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

#ifdef __cplusplus
}
#endif

#endif // GODOT_MONO_DECOMP_H 