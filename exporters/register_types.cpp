#include "register_types.h"
#include "autoconverted_exporter.h"
#include "core/object/class_db.h"
#include "csharp_exporter.h"
#include "dialogue_exporter.h"
#include "export_report.h"
#include "fontfile_exporter.h"
#include "func_godot_exporter.h"
#include "gdextension_exporter.h"
#include "gdscript_exporter.h"
#include "mp3str_exporter.h"
#include "obj_exporter.h"
#include "oggstr_exporter.h"
#include "sample_exporter.h"
#include "scene_exporter.h"
#include "shaderfile_exporter.h"
#include "spine_exporter.h"
#include "texture_exporter.h"
#include "translation_exporter.h"

//exporters
static Ref<AutoConvertedExporter> auto_converted_exporter = nullptr;
static Ref<CSharpExporter> csharp_exporter = nullptr;
static Ref<DialogueExporter> dialogue_exporter = nullptr;
static Ref<FuncGodotLmpExporter> func_godot_lmp_exporter = nullptr;
static Ref<FuncGodotMapExporter> func_godot_map_exporter = nullptr;
static Ref<FuncGodotWADExporter> func_godot_wad_exporter = nullptr;
static Ref<FontFileExporter> fontfile_exporter = nullptr;
static Ref<GDExtensionExporter> gdextension_exporter = nullptr;
static Ref<GDScriptExporter> gdscript_exporter = nullptr;
static Ref<Mp3StrExporter> mp3str_exporter = nullptr;
static Ref<OggStrExporter> oggstr_exporter = nullptr;
static Ref<SampleExporter> sample_exporter = nullptr;
static Ref<SceneExporter> scene_exporter = nullptr;
static Ref<ShaderFileExporter> shaderfile_exporter = nullptr;
static Ref<SpineAtlasExporter> spine_atlas_exporter = nullptr;
static Ref<SpineSkeletonExporter> spine_skeleton_exporter = nullptr;
static Ref<TextureExporter> texture_exporter = nullptr;
static Ref<HTerrainPackedTextureExporter> packed_texture_exporter = nullptr;
static Ref<HTerrainPackedTextureArrayExporter> packed_texture_array_exporter = nullptr;
static Ref<TranslationExporter> translation_exporter = nullptr;
static Ref<ObjExporter> obj_exporter = nullptr;

void init_exporters() {
	fontfile_exporter = memnew(FontFileExporter);
	mp3str_exporter = memnew(Mp3StrExporter);
	oggstr_exporter = memnew(OggStrExporter);
	sample_exporter = memnew(SampleExporter);
	texture_exporter = memnew(TextureExporter);
	packed_texture_exporter = memnew(HTerrainPackedTextureExporter);
	packed_texture_array_exporter = memnew(HTerrainPackedTextureArrayExporter);
	obj_exporter = memnew(ObjExporter);
	translation_exporter = memnew(TranslationExporter);
	scene_exporter = memnew(SceneExporter);
	auto_converted_exporter = memnew(AutoConvertedExporter);
	dialogue_exporter = memnew(DialogueExporter);
	func_godot_lmp_exporter = memnew(FuncGodotLmpExporter);
	func_godot_map_exporter = memnew(FuncGodotMapExporter);
	func_godot_wad_exporter = memnew(FuncGodotWADExporter);
	gdscript_exporter = memnew(GDScriptExporter);
	csharp_exporter = memnew(CSharpExporter);
	gdextension_exporter = memnew(GDExtensionExporter);
	shaderfile_exporter = memnew(ShaderFileExporter);
	spine_atlas_exporter = memnew(SpineAtlasExporter);
	spine_skeleton_exporter = memnew(SpineSkeletonExporter);
	Exporter::add_exporter(auto_converted_exporter);
	Exporter::add_exporter(fontfile_exporter);
	Exporter::add_exporter(mp3str_exporter);
	Exporter::add_exporter(oggstr_exporter);
	Exporter::add_exporter(sample_exporter);
	Exporter::add_exporter(texture_exporter);
	Exporter::add_exporter(obj_exporter);
	Exporter::add_exporter(dialogue_exporter);
	Exporter::add_exporter(func_godot_lmp_exporter);
	Exporter::add_exporter(func_godot_map_exporter);
	Exporter::add_exporter(func_godot_wad_exporter);
	Exporter::add_exporter(gdextension_exporter);
	Exporter::add_exporter(scene_exporter);
	Exporter::add_exporter(translation_exporter);
	Exporter::add_exporter(gdscript_exporter);
	Exporter::add_exporter(csharp_exporter);
	Exporter::add_exporter(shaderfile_exporter);
	Exporter::add_exporter(spine_atlas_exporter);
	Exporter::add_exporter(spine_skeleton_exporter);
	Exporter::add_exporter(packed_texture_exporter);
	Exporter::add_exporter(packed_texture_array_exporter);
}

