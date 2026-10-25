#include "gdre_config.h"
#include "common.h"
#include "core/io/config_file.h"
#include "core/object/class_db.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "core/variant/variant_utility.h"
#include "gdre_logger.h"
#include "gdre_settings.h"
#include "gdre_version.gen.h"

GDREConfig *GDREConfig::singleton = nullptr;

GDREConfig *GDREConfig::get_singleton() {
	return singleton;
}

static Dictionary get_max_cores_to_use_list_of_possible_values() {
	Dictionary ret;
	ret[-1] = "Auto-detect";
	// descending order
	for (int i = WorkerThreadPool::get_singleton()->get_thread_count(); i > 0; i--) {
		ret[i] = String::num_int64(i);
	}
	return ret;
}

void GDREConfig::_init_default_settings() {
	CONFIG_DEF(
			"last_gdre_version_used",
			"Last GDRE version used",
			"",
			"",
			SETTING_FLAG_HIDDEN);
	CONFIG_DEF(
			"download_plugins",
			"Download plugins",
			"Automatically detect binary plugin versions and download them from the asset library",
			false);
	CONFIG_DEF(
			"load_embedded_zips",
			"Load embedded zips",
			"Load embedded zips within the project pack as part of project loading/recovery",
			true);
	CONFIG_DEF(
			"delete_auto_converted_files",
			"Delete auto-converted files",
			"Delete auto-converted files (*.gdc, etc.) after exporting. If disabled, the files will be moved to the `.autoconverted` folder.",
			false);
	CONFIG_DEF("default_parent_folder_for_recovery",
			"Default parent folder for recovery",
			"The default parent folder to use for recovery.\nIf not set, the user's Desktop directory will be used.",
			OS::get_singleton()->get_system_dir(OS::SYSTEM_DIR_DESKTOP),
			GDREConfig::SETTING_FLAG_DIRPICKER);
	CONFIG_DEF_ENUM_SPECIAL("max_cores_to_use",
			"Max cores for threaded tasks",
			"The maximum number of cores to use for threaded tasks (Extract, project recovery, etc.)",
			-1,
			&get_max_cores_to_use_list_of_possible_values,
			GDREConfig::SETTING_FLAG_NONE);
	CONFIG_DEF(
			"force_single_threaded",
			"Force single-threaded mode",
			"Forces all tasks to run on the main thread",
			false);
	CONFIG_DEF(
			"write_json_report",
			"Write JSON report",
			"Write a JSON report to the output directory after exporting.",
			false,
			SETTING_FLAG_HIDDEN);
	CONFIG_DEF(
			"ask_for_download",
			"Ask for download",
			"",
			true,
			SETTING_FLAG_HIDDEN);
	CONFIG_DEF(
			"last_showed_disclaimer",
			"Last showed disclaimer",
			"",
			"<NONE>",
			SETTING_FLAG_HIDDEN);
	CONFIG_DEF(
			"execute_visual_shader_node_scripts",
			"Execute VisualShaderNodeCustom scripts",
			"Execute VisualShaderNodeCustom scripts when loading visual shaders.\nSetting this to `true` can fix errors occurring when exporting scenes that contain VisualShaders.\n\nWARNING: As this executes arbitrary code, this should be considered unsafe.\nEnsure that you trust the source code in this project before enabling this.",
			false);
	CONFIG_DEF(
			"Preview/use_scene_view_by_default",
			"Use scene view by default",
			"Use scene view by default instead of the text preview.\nWARNING: Scene view is still experimental and certain scenes may cause the program to become unresponsive.",
			false);
	CONFIG_DEF(
			"Recovery/clear_output_dir_except_git_before_full_recovery",
			"Clear output directory except git before full recovery",
			"Clear the output directory except for the .git directory before running a full recovery (i.e. no includes/excludes)",
			false,
			SETTING_FLAG_HIDDEN);
	CONFIG_DEF(
			"Recovery/git/create_git_repo",
			"Create git repo in recovered project",
			"Create a git repo in the output directory after exporting.\nRequires git to be installed on the system.",
			false);
	CONFIG_DEF(
			"Recovery/git/add_imports_to_git_repo",
			"Add imports to git repo",
			"Add .godot/imported/ (or .import/ for Godot 3) to the git repo.",
			false);
	CONFIG_DEF_ENUM(
			"Script/Indent/type",
			"Indent type",
			"The type of indentation to use when decompiling and displaying scripts.",
			0,
			"Tabs,Spaces");
	CONFIG_DEF_RANGE(
			"Script/Indent/size",
			"Tab size",
			"The number of spaces to use when decompiling and displaying scripts.",
			4,
			"1,64,1");
}

