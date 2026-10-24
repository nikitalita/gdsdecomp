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

#include "utility/gdre_config.h"
#include "utility/gdre_settings.h"

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
	Exporter::add_exporter(translation_exporter);
	Exporter::add_exporter(scene_exporter);
	Exporter::add_exporter(gdscript_exporter);
	Exporter::add_exporter(csharp_exporter);
	Exporter::add_exporter(gdextension_exporter);
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

class GDREConfigSetting_TranslationExporter_LoadKeyHintFile : public GDREConfigSetting {
	GDSOFTCLASS(GDREConfigSetting_TranslationExporter_LoadKeyHintFile, GDREConfigSetting);

public:
	GDREConfigSetting_TranslationExporter_LoadKeyHintFile() :
			GDREConfigSetting(
					"Exporter/Translation/load_key_hint_file",
					"Load key hint file",
					"Load a key hint file to use for the translation exporter",
					"",
					GDREConfig::SETTING_FLAG_FILEPICKER) {}
	String error_message;

	virtual bool is_virtual_setting() const override { return true; }
	virtual String get_error_message() const override { return error_message; }
	virtual void clear_error_message() override { error_message = ""; }
	virtual Variant get_value() const override {
		return "";
	}
	virtual void set_value(const Variant &p_value, bool p_force_ephemeral = false) override {
		String path = p_value;
		if (path.is_empty()) {
			return;
		}
		Error err = GDRESettings::get_singleton() ? GDRESettings::get_singleton()->load_translation_key_hint_file(path) : ERR_UNAVAILABLE;
		if (err != OK) {
			WARN_PRINT("Failed to load key hint file: " + path);
			error_message = "Failed to load key hint file: " + path;
			return;
		}
		error_message = "";
	}
};

static inline void initialize_config_options() {
	CONFIG_DEF(
			"Exporter/GDExtension/make_editor_copy",
			"Copy template_release plugins to editor plugin",
			"If the plugin was not downloaded, copy the template_release plugin (the library distributed with the game) to the editor plugin path (required for using the plugin in the editor).",
			true);
	CONFIG_DEF(
			"Exporter/Scene/GLTF/force_lossless_images",
			"Force lossless images",
			"Forces images to be saved as lossless PNGs when exporting to GLTF, regardless of the original image format",
			false);
	// TODO: This isn't viable yet, as the gltf document converter doesn't write double precision values
	CONFIG_DEF(
			"Exporter/Scene/GLTF/use_double_precision",
			"Use double precision",
			"Uses double precision for all floating-point values in the GLTF document",
			false,
			GDREConfig::SETTING_FLAG_HIDDEN);
	CONFIG_DEF(
			"Exporter/Scene/GLTF/force_export_multi_root",
			"Force export multi root",
			"Forces the export to export in multi-root mode, even if the scene is a single root",
			false);
	CONFIG_DEF(
			"Exporter/Scene/GLTF/force_require_KHR_node_visibility",
			"Force require KHR_node_visibility extension",
			"By default, the exporter will only set KHR_node_visibility as an optional extension.\nThis setting forces the exporter to require the extension regardless of engine version.",
			false);
	CONFIG_DEF(
			"Exporter/Scene/GLTF/ignore_missing_dependencies",
			"Ignore missing dependencies",
			"Ignore missing dependencies when exporting the scene.",
			false);
	CONFIG_DEF(
			"Exporter/Scene/GLTF/remove_physics_bodies",
			"Remove physics bodies",
			"Removes physics bodies (CollisionShape3D, CollisionObject3D, etc.) from the scene when exporting to GLTF",
			false);
	CONFIG_DEF(
			"Exporter/Scene/GLTF/replace_shader_materials",
			"Replace shader materials",
			"Replaces shader materials with generated standard materials when exporting the scene.\nSolves issues with exported scenes not having any textures.\nWARNING: This is experimental and may result in inaccurate exports.",
			false);
	CONFIG_DEF(
			"Exporter/Scene/GLTF/debug_copies",
			"Create debug copies",
			"For development.",
			false,
			GDREConfig::SETTING_FLAG_HIDDEN);
	CONFIG_DEF(
			"Exporter/Texture/create_lossless_copy",
			"Create lossless copy",
			"Create a lossless .png copy of exported textures in '<OUTPUT_DIR>/.assets' if the texture is stored in a lossy format (e.g. jpeg)",
			false);
	CONFIG_DEF(
			"Exporter/Translation/disable_key_recovery",
			"Disable translation key recovery",
			"Disable translation key recovery.\nThis will skip the translation key recovery process entirely.",
			false);
	CONFIG_DEF_ENUM(
			// Name unchanged for backwards compatibility (bool `true` -> `1` == `Very Thorough`)
			"Exporter/Translation/more_thorough_recovery",
			"Key recovery mode",
			"Sets the key recovery mode.\n"
			"* Default: Balanced, runs most stages of key recovery with reduced working set size\n"
			"* Very Thorough: runs all stages of key recovery with full working set size\n"
			"    WARNING: this is not guaranteed to find all the keys in the translation file and may massively increase the time it takes to run.\n"
			"* Only resource strings: Only runs first stage of key recovery, fastest option",
			0,
			"Default,Very Thorough,Only resource strings");
	CONFIG_DEF_CUSTOM(Ref<GDREConfigSetting>(memnew(GDREConfigSetting_TranslationExporter_LoadKeyHintFile())));
	CONFIG_DEF(
			"Exporter/Translation/skip_loading_resource_strings",
			"Skip loading resource strings",
			"Skip loading resource strings from all resources during translation recovery.\nDon't enable this without a key hint file.",
			false,
			GDREConfig::SETTING_FLAG_EPHEMERAL);
	CONFIG_DEF(
			"Exporter/Translation/dump_resource_strings",
			"Dump resource strings",
			"Dumps collected resource strings to '<OUTPUT_DIR>/.assets/resource_strings.stringdump'",
			false,
			GDREConfig::SETTING_FLAG_EPHEMERAL);
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
	initialize_config_options();
}

void uninitialize_gdsdecomp_exporters_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	deinit_exporters();
}