void deinit_exporters() {
	if (auto_converted_exporter.is_valid()) {
		Exporter::remove_exporter(auto_converted_exporter);
	}
	if (fontfile_exporter.is_valid()) {
		Exporter::remove_exporter(fontfile_exporter);
	}
	if (gdextension_exporter.is_valid()) {
		Exporter::remove_exporter(gdextension_exporter);
	}
	if (mp3str_exporter.is_valid()) {
		Exporter::remove_exporter(mp3str_exporter);
	}
	if (oggstr_exporter.is_valid()) {
		Exporter::remove_exporter(oggstr_exporter);
	}
	if (sample_exporter.is_valid()) {
		Exporter::remove_exporter(sample_exporter);
	}
	if (scene_exporter.is_valid()) {
		Exporter::remove_exporter(scene_exporter);
	}
	if (texture_exporter.is_valid()) {
		Exporter::remove_exporter(texture_exporter);
	}
	if (translation_exporter.is_valid()) {
		Exporter::remove_exporter(translation_exporter);
	}
	if (gdscript_exporter.is_valid()) {
		Exporter::remove_exporter(gdscript_exporter);
	}
	if (obj_exporter.is_valid()) {
		Exporter::remove_exporter(obj_exporter);
	}
	if (csharp_exporter.is_valid()) {
		Exporter::remove_exporter(csharp_exporter);
	}
	if (shaderfile_exporter.is_valid()) {
		Exporter::remove_exporter(shaderfile_exporter);
	}
	if (spine_atlas_exporter.is_valid()) {
		Exporter::remove_exporter(spine_atlas_exporter);
	}
	if (spine_skeleton_exporter.is_valid()) {
		Exporter::remove_exporter(spine_skeleton_exporter);
	}
	if (dialogue_exporter.is_valid()) {
		Exporter::remove_exporter(dialogue_exporter);
	}
	if (func_godot_lmp_exporter.is_valid()) {
		Exporter::remove_exporter(func_godot_lmp_exporter);
	}
	if (func_godot_map_exporter.is_valid()) {
		Exporter::remove_exporter(func_godot_map_exporter);
	}
	if (func_godot_wad_exporter.is_valid()) {
		Exporter::remove_exporter(func_godot_wad_exporter);
	}
	if (packed_texture_exporter.is_valid()) {
		Exporter::remove_exporter(packed_texture_exporter);
	}
	if (packed_texture_array_exporter.is_valid()) {
		Exporter::remove_exporter(packed_texture_array_exporter);
	}
	auto_converted_exporter = nullptr;
	fontfile_exporter = nullptr;
	gdextension_exporter = nullptr;
	mp3str_exporter = nullptr;
	oggstr_exporter = nullptr;
	sample_exporter = nullptr;
	scene_exporter = nullptr;
	texture_exporter = nullptr;
	translation_exporter = nullptr;
	gdscript_exporter = nullptr;
	obj_exporter = nullptr;
	dialogue_exporter = nullptr;
	csharp_exporter = nullptr;
	shaderfile_exporter = nullptr;
	spine_atlas_exporter = nullptr;
	spine_skeleton_exporter = nullptr;
	func_godot_lmp_exporter = nullptr;
	func_godot_map_exporter = nullptr;
	func_godot_wad_exporter = nullptr;
	packed_texture_exporter = nullptr;
	packed_texture_array_exporter = nullptr;
}

void initialize_gdsdecomp_exporters_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<ExportReport>();
	ClassDB::register_class<Exporter>();
	ClassDB::register_class<ResourceExporter>();
	ClassDB::register_class<AutoConvertedExporter>();
	ClassDB::register_class<FontFileExporter>();
	ClassDB::register_class<Mp3StrExporter>();
	ClassDB::register_class<OggStrExporter>();
	ClassDB::register_class<SampleExporter>();
	ClassDB::register_class<SceneExporter>();
	ClassDB::register_class<TextureExporter>();
	ClassDB::register_class<ShaderFileExporter>();
	ClassDB::register_class<TranslationExporter>();
	ClassDB::register_class<GDScriptExporter>();
	ClassDB::register_class<GDExtensionExporter>();
	ClassDB::register_class<ObjExporter>();
	ClassDB::register_class<CSharpExporter>();
	ClassDB::register_class<SpineAtlasExporter>();
	ClassDB::register_class<SpineSkeletonExporter>();
	ClassDB::register_class<HTerrainPackedTextureExporter>();
	ClassDB::register_class<HTerrainPackedTextureArrayExporter>();
	ClassDB::register_class<FuncGodotLmpExporter>();
	ClassDB::register_class<FuncGodotMapExporter>();
	ClassDB::register_class<FuncGodotWADExporter>();
	ClassDB::register_class<DialogueExporter>();
	ClassDB::register_class<FuncGodotLmpExporter>();
	ClassDB::register_class<FuncGodotMapExporter>();
	ClassDB::register_class<FuncGodotWADExporter>();
	init_exporters();
}

void uninitialize_gdsdecomp_exporters_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	deinit_exporters();
}
