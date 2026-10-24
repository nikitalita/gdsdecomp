#ifdef TOOLS_ENABLED
#include "register_types.h"

#include "core/object/class_db.h"

#include "editor/editor_node.h"
#include "editor/gdre_editor.h"
#include "gui/gdre_audio_stream_preview.h"
#include "gui/gdre_progress.h"
#include "main/gdre_main_loop.h"

static void gdsdecomp_editor_init_callback() {
	EditorNode *editor = EditorNode::get_singleton();
	editor->add_child(memnew(GodotREEditor(editor)));
	editor->add_child(memnew(GDREAudioStreamPreviewGeneratorNode));
	editor->add_child(memnew(GDREProgressDialog));
	GDREMainLoopNode::setup();
}

void initialize_gdsdecomp_editor_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_class<PackDialog>();
	ClassDB::register_class<NewPackDialog>();
	ClassDB::register_class<ScriptCompDialog>();
	ClassDB::register_class<ScriptDecompDialog>();
	EditorNode::add_init_callback(&gdsdecomp_editor_init_callback);
}

void uninitialize_gdsdecomp_editor_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
#endif
