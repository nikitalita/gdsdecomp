#include "register_types.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "servers/rendering/rendering_server.h"

#include "gui/find_replace_bar.h"
#include "gui/gdre_audio_stream_preview.h"
#include "gui/gdre_color_channel_selector.h"
#include "gui/gdre_standalone.h"
#include "gui/gdre_window.h"
#include "gui/gdre_xml_highlighter.h"
#include "gui/gui_icons.h"
#include "gui/mesh_previewer.h"
#include "gui/scene_previewer.h"
#include "gui/texture_layered_previewer.h"
#include "gui/texture_previewer.h"

static GDREGuiIcons *gui_icons = nullptr;

static GDREAudioStreamPreviewGenerator *audio_stream_preview_generator = nullptr;

void initialize_gdsdecomp_gui_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<GDREWindow>();
	ClassDB::register_class<GDREAcceptDialogBase>();
	ClassDB::register_class<GDREConfirmationDialogBase>();
	ClassDB::register_class<GDREFileDialog>();
	ClassDB::register_class<GodotREEditorStandalone>();
	ClassDB::register_class<GDREAudioStreamPreviewGeneratorNode>();
	ClassDB::register_class<GDREAudioStreamPreviewGenerator>();
	ClassDB::register_class<GDREAudioStreamPreview>();
	ClassDB::register_class<GDREPreviewer>();
	ClassDB::register_class<MeshPreviewer>();
	ClassDB::register_class<ScenePreviewer3D>();
	ClassDB::register_class<ScenePreviewer2D>();
	ClassDB::register_class<ScenePreviewer>();
	ClassDB::register_class<GDREColorChannelSelector>();
	ClassDB::register_class<TextureLayeredPreviewer>();
	ClassDB::register_class<TexturePreviewer>();
	ClassDB::register_class<GDREFindReplaceBar>();
	ClassDB::register_class<GDREXMLHighlighter>();

	gui_icons = memnew(GDREGuiIcons);

	audio_stream_preview_generator = memnew(GDREAudioStreamPreviewGenerator);
	Engine::get_singleton()->add_singleton(Engine::Singleton("GDREAudioStreamPreviewGenerator", GDREAudioStreamPreviewGenerator::get_singleton()));

	if (RenderingServer::get_singleton()) {
		TextureLayeredPreviewer::init_shaders();
		TexturePreviewer::init_shaders();
	}
}

void uninitialize_gdsdecomp_gui_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	if (RenderingServer::get_singleton()) {
		TextureLayeredPreviewer::finish_shaders();
		TexturePreviewer::finish_shaders();
	}
	if (audio_stream_preview_generator) {
		memdelete(audio_stream_preview_generator);
		audio_stream_preview_generator = nullptr;
	}

	if (gui_icons) {
		memdelete(gui_icons);
		gui_icons = nullptr;
	}
}
