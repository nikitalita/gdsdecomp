#include "register_types.h"
#include "core/object/class_db.h"

#include "bytecode/bytecode_base.h"
#include "bytecode/bytecode_versions.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "utility/gdre_config.h"
#include "utility/gdre_logger.h"
#include "utility/gdre_settings.h"

class GDREConfigSetting_LoadCustomBytecode : public GDREConfigSetting {
	GDSOFTCLASS(GDREConfigSetting_LoadCustomBytecode, GDREConfigSetting);

	String error_message;

public:
	GDREConfigSetting_LoadCustomBytecode() :
			GDREConfigSetting(
					"Bytecode/load_custom_bytecode",
					"Load Custom Bytecode",
					"Load a custom bytecode file.",
					"",
					GDREConfig::SETTING_FLAG_FILEPICKER) {
	}

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
		if (!FileAccess::exists(path)) {
			WARN_PRINT("Custom bytecode file does not exist: " + path);
			error_message = "Custom bytecode file does not exist";
			return;
		}
		String file_contents = FileAccess::get_file_as_string(path);
		if (file_contents.is_empty()) {
			WARN_PRINT("Custom bytecode file is empty: " + path);
			error_message = "Custom bytecode file is empty";
			return;
		}
		Dictionary json = JSON::parse_string(file_contents);
		if (json.is_empty()) {
			WARN_PRINT("Custom bytecode file is not valid JSON: " + path);
			error_message = "Custom bytecode file is not valid JSON";
			return;
		}

		// clears errors
		GDRELogger::clear_error_queues();
		int commit = GDScriptDecomp::register_decomp_version_custom(json);
		if (commit == 0) {
			WARN_PRINT("Failed to register custom bytecode file: " + path);
			error_message = "Failed to register custom bytecode file: \n" + String("\n").join(GDRELogger::get_errors());
			return;
		}
		GDREConfig::get_singleton()->set_setting("Bytecode/force_bytecode_revision", commit, true);
		error_message = "";
	}
};

static Dictionary get_list_of_bytecode_override_values() {
	Dictionary ret;
	int ver_major = 0;
	if (GDRESettings::get_singleton() && GDRESettings::get_singleton()->is_pack_loaded()) {
		ver_major = GDRESettings::get_singleton()->get_ver_major();
	}
	auto versions = GDScriptDecompVersion::get_decomp_versions(true, 0);
	ret[0] = "Auto-detect";
	for (const auto &version : versions) {
		if ((ver_major > 0 && version.get_major_version() != ver_major)) {
			if (version.commit != (int)CONFIG_GET("Bytecode/force_bytecode_revision", 0)) {
				continue;
			}
		}
		String short_name = version.name.split("/")[0].strip_edges() + ")";
		ret[version.commit] = short_name;
	}
	return ret;
}

static void initialize_gdsdecomp_bytecode_options() {
	CONFIG_DEF_ENUM_SPECIAL("Bytecode/force_bytecode_revision",
			"Force Bytecode Revision",
			"Forces the bytecode revision to be the specified value.",
			0,
			&get_list_of_bytecode_override_values,
			GDREConfig::SETTING_FLAG_NONE);
	CONFIG_DEF_CUSTOM(Ref<GDREConfigSetting>(memnew(GDREConfigSetting_LoadCustomBytecode)));
}

void initialize_gdsdecomp_bytecode_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_abstract_class<GDScriptDecomp>();
	register_decomp_versions();
	initialize_gdsdecomp_bytecode_options();
}

void uninitialize_gdsdecomp_bytecode_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
