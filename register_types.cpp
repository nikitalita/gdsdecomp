/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "core/object/class_db.h"
#include "core/string/regex.h"
#include "modules/register_module_types.h"
#include "utility/app_version_getter.h"
#include "utility/file_access_gdre.h"
#include "utility/file_access_patched_gdre.h"
#include "utility/image_saver.h"
#include "utility/text_diff.h"

#include "godot-mono-decomp/wrapper/godot_mono_decomp_wrapper.h"
#include "main/gdre_main_loop.h"
#include "utility/common.h"
#include "utility/gdre_config.h"
#include "utility/gdre_settings.h"
#include "utility/glob.h"
#include "utility/godotver.h"
#include "utility/import_exporter.h"
#include "utility/packed_file_info.h"
#include "utility/pck_creator.h"
#include "utility/pck_dumper.h"
#include "utility/task_manager.h"

#include "bytecode/register_types.h"
#include "compat/register_types.h"
#include "crypto/register_types.h"
#include "exporters/register_types.h"
#include "godot-mono-decomp/register_types.h"
#include "gui/register_types.h"
#include "plugin_manager/register_types.h"

#ifdef TOOLS_ENABLED
#include "editor/register_types.h"
#endif

static GDREMainLoop *gdre_main_loop = nullptr;
static GDRESettings *gdre_singleton = nullptr;
static TaskManager *task_manager = nullptr;
static GDREConfig *gdre_config = nullptr;
static GDREPackedData *gdre_packeddata_singleton = nullptr;

void init_ver_regex() {
	SemVer::strict_regex = RegEx::create_from_string(GodotVer::strict_regex_str);
	GodotVer::non_strict_regex = RegEx::create_from_string(GodotVer::non_strict_regex_str);
	Glob::magic_check = RegEx::create_from_string(Glob::magic_pattern);
	Glob::escapere = RegEx::create_from_string(Glob::escape_pattern);
}

void free_ver_regex() {
	SemVer::strict_regex = Ref<RegEx>();
	GodotVer::non_strict_regex = Ref<RegEx>();
	Glob::magic_check = Ref<RegEx>();
	Glob::escapere = Ref<RegEx>();
}

void initialize_gdsdecomp_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	if (!Engine::get_singleton()->is_editor_hint()) {
		float scale = GDRESettings::get_auto_display_scale();
		ProjectSettings::get_singleton()->set_setting("display/window/stretch/scale", scale);
	}

	ClassDB::register_class<SemVer>();
	ClassDB::register_class<GodotVer>();
	ClassDB::register_class<Glob>();
	init_ver_regex();

	ClassDB::register_class<FileAccessGDRE>();
	ClassDB::register_class<PckDumper>();
	ClassDB::register_class<PckCreator>();
	ClassDB::register_class<ResourceImportMetadatav2>();
	ClassDB::register_abstract_class<ImportInfo>();
	ClassDB::register_class<ProjectConfigLoader>();
	ClassDB::register_class<AppVersionGetter>();

	ClassDB::register_class<ImportInfoModern>();
	ClassDB::register_class<ImportInfov2>();
	ClassDB::register_class<ImportInfoDummy>();
	ClassDB::register_class<ImportInfoRemap>();
	ClassDB::register_class<ImportInfoGDExt>();
	ClassDB::register_class<ImportExporter>();
	ClassDB::register_class<ImportExporterReport>();
	ClassDB::register_class<GDRESettings>();

	ClassDB::register_class<PackedFileInfo>();
	ClassDB::register_class<PackInfo>();

	ClassDB::register_class<FileAccessPatchedGDRE>();

	ClassDB::register_class<GDRECommon>();
	ClassDB::register_class<TextDiff>();
	ClassDB::register_class<TaskManager>();
	ClassDB::register_class<ResourceInfo>();
	ClassDB::register_class<GodotMonoDecompWrapper>();
	ClassDB::register_class<CoreBind::PackedFile>();
	ClassDB::register_class<PackSourceCustom>();

	ClassDB::register_class<GDREConfig>();
	ClassDB::register_class<GDREConfigSetting>();
	ClassDB::register_class<ImageSaver>();

	ClassDB::register_class<GDRESceneTree>();
	ClassDB::register_class<GDREMainLoop>();

	ClassDB::register_class<GDREPackedData>();
	gdre_packeddata_singleton = memnew(GDREPackedData);
	Engine::get_singleton()->add_singleton(Engine::Singleton("GDREPackedData", GDREPackedData::get_singleton()));
	gdre_main_loop = memnew(GDREMainLoop);
	Engine::get_singleton()->add_singleton(Engine::Singleton("GDREMainLoop", GDREMainLoop::get_singleton()));
	gdre_singleton = memnew(GDRESettings);
	Engine::get_singleton()->add_singleton(Engine::Singleton("GDRESettings", GDRESettings::get_singleton()));
	gdre_config = memnew(GDREConfig);
	Engine::get_singleton()->add_singleton(Engine::Singleton("GDREConfig", GDREConfig::get_singleton()));
	task_manager = memnew(TaskManager);
	Engine::get_singleton()->add_singleton(Engine::Singleton("TaskManager", TaskManager::get_singleton()));
	initialize_gdsdecomp_bytecode_submodule(p_level);
	initialize_gdsdecomp_crypto_submodule(p_level);
	initialize_gdsdecomp_compat_submodule(p_level);
	initialize_gdsdecomp_godot_mono_decomp_submodule(p_level);
	initialize_gdsdecomp_plugin_manager_submodule(p_level);
	initialize_gdsdecomp_exporters_submodule(p_level);
	initialize_gdsdecomp_gui_submodule(p_level);
#ifdef TOOLS_ENABLED
	initialize_gdsdecomp_editor_submodule(p_level);
#endif
}

void uninitialize_gdsdecomp_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
#ifdef TOOLS_ENABLED
	uninitialize_gdsdecomp_editor_submodule(p_level);
#endif
	uninitialize_gdsdecomp_gui_submodule(p_level);
	uninitialize_gdsdecomp_exporters_submodule(p_level);
	uninitialize_gdsdecomp_plugin_manager_submodule(p_level);
	uninitialize_gdsdecomp_godot_mono_decomp_submodule(p_level);
	uninitialize_gdsdecomp_crypto_submodule(p_level);
	uninitialize_gdsdecomp_compat_submodule(p_level);
	uninitialize_gdsdecomp_bytecode_submodule(p_level);
	if (gdre_config) {
		memdelete(gdre_config);
		gdre_config = nullptr;
	}
	if (gdre_singleton) {
		memdelete(gdre_singleton);
		gdre_singleton = nullptr;
	}
	if (task_manager) {
		memdelete(task_manager);
		task_manager = nullptr;
	}
	free_ver_regex();
	if (gdre_main_loop) {
		memdelete(gdre_main_loop);
		gdre_main_loop = nullptr;
	}
	if (gdre_packeddata_singleton) {
		memdelete(gdre_packeddata_singleton);
		gdre_packeddata_singleton = nullptr;
	}
}
