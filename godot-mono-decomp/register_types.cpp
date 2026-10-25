#include "register_types.h"

#include "core/object/class_db.h"

#include "wrapper/godot_mono_decomp_wrapper.h"


void initialize_gdsdecomp_godot_mono_decomp_submodule(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
	ClassDB::register_class<GodotMonoDecompWrapper>();
#if !GODOT_MONO_DECOMP_DISABLED
	GodotMonoDecompWrapper::add_default_config_settings();
#endif
}

void uninitialize_gdsdecomp_godot_mono_decomp_submodule(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
}
