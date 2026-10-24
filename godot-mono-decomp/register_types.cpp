#include "register_types.h"

#include "core/object/class_db.h"

#include "utility/gdre_config.h"

#include "wrapper/godot_mono_decomp_wrapper.h"


static void initialize_gdsdecomp_bytecode_options() {
#if !GODOT_MONO_DECOMP_DISABLED
	CONFIG_DEF(
			"CSharp/write_nuget_package_references",
			"Write NuGet package references",
			"Detect and write NuGet package references to the project file instead of assembly references.",
			true);
	CONFIG_DEF(
			"CSharp/copy_out_of_tree_references",
			"Copy referenced assemblies",
			"Copy referenced assemblies to the project directory.",
			true);
	CONFIG_DEF(
			"CSharp/verify_nuget_package_is_from_nuget_org",
			"Verify NuGet package is from NuGet.org",
			"Verify that the NuGet package is from NuGet.org before writing it to the project file.\nWARNING: This involves downloading the package from nuget.org and checking the hash of the downloaded package.",
			false);
	CONFIG_DEF(
			"CSharp/create_additional_projects_for_project_references",
			"Create additional projects for project references",
			"If a project reference is detected, create an additional project and add it to the solution.",
			true);
	CONFIG_DEF(
			"CSharp/remove_generated_json_context_body",
			"Remove generated Json context body",
			"Remove the body of generated JsonSourceGeneration context classes.",
			false);
	CONFIG_DEF(
			"CSharp/enable_collection_initializer_lifting",
			"Enable collection initializer lifting",
			"Enable the LiftCollectionInitializers transform.\nIf disabled, the legacy RemoveBogusBaseConstructorCalls transform is used.",
			true);
	CONFIG_DEF(
			"CSharp/emit_il_annotation_comments",
			"Emit IL annotation comments",
			"Emit ILInstruction annotations as comments for statement/expression nodes.\nIntended for debug verification of annotation propagation.",
			false);
	CONFIG_DEF_ENUM_SPECIAL("CSharp/force_language_version",
			"Force C# Language Level",
			"By default, the C# language level is selected based on the default language level of the target framework of the module.\nThis setting forces the C# language level to be the specified value.",
			0,
			&GodotMonoDecompWrapper::get_language_versions,
			GDREConfig::SETTING_FLAG_NONE);
	CONFIG_DEF(
			"CSharp/automatically_search_workshop_dependencies",
			"Automatically find search paths for Steam workshop modules",
			"Automatically find additional assembly search paths for Steam workshop modules.",
			true);
	CONFIG_DEF(
			"CSharp/additional_assembly_search_paths",
			"Additional assembly search paths",
			"Additional assembly search paths to use for resolving assembly references.\nThis is useful for resolving assembly references in Steam workshop modules that rely on additional assemblies.",
			Vector<String>());
	CONFIG_DEF(
			"CSharp/compile_after_decompile",
			"Compile after decompile",
			"Compile the C# project after decompiling.\nThis is done to prevent editor errors when first opening the project in the editor.\nThis requires that you have the dotnet sdk installed.",
			true);
#endif
}


void initialize_gdsdecomp_godot_mono_decomp_submodule(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
	ClassDB::register_class<GodotMonoDecompWrapper>();
	initialize_gdsdecomp_bytecode_options();
}

void uninitialize_gdsdecomp_godot_mono_decomp_submodule(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}