// static const HashMap<String, String> deprecated_setting_mappings = {
// 	{ "General/download_plugins", "Exporter/GDExtension/download_plugins" },
// };

GDREConfig::GDREConfig() {
	singleton = this;
	_init_default_settings();
}

GDREConfig::~GDREConfig() {
	if (singleton == this) {
		singleton = nullptr;
	}
	default_settings.clear();
}

TypedArray<GDREConfigSetting> GDREConfig::get_all_settings() const {
	TypedArray<GDREConfigSetting> settings;
	settings.resize(default_settings.size());
	int i = 0;
	for (const auto &[key, setting] : default_settings) {
		settings.set(i, setting);
		i++;
	}
	return settings;
}

void GDREConfig::load_config() {
	settings.clear();

	for (const auto &[key, setting] : default_settings) {
		if (setting->is_virtual_setting()) {
			continue;
		}
		set_setting(setting->get_full_name(), setting->get_default_value(), setting->is_ephemeral());
	}

	auto cfg_path = get_config_path();
	if (FileAccess::exists(cfg_path)) {
		Ref<ConfigFile> config = memnew(ConfigFile);
		Error err = config->load(cfg_path);
		if (err != OK) {
			WARN_PRINT("Failed to load config file: " + cfg_path);
		}
		for (auto &section : config->get_sections()) {
			for (auto &key : config->get_section_keys(section)) {
				set_setting(section + "/" + key, config->get_value(section, key));
			}
		}
	}
}

String GDREConfig::get_config_path() {
	return GDRESettings::get_gdre_user_path().path_join("gdre_settings.cfg");
}

void GDREConfig::save_config() {
	auto cfg_path = get_config_path();
	Ref<ConfigFile> config = memnew(ConfigFile);
	for (const auto &[key, p_value] : settings) {
		String name = get_name_from_key(key);
		Variant value = p_value;
		if (default_settings.has(key) && default_settings[key]->get_type() != Variant::Type::NIL) {
			value = VariantUtilityFunctions::type_convert(p_value, default_settings[key]->get_type());
		}
		if (!default_settings.has(key) || get_default_value(key) != value) {
			config->set_value(get_section_from_key(key), name, value);
		}
	}
	config->set_value("General", "last_gdre_version_used", GDRE_VERSION);
	gdre::ensure_dir(cfg_path.get_base_dir());
	Error err = config->save(cfg_path);
	if (err != OK) {
		WARN_PRINT("Failed to save config file: " + cfg_path);
	}
}

String get_full_name(const String &p_setting) {
	if (!p_setting.contains_char('/')) {
		return "General/" + p_setting;
	}
	return p_setting;
}

void GDREConfig::set_setting(const String &p_setting, const Variant &p_value, bool p_force_ephemeral) {
	String full_name = get_full_name(p_setting);
	Variant value = p_value;
	if (default_settings.has(full_name) && default_settings[full_name]->get_type() != Variant::Type::NIL) {
		value = VariantUtilityFunctions::type_convert(p_value, default_settings[full_name]->get_type());
	}
	if (p_force_ephemeral || ephemeral_settings.contains(full_name)) {
		ephemeral_settings.try_emplace_l(full_name, [=](auto &v) { v.second = value; }, value);
		return;
	}
	settings.try_emplace_l(full_name, [=](auto &v) { v.second = value; }, value);
}

