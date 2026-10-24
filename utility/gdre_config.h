#pragma once

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/typed_array.h"
#include "gd_parallel_hashmap.h"

class GDREConfig;
class GDREConfigSetting;

class GDREConfig : public Object {
	GDCLASS(GDREConfig, Object);

	ParallelFlatHashMap<String, Variant> settings;
	static GDREConfig *singleton;
	HashMap<String, Ref<GDREConfigSetting>> default_settings;
	ParallelFlatHashMap<String, Variant> ephemeral_settings;

	static void _init_default_settings();

	static String get_config_path();

public:
	enum SettingFlag {
		SETTING_FLAG_NONE = 0,
		SETTING_FLAG_HIDDEN = 1 << 0,
		SETTING_FLAG_EPHEMERAL = 1 << 1,
		SETTING_FLAG_DIRPICKER = 1 << 2,
		SETTING_FLAG_FILEPICKER = 1 << 3,
	};

	static GDREConfig *get_singleton();

	GDREConfig();
	~GDREConfig();

	void load_config();
	void save_config();
	void set_setting(const String &p_setting, const Variant &p_value, bool p_force_ephemeral = false);
	bool has_setting(const String &string) const;
	static String get_section_from_key(const String &p_setting);
	static String get_name_from_key(const String &p_setting);
	Variant get_setting(const String &p_setting, const Variant &p_default_value = Variant()) const;
	Variant get_default_value(const String &p_setting) const;
	TypedArray<GDREConfigSetting> get_all_settings() const;
	bool is_ephemeral_setting(const String &p_setting) const;
	void reset_ephemeral_settings();

	void add_default_setting(Ref<GDREConfigSetting> p_setting);

protected:
	static void _bind_methods();
};

// this is mainly used by the GUI
class GDREConfigSetting : public RefCounted {
	GDCLASS(GDREConfigSetting, RefCounted);
	friend class GDREConfig;

protected:
	String full_name;
	String brief_description;
	String description;
	Variant default_value;
	bool hidden = false;
	bool ephemeral = false;
	bool dirpicker = false;
	bool filepicker = false;

public:
	String get_full_name() const;
	String get_name() const;
	String get_section() const;
	String get_brief_description() const;
	String get_description() const;
	virtual Variant get_value() const;
	Variant get_default_value() const;
	bool is_hidden() const;
	Variant::Type get_type() const;
	bool is_ephemeral() const;
	virtual bool is_range_setting() const { return false; }
	virtual Variant get_min_value() const;
	virtual Variant get_max_value() const;
	virtual Variant get_step_value() const;
	virtual bool is_virtual_setting() const { return false; }
	bool is_filepicker() const;
	bool is_dirpicker() const;
	virtual String get_error_message() const { return ""; }
	virtual void clear_error_message() {}

	virtual bool has_special_value() const { return false; }
	// get a list of possible values along with their descriptions
	virtual Dictionary get_list_of_possible_values() const { return Dictionary(); }

	// this calls GDREConfig::set_setting
	void reset();
	// this calls GDREConfig::set_setting
	virtual void set_value(const Variant &p_value, bool p_force_ephemeral = false);
	GDREConfigSetting(const String &p_full_name, const String &p_brief, const String &p_description, const Variant &p_default_value, GDREConfig::SettingFlag p_flags = GDREConfig::SETTING_FLAG_NONE);

protected:
	static void _bind_methods();
	// Don't call this, this is just to make the class_db happy
	GDREConfigSetting() {}
};

class GDREConfigSettingEnum : public GDREConfigSetting {
	GDSOFTCLASS(GDREConfigSettingEnum, GDREConfigSetting);

	Vector<String> enum_values;

public:
	GDREConfigSettingEnum(
			const String &p_full_name,
			const String &p_brief,
			const String &p_description,
			const int &p_default_value,
			const String &p_enum_values,
			GDREConfig::SettingFlag p_flags = GDREConfig::SETTING_FLAG_NONE);
	virtual bool has_special_value() const override;
	virtual Dictionary get_list_of_possible_values() const override;
};

class GDREConfigSettingRange : public GDREConfigSetting {
	GDSOFTCLASS(GDREConfigSettingRange, GDREConfigSetting);

	Variant min_value;
	Variant max_value;
	Variant step_value;

public:
	GDREConfigSettingRange(
			const String &p_full_name,
			const String &p_brief,
			const String &p_description,
			const Variant &p_default_value,
			const String &p_range_string, GDREConfig::SettingFlag p_flags = GDREConfig::SETTING_FLAG_NONE);
	virtual Variant get_min_value() const override;
	virtual Variant get_max_value() const override;
	virtual Variant get_step_value() const override;
	virtual bool is_range_setting() const override;
};

class GDREConfigSettingEnumSpecial : public GDREConfigSetting {
	GDSOFTCLASS(GDREConfigSettingEnumSpecial, GDREConfigSetting);

public:
	typedef Dictionary (*GetListOfPossibleValuesFunc)();
	GDREConfigSettingEnumSpecial(
			const String &p_full_name,
			const String &p_brief,
			const String &p_description,
			const Variant &p_default_value,
			GetListOfPossibleValuesFunc p_get_list_of_possible_values, GDREConfig::SettingFlag p_flags = GDREConfig::SETTING_FLAG_NONE);

	virtual bool has_special_value() const override;
	virtual Dictionary get_list_of_possible_values() const override;

private:
	GetListOfPossibleValuesFunc get_list_of_possible_values_func;
};

#define CONFIG_GET(p_setting, p_default_value) GDREConfig::get_singleton()->get_setting(p_setting, p_default_value)

static inline void CONFIG_DEF(const String &p_setting, const String &p_brief, const String &p_description, const Variant &p_default_value, GDREConfig::SettingFlag p_flags = GDREConfig::SETTING_FLAG_NONE) {
	GDREConfig::get_singleton()->add_default_setting(memnew(GDREConfigSetting(p_setting, p_brief, p_description, p_default_value, p_flags)));
}

static inline void CONFIG_DEF_RANGE(const String &p_setting, const String &p_brief, const String &p_description, const Variant &p_default_value, const String &p_range_string, GDREConfig::SettingFlag p_flags = GDREConfig::SETTING_FLAG_NONE) {
	GDREConfig::get_singleton()->add_default_setting(memnew(GDREConfigSettingRange(p_setting, p_brief, p_description, p_default_value, p_range_string, p_flags)));
}

static inline void CONFIG_DEF_ENUM(const String &p_setting, const String &p_brief, const String &p_description, const Variant &p_default_value, const String &p_enum_values, GDREConfig::SettingFlag p_flags = GDREConfig::SETTING_FLAG_NONE) {
	GDREConfig::get_singleton()->add_default_setting(memnew(GDREConfigSettingEnum(p_setting, p_brief, p_description, p_default_value, p_enum_values, p_flags)));
}

static inline void CONFIG_DEF_ENUM_SPECIAL(const String &p_setting, const String &p_brief, const String &p_description, const Variant &p_default_value, GDREConfigSettingEnumSpecial::GetListOfPossibleValuesFunc p_get_list_of_possible_values, GDREConfig::SettingFlag p_flags = GDREConfig::SETTING_FLAG_NONE) {
	GDREConfig::get_singleton()->add_default_setting(memnew(GDREConfigSettingEnumSpecial(p_setting, p_brief, p_description, p_default_value, p_get_list_of_possible_values, p_flags)));
}

static inline void CONFIG_DEF_CUSTOM(Ref<GDREConfigSetting> p_setting) {
	GDREConfig::get_singleton()->add_default_setting(p_setting);
}
