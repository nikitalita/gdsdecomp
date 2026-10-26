#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include "godot_mono_decomp.h"

static void _godot_mono_decomp_log_callback(void *p_userdata, int p_level, const char *p_message, const char *p_function, const char *p_file, int p_line) {
    switch (p_level) {
        case GODOT_MONO_DECOMP_LOG_WARNING:
            std::cout << "WARNING: " << p_message << std::endl;
            std::cout << "     at: " << p_function << " (" << p_file << ":" << p_line << ")" << std::endl;
            break;
        case GODOT_MONO_DECOMP_LOG_ERROR:
            std::cout << "ERROR: " << p_message << std::endl;
            std::cout << "     at: " << p_function << " (" << p_file << ":" << p_line << ")" << std::endl;
            break;
        default:
            std::cout << p_message << std::endl;
            break;
    }
}

int main(int argc, char* argv[]) {
	GodotMonoDecomp_SetLogCallback(&_godot_mono_decomp_log_callback, nullptr, GODOT_MONO_DECOMP_LOG_INFO, false);
    std::cout << "Godot Mono Decompiler AOT Test (Static Linking)" << std::endl;
    std::cout << "================================================" << std::endl;

    // Check if we have the required arguments
    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <assembly_path> <output_directory> [<reference_paths>...]" << std::endl;
        std::cout << "Example: " << argv[0] << " ./test.dll ./output ./reference1 ./reference2" << std::endl;
        return 1;
    }

    std::string assemblyPath = argv[1];
    std::string outputDir = argv[2];
    std::vector<std::string> referencePaths;
    for (int i = 3; i < argc; i++) {
        referencePaths.push_back(argv[i]);
    }

    // Check if the assembly file exists
    if (!std::filesystem::exists(assemblyPath)) {
        std::cerr << "Error: Assembly file '" << assemblyPath << "' does not exist." << std::endl;
        return 1;
    }

    // Create output directory if it doesn't exist
    std::filesystem::create_directories(outputDir);

    // Construct the output project file path
    std::filesystem::path assemblyFilePath(assemblyPath);
    std::string projectFileName = assemblyFilePath.stem().string() + ".csproj";
    std::string outputProjectPath = (std::filesystem::path(outputDir) / projectFileName).string();

    std::cout << "Assembly Path: " << assemblyPath << std::endl;
    std::cout << "Output Directory: " << outputDir << std::endl;
    std::cout << "Output Project File: " << outputProjectPath << std::endl;
    std::cout << "Reference Paths: " << referencePaths.size() << std::endl;

    // Prepare arguments for the function call
    const char* assemblyPathPtr = assemblyPath.c_str();
    const char* outputProjectPathPtr = outputProjectPath.c_str();
    const char* projectPathPtr = outputDir.c_str();

    // Prepare reference paths
    const char** referenceDirs = new const char*[referencePaths.size()];
    for (int i = 0; i < referencePaths.size(); i++) {
        referenceDirs[i] = referencePaths[i].c_str();
    }
    int referencePathsCount = referencePaths.size();

    std::cout << "Calling statically linked AOT decompile function..." << std::endl;

    // Call the statically linked AOT function
    int result = GodotMonoDecomp_DecompileProject(
        assemblyPathPtr,
        outputProjectPathPtr,
        projectPathPtr,
        referenceDirs,
        referencePathsCount
    );

    // Clean up dynamic memory
    delete[] referenceDirs;

    // Check the result
    if (result == 0) {
        std::cout << "SUCCESS: Decompilation completed successfully!" << std::endl;

        // Check if output files were created
        if (std::filesystem::exists(outputProjectPath)) {
            std::cout << "Output project file created: " << outputProjectPath << std::endl;
        }

        // List created files
        std::cout << "Files in output directory:" << std::endl;
        for (const auto& entry : std::filesystem::directory_iterator(outputDir)) {
            std::cout << "  - " << entry.path().filename() << std::endl;
        }
    } else {
        std::cerr << "ERROR: Decompilation failed with return code: " << result << std::endl;
    }

    std::cout << "Test completed." << std::endl;
    return result;
}
