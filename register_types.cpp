/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "core/object_type_db.h"

#include "bytecode/bytecode_versions.h"
#include "editor/gdre_editor.h"

#ifdef TOOLS_ENABLED
void gdsdecomp_init_callback() {

	EditorNode *editor = EditorNode::get_singleton();
	editor->add_child(memnew(GodotREEditor(editor)));
};
#endif

void register_gdsdecomp_types() {

	ObjectTypeDB::register_virtual_type<GDScriptDecomp>();
	register_decomp_versions();

	ObjectTypeDB::register_type<GodotREEditorStandalone>();

	ObjectTypeDB::register_type<PackDialog>();
	ObjectTypeDB::register_type<NewPackDialog>();
	ObjectTypeDB::register_type<ScriptCompDialog>();
	ObjectTypeDB::register_type<ScriptDecompDialog>();
#ifdef TOOLS_ENABLED
	EditorNode::add_init_callback(&gdsdecomp_init_callback);
#endif
}

void unregister_gdsdecomp_types() {
	//NOP
}