bool GDREConfig::has_setting(const String &p_setting) const {
	return settings.contains(get_full_name(p_setting)) || ephemeral_settings.contains(get_full_name(p_setting));
}

Variant GDREConfig::get_setting(const String &p_setting, const Variant &p_default_value) const {
	Variant ret = p_default_value;
	if (ephemeral_settings.if_contains(get_full_name(p_setting), [&](const auto &v) { ret = v.second; })) {
		return ret;
	}
	settings.if_contains(get_full_name(p_setting), [&](const auto &v) { ret = v.second; });
	return ret;
}

String GDREConfig::get_section_from_key(const String &p_setting) {
	if (!p_setting.contains_char('/')) {
		return "General";
	}
	return p_setting.get_slice("/", 0);
}

String GDREConfig::get_name_from_key(const String &p_setting) {
	auto parts = p_setting.split("/", true, 1);
	if (parts.size() == 1) {
		return p_setting;
	}
	return parts[1];
}

Variant GDREConfig::get_default_value(const String &p_setting) const {
	String full_name = get_full_name(p_setting);
	if (default_settings.has(full_name)) {
		return default_settings[full_name]->get_default_value();
	}
	return Variant();
}

void GDREConfig::reset_ephemeral_settings() {
	ephemeral_settings.clear();
	for (const auto &[key, setting] : default_settings) {
		if (setting->is_ephemeral()) {
			Variant default_value = setting->get_default_value();
			settings.if_contains(setting->get_full_name(), [&](const auto &v) { default_value = v.second; });
			ephemeral_settings.try_emplace_l(setting->get_full_name(), [=](auto &v) { v.second = default_value; }, default_value);
		}
	}
}

Dictionary GDREConfig::get_section_settings(const String &p_section) const {
	Dictionary ret;
	String section = p_section.trim_suffix("/") + "/";
	settings.for_each([&](const auto &v) {
		if (v.first.begins_with(section)) {
			ret[v.first.trim_prefix(section)] = v.second;
		}
	});
	return ret;
}

void GDREConfig::add_default_setting(Ref<GDREConfigSetting> p_setting) {
	if (default_settings.has(p_setting->get_full_name())) {
		WARN_PRINT("Setting already exists, overwriting: " + p_setting->get_full_name());
	}
	default_settings[p_setting->get_full_name()] = p_setting;
}

GDREConfigSetting::GDREConfigSetting(const String &p_full_name, const String &p_brief, const String &p_description, const Variant &p_default_value, GDREConfig::SettingFlag p_flags) {
	full_name = ::get_full_name(p_full_name);
	brief_description = p_brief;
	description = p_description;
	default_value = p_default_value;
	hidden = (p_flags & GDREConfig::SETTING_FLAG_HIDDEN) != 0;
	ephemeral = (p_flags & GDREConfig::SETTING_FLAG_EPHEMERAL) != 0;
	dirpicker = (p_flags & GDREConfig::SETTING_FLAG_DIRPICKER) != 0;
	filepicker = (p_flags & GDREConfig::SETTING_FLAG_FILEPICKER) != 0;
}

String GDREConfigSetting::get_full_name() const {
	return full_name;
}

String GDREConfigSetting::get_name() const {
	return full_name.get_file();
}

String GDREConfigSetting::get_brief_description() const {
	if (brief_description.is_empty()) {
		return get_name().replace("_", " ").capitalize();
	}
	return brief_description;
}

Variant::Type GDREConfigSetting::get_type() const {
	return default_value.get_type();
}

String GDREConfigSetting::get_description() const {
	return description;
}

Variant GDREConfigSetting::get_default_value() const {
	return default_value;
}

bool GDREConfigSetting::is_hidden() const {
	return hidden;
}

bool GDREConfigSetting::is_ephemeral() const {
	return ephemeral;
}

Variant GDREConfigSetting::get_value() const {
	return GDREConfig::get_singleton()->get_setting(full_name, default_value);
}

