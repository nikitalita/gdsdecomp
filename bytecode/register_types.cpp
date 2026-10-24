#include "register_types.h"
#include "core/object/class_db.h"

#include "bytecode/bytecode_base.h"
#include "bytecode/bytecode_versions.h"

void initialize_gdsdecomp_bytecode_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_abstract_class<GDScriptDecomp>();
	register_decomp_versions();
}

void uninitialize_gdsdecomp_bytecode_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