void GDREConfigSetting::reset() {
	GDREConfig::get_singleton()->set_setting(full_name, default_value, ephemeral);
}

void GDREConfigSetting::set_value(const Variant &p_value, bool p_force_ephemeral) {
	GDREConfig::get_singleton()->set_setting(full_name, p_value, p_force_ephemeral || ephemeral);
}

Variant GDREConfigSetting::get_min_value() const {
	switch (get_type()) {
		case Variant::Type::INT:
			return 0;
		case Variant::Type::FLOAT:
			return 0.0f;
		case Variant::Type::BOOL:
			return false;
		default:
			return Variant();
	}
}

Variant GDREConfigSetting::get_max_value() const {
	switch (get_type()) {
		case Variant::Type::INT:
			return INT_MAX;
		case Variant::Type::FLOAT:
			return 179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0;
		case Variant::Type::BOOL:
			return true;
		default:
			return Variant();
	}
}

Variant GDREConfigSetting::get_step_value() const {
	switch (get_type()) {
		case Variant::Type::INT:
			return 1;
		case Variant::Type::FLOAT:
			return 0.01f;
		case Variant::Type::BOOL:
			return 1;
		default:
			return Variant();
	}
}

bool GDREConfigSetting::is_filepicker() const {
	return filepicker;
}

bool GDREConfigSetting::is_dirpicker() const {
	return dirpicker;
}

GDREConfigSettingEnum::GDREConfigSettingEnum(
		const String &p_full_name,
		const String &p_brief,
		const String &p_description,
		const int &p_default_value,
		const String &p_enum_values,
		GDREConfig::SettingFlag p_flags) :
		GDREConfigSetting(p_full_name, p_brief, p_description, p_default_value, p_flags) {
	enum_values = p_enum_values.split(",");
}

bool GDREConfigSettingEnum::has_special_value() const {
	return true;
}

Dictionary GDREConfigSettingEnum::get_list_of_possible_values() const {
	Dictionary ret;
	for (int i = 0; i < enum_values.size(); i++) {
		ret[i] = enum_values[i];
	}
	return ret;
}

GDREConfigSettingEnumDict::GDREConfigSettingEnumDict(
		const String &p_full_name,
		const String &p_brief,
		const String &p_description,
		const Variant &p_default_value,
		const Dictionary &p_enum_values,
		GDREConfig::SettingFlag p_flags) :
		GDREConfigSetting(p_full_name, p_brief, p_description, p_default_value, p_flags), enum_values(p_enum_values) {
}

bool GDREConfigSettingEnumDict::has_special_value() const {
	return true;
}

Dictionary GDREConfigSettingEnumDict::get_list_of_possible_values() const {
	return enum_values;
}

GDREConfigSettingRange::GDREConfigSettingRange(
		const String &p_full_name,
		const String &p_brief,
		const String &p_description,
		const Variant &p_default_value,
		const String &p_range_string, GDREConfig::SettingFlag p_flags) :
		GDREConfigSetting(p_full_name, p_brief, p_description, p_default_value, p_flags) {
	auto parts = p_range_string.split(",");
	ERR_FAIL_COND_MSG(parts.size() != 3, "Invalid range string: " + p_range_string);
	if (p_default_value.get_type() == Variant::Type::INT) {
		min_value = parts[0].to_int();
		max_value = parts[1].to_int();
		step_value = parts[2].to_int();
	} else if (p_default_value.get_type() == Variant::Type::FLOAT) {
		min_value = parts[0].to_float();
		max_value = parts[1].to_float();
		step_value = parts[2].to_float();
	} else {
		ERR_FAIL_MSG("Invalid range string: " + p_range_string);
	}
}

Variant GDREConfigSettingRange::get_min_value() const {
	return min_value;
}

Variant GDREConfigSettingRange::get_max_value() const {
	return max_value;
}

Variant GDREConfigSettingRange::get_step_value() const {
	return step_value;
}

bool GDREConfigSettingRange::is_range_setting() const {
	return true;
}

GDREConfigSettingEnumSpecial::GDREConfigSettingEnumSpecial(
		const String &p_full_name,
		const String &p_brief,
		const String &p_description,
		const Variant &p_default_value,
		GetListOfPossibleValuesFunc p_get_list_of_possible_values, GDREConfig::SettingFlag p_flags) :
		GDREConfigSetting(p_full_name, p_brief, p_description, p_default_value, p_flags), get_list_of_possible_values_func(p_get_list_of_possible_values) {
}

bool GDREConfigSettingEnumSpecial::has_special_value() const {
	return true;
}

Dictionary GDREConfigSettingEnumSpecial::get_list_of_possible_values() const {
	return get_list_of_possible_values_func();
}

void GDREConfigSetting::_bind_methods() {
	ClassDB::bind_method(D_METHOD("reset"), &GDREConfigSetting::reset);
	ClassDB::bind_method(D_METHOD("set_value", "value", "force_ephemeral"), &GDREConfigSetting::set_value, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_description"), &GDREConfigSetting::get_description);
	ClassDB::bind_method(D_METHOD("get_value"), &GDREConfigSetting::get_value);
	ClassDB::bind_method(D_METHOD("get_default_value"), &GDREConfigSetting::get_default_value);
	ClassDB::bind_method(D_METHOD("get_name"), &GDREConfigSetting::get_name);
	ClassDB::bind_method(D_METHOD("get_full_name"), &GDREConfigSetting::get_full_name);
	ClassDB::bind_method(D_METHOD("get_brief_description"), &GDREConfigSetting::get_brief_description);
	ClassDB::bind_method(D_METHOD("get_type"), &GDREConfigSetting::get_type);
	ClassDB::bind_method(D_METHOD("is_hidden"), &GDREConfigSetting::is_hidden);
	ClassDB::bind_method(D_METHOD("is_ephemeral"), &GDREConfigSetting::is_ephemeral);
	ClassDB::bind_method(D_METHOD("is_filepicker"), &GDREConfigSetting::is_filepicker);
	ClassDB::bind_method(D_METHOD("is_dirpicker"), &GDREConfigSetting::is_dirpicker);
	ClassDB::bind_method(D_METHOD("is_virtual_setting"), &GDREConfigSetting::is_virtual_setting);
	ClassDB::bind_method(D_METHOD("is_range_setting"), &GDREConfigSetting::is_range_setting);
	ClassDB::bind_method(D_METHOD("get_min_value"), &GDREConfigSetting::get_min_value);
	ClassDB::bind_method(D_METHOD("get_max_value"), &GDREConfigSetting::get_max_value);
	ClassDB::bind_method(D_METHOD("get_step_value"), &GDREConfigSetting::get_step_value);
	ClassDB::bind_method(D_METHOD("get_error_message"), &GDREConfigSetting::get_error_message);
	ClassDB::bind_method(D_METHOD("clear_error_message"), &GDREConfigSetting::clear_error_message);
	ClassDB::bind_method(D_METHOD("has_special_value"), &GDREConfigSetting::has_special_value);
	ClassDB::bind_method(D_METHOD("get_list_of_possible_values"), &GDREConfigSetting::get_list_of_possible_values);
}

void GDREConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_all_settings"), &GDREConfig::get_all_settings);
	ClassDB::bind_method(D_METHOD("load_config"), &GDREConfig::load_config);
	ClassDB::bind_method(D_METHOD("save_config"), &GDREConfig::save_config);
	ClassDB::bind_method(D_METHOD("set_setting", "setting", "value", "force_ephemeral"), &GDREConfig::set_setting, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("has_setting", "setting"), &GDREConfig::has_setting);
	ClassDB::bind_method(D_METHOD("get_setting", "setting", "default_value"), &GDREConfig::get_setting, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_default_value", "setting"), &GDREConfig::get_default_value);
	ClassDB::bind_method(D_METHOD("reset_ephemeral_settings"), &GDREConfig::reset_ephemeral_settings);
}
