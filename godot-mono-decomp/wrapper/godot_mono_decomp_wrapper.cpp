#include "godot_mono_decomp_wrapper.h"

#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/templates/vector.h"

#include "utility/gd_parallel_queue.h"
#include "utility/gdre_settings.h"
#include "utility/task_manager.h"

#if !GODOT_MONO_DECOMP_DISABLED
#include "godot_mono_decomp.h"

Dictionary GodotMonoDecompWrapper::_get_settings_from_config() {
	Dictionary dict = GDREConfig::get_singleton()->get_section_settings("CSharp");
	Dictionary ret;
	for (const auto &[key, value] : dict) {
		String key_str = key;
		int idx = key_str.rfind("/");
		if (idx != -1) {
			key_str = key_str.substr(idx + 1);
		}
		ret[key_str] = value;
	}
	return ret;

}

Ref<GodotMonoDecompWrapper> GodotMonoDecompWrapper::create(const String &assembly_path, const Vector<String> &originalProjectFiles) {
	Ref<GodotMonoDecompWrapper> wrapper = memnew(GodotMonoDecompWrapper);
	Error err = wrapper->_load(assembly_path, originalProjectFiles, _get_settings_from_config());
	ERR_FAIL_COND_V_MSG(err != OK, Ref<GodotMonoDecompWrapper>(), "Failed to load assembly " + assembly_path + " (Not a valid .NET assembly?)");
	return wrapper;
}

Error GodotMonoDecompWrapper::_load(const String &p_assembly_path, const Vector<String> &p_original_project_files, const Dictionary &p_settings) {
	if (decompilerHandle != nullptr) {
		GodotMonoDecomp_FreeObjectHandle(decompilerHandle);
		decompilerHandle = nullptr;
	}
	CharString assembly_path_chrstr = p_assembly_path.utf8();
	const char *assembly_path_c = assembly_path_chrstr.get_data();

	String version_string = GDRESettings::get_singleton()->get_version_string();
	CharString godotVersionOverride_chrstr = version_string.is_empty() ? "" : version_string.utf8();
	const char *godotVersionOverride_c = version_string.is_empty() ? nullptr : godotVersionOverride_chrstr.get_data();

	const char **originalProjectFiles_c_array = new const char *[p_original_project_files.size()];
	Vector<CharString> originalProjectFiles_chrstrs;
	originalProjectFiles_chrstrs.resize(p_original_project_files.size());
	for (int i = 0; i < p_original_project_files.size(); i++) {
		// to keep them from being freed
		originalProjectFiles_chrstrs.write[i] = p_original_project_files[i].utf8();
		originalProjectFiles_c_array[i] = originalProjectFiles_chrstrs[i].get_data();
	}

	String settings_json = JSON::stringify(p_settings);
	CharString settings_json_chrstr = settings_json.utf8();
	const char *settings_json_c = settings_json_chrstr.get_data();

	auto new_decompiler_handle = GodotMonoDecomp_CreateGodotModuleDecompiler(
			assembly_path_c,
			originalProjectFiles_c_array,
			p_original_project_files.size(),
			godotVersionOverride_c,
			settings_json_c);
	delete[] originalProjectFiles_c_array;
	if (new_decompiler_handle == nullptr) {
		return ERR_CANT_CREATE;
	}
	this->decompilerHandle = new_decompiler_handle;
	this->assembly_path = p_assembly_path;
	this->originalProjectFiles = p_original_project_files;
	return OK;
}

struct DecompileModuleTaskData : public TaskRunnerStruct {
	String outputCSProjectPath;
	Vector<String> excludeFiles;
	Error err = OK;
	std::atomic<int> current_step = 0;
	int total_steps = 0;
	ParalellQueue<String> queue;
	String current_step_description = "Decompiling module...";
	bool cancelled = false;

	DecompileModuleTaskData(const String &p_outputCSProjectPath, const Vector<String> &p_excludeFiles, int p_total_steps) :
			outputCSProjectPath(p_outputCSProjectPath), excludeFiles(p_excludeFiles), total_steps(p_total_steps), queue(p_total_steps) {}

	int get_current_task_step_value() override {
		return current_step;
	}

	String get_current_task_step_description() override {
		// pop them all off until we get the current one
		while (queue.try_pop(current_step_description)) {
		}
		return current_step_description;
	}

	void cancel() override {
		cancelled = true;
	}

	int progress_callback(int p_current, int p_total, const char *p_description) {
		if (cancelled) {
			return 1;
		}
		current_step++;
		// compare and exchange if p_current is greater than current_step
		// current_step = p_current;
		// total_steps = p_total;
		String description = String::utf8(p_description);
		queue.push(description);
		return 0;
	}

	static int _progress_callback(void *p_userdata, int p_current, int p_total, const char *p_description) {
		DecompileModuleTaskData *taskData = (DecompileModuleTaskData *)p_userdata;
		return taskData->progress_callback(p_current, p_total, p_description);
	}

	void run(void *p_userdata) override {
		GodotMonoDecompWrapper *wrapper = (GodotMonoDecompWrapper *)p_userdata;
		if (wrapper->decompilerHandle == nullptr) {
			ERR_PRINT("Decompiler handle is null");
			err = ERR_CANT_CREATE;
			return;
		}
		CharString outputCSProjectPath_chrstr = outputCSProjectPath.utf8();
		const char *outputCSProjectPath_c = outputCSProjectPath_chrstr.get_data();
		Vector<CharString> excludeFiles_chrstrrs;
		excludeFiles_chrstrrs.resize(excludeFiles.size());
		const char **excludeFiles_c_array = new const char *[excludeFiles.size()];
		for (int i = 0; i < excludeFiles.size(); i++) {
			excludeFiles_chrstrrs.write[i] = excludeFiles[i].utf8();
			excludeFiles_c_array[i] = excludeFiles_chrstrrs[i].get_data();
		}

		int result = GodotMonoDecomp_DecompileModuleWithProgress(wrapper->decompilerHandle, outputCSProjectPath_c, excludeFiles_c_array, excludeFiles.size(), &DecompileModuleTaskData::_progress_callback, this);
		delete[] excludeFiles_c_array;
		if (result != 0 && !cancelled) {
			err = ERR_CANT_CREATE;
		}
	}

	virtual ~DecompileModuleTaskData() = default;
};

Error GodotMonoDecompWrapper::decompile_module(const String &outputCSProjectPath, const Vector<String> &excludeFiles) {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, ERR_CANT_CREATE, "Decompiler handle is null");
	int total_steps = GodotMonoDecomp_GetNumberOfFilesInFileMap(decompilerHandle);
	auto taskData = std::make_shared<DecompileModuleTaskData>(outputCSProjectPath, excludeFiles, total_steps);
	TaskManager::get_singleton()->run_task(taskData, this, "Decompiling C# scripts...", total_steps, true, true);
	return taskData->err;
}

String GodotMonoDecompWrapper::decompile_individual_file(const String &file) {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, "", "Decompiler handle is null");
	CharString file_chrstr = file.utf8();
	const char *file_c = file_chrstr.get_data();
	const char *result = GodotMonoDecomp_DecompileIndividualFile(decompilerHandle, file_c);
	ERR_FAIL_COND_V_MSG(result == nullptr, "", "Failed to decompile individual file");
	String result_str = String::utf8(result);
	GodotMonoDecomp_FreeString((void *)result);
	return result_str;
}

Dictionary GodotMonoDecompWrapper::get_script_info(const String &file) {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, Dictionary(), "Decompiler handle is null");
	CharString file_chrstr = file.utf8();
	const char *file_c = file_chrstr.get_data();
	const char *result = GodotMonoDecomp_GetScriptInfo(decompilerHandle, file_c);
	if (result == nullptr) {
		return Dictionary();
	}
	String result_str = String::utf8(result);
	GodotMonoDecomp_FreeString((void *)result);
	Dictionary dict = JSON::parse_string(result_str);
	return dict;
}

Vector<String> GodotMonoDecompWrapper::get_files_not_present_in_file_map() {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, Vector<String>(), "Decompiler handle is null");
	int num = GodotMonoDecomp_GetNumberOfFilesNotPresentInFileMap(decompilerHandle);
	if (num == 0) {
		return Vector<String>();
	}
	ERR_FAIL_COND_V_MSG(num < 0, Vector<String>(), "Failed to get number of files not present in file map");
	const char **files_not_present_in_file_map_c_array = GodotMonoDecomp_GetFilesNotPresentInFileMap(decompilerHandle);
	Vector<String> files_not_present_in_file_map_strs;
	for (int i = 0; i < num; i++) {
		files_not_present_in_file_map_strs.push_back("res://" + String::utf8(files_not_present_in_file_map_c_array[i]).trim_prefix("res://"));
	}
	GodotMonoDecomp_FreeArray((void *)files_not_present_in_file_map_c_array, num);
	return files_not_present_in_file_map_strs;
}

Vector<String> GodotMonoDecompWrapper::get_files_in_file_map() {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, Vector<String>(), "Decompiler handle is null");
	int num = GodotMonoDecomp_GetNumberOfFilesInFileMap(decompilerHandle);
	if (num == 0) {
		return Vector<String>();
	}
	ERR_FAIL_COND_V_MSG(num < 0, Vector<String>(), "Failed to get number of files in file map");
	const char **files_in_file_map_c_array = GodotMonoDecomp_GetFilesInFileMap(decompilerHandle);
	Vector<String> files_in_file_map_strs;
	for (int i = 0; i < num; i++) {
		files_in_file_map_strs.push_back("res://" + String::utf8(files_in_file_map_c_array[i]).trim_prefix("res://"));
	}
	GodotMonoDecomp_FreeArray((void *)files_in_file_map_c_array, num);
	return files_in_file_map_strs;
}

bool GodotMonoDecompWrapper::is_custom_version_detected() const {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, false, "Decompiler handle is null");
	return GodotMonoDecomp_IsCustomVersionDetected(decompilerHandle) != 0;
}

Vector<String> GodotMonoDecompWrapper::get_all_strings_in_module() {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, Vector<String>(), "Decompiler handle is null");
	int num_strings = 0;
	char32_t **strings = GodotMonoDecomp_GetAllUtf32StringsInModule(decompilerHandle, &num_strings);
	Vector<String> strings_strs;
	for (int i = 0; i < num_strings; i++) {
		strings_strs.push_back(String(strings[i]));
	}
	GodotMonoDecomp_FreeArray((void *)strings, num_strings);
	return strings_strs;
}

bool GodotMonoDecompWrapper::is_handle_valid() const {
	return decompilerHandle != nullptr;
}

static inline void _ensure_settings_int(Dictionary &p_dict){
	for (const auto &[key, value] : p_dict) {
		if (value.get_type() == Variant::Type::FLOAT) {
			p_dict[key] = (int)round(value.operator double());
		} else if (value.get_type() == Variant::Type::ARRAY) {
			TypedArray<String> array = value;
			PackedStringArray packed_array;
			for (int i = 0; i < array.size(); i++) {
				packed_array.push_back(array[i]);
			}
			p_dict[key] = packed_array;
		} else if (value.get_type() == Variant::Type::DICTIONARY) {
			Dictionary dict = value;
			_ensure_settings_int(dict);
			p_dict[key] = dict;
		}
	}
}

Dictionary GodotMonoDecompWrapper::get_settings() const {
	const char *json = GodotMonoDecomp_GetSettingsJSON(decompilerHandle);
	String result = String::utf8(json);
	GodotMonoDecomp_FreeString((void *)json);
	Dictionary dict = JSON::parse_string(result);
	_ensure_settings_int(dict);
	return dict;
}

bool GodotMonoDecompWrapper::reload_settings() {
	Dictionary settings = get_settings();
	Dictionary new_settings = _get_settings_from_config();
	bool changed = false;
	for (const auto &[key, value] : new_settings) {
		if (settings.has(key) && settings[key] != value) {
			if (settings[key].get_type() == Variant::Type::NIL && value.get_type() == Variant::Type::INT && value.operator int() == 0){
				continue;
			}
			changed = true;
			settings[key] = value;
		}
	}
	if (changed) {
		Error err = _load(assembly_path, originalProjectFiles, settings);
		ERR_FAIL_COND_V_MSG(err != OK, false, "Failed to reload assembly " + assembly_path + " (Not a valid .NET assembly?)");
	}
	return changed;
}

bool GodotMonoDecompWrapper::is_file_assembly(const String &file) {
	CharString file_chrstr = file.utf8();
	const char *file_c = file_chrstr.get_data();
	return GodotMonoDecomp_CheckFileIsAssembly(file_c) != 0;
}

Array GodotMonoDecompWrapper::get_default_settings() {
	const char *json = GodotMonoDecomp_GetDefaultSettingsJSON();
	String result = String::utf8(json);
	GodotMonoDecomp_FreeString((void *)json);
	Array dict = JSON::parse_string(result);
	return dict;
}

static inline void _add_setting(Dictionary dict){
	String name = "CSharp/" + dict.get("name", "").operator String();
	String brief = dict.get("brief", "").operator String();
	String description = dict.get("description", "").operator String();
	Variant default_value = dict.get("default_value", 0);
	if (default_value.get_type() == Variant::Type::FLOAT) {
		default_value = (int)round(default_value.operator double());
	} else if (default_value.get_type() == Variant::Type::ARRAY) {
		TypedArray<String> array = default_value;
		PackedStringArray packed_array;
		for (int i = 0; i < array.size(); i++) {
			packed_array.push_back(array[i]);
		}
		default_value = packed_array;
	}
	Dictionary enum_values = dict.get("enum_values", Dictionary());
	if (enum_values.size() > 0) {
		Dictionary fixed_enum_values;
		for (auto &[key, value] : enum_values) {
			String key_str = key;
			if (key_str.begins_with("CSharp")){
				key_str = "C# " + key_str.trim_prefix("CSharp").replace("_", ".");
			}
			if (value.get_type() == Variant::Type::FLOAT) {
				fixed_enum_values[(int)round(value.operator double())] = key_str;
			} else {
				fixed_enum_values[value] = key_str;
			}
		}
		enum_values = fixed_enum_values;
		CONFIG_DEF_ENUM_DICT(name, brief, description, default_value, enum_values);
	} else {
		CONFIG_DEF(name, brief, description, default_value);
	}

}

void GodotMonoDecompWrapper::add_default_config_settings() {

	auto default_settings = get_default_settings();
	Vector<Dictionary> ILSpySettings;
	for (int i = 0; i < default_settings.size(); i++) {
		Dictionary dict = default_settings[i];
		if (dict.get("name", "").operator String().begins_with("ILSpy/")) {
			ILSpySettings.push_back(dict);
			continue;
		}
		_add_setting(dict);
	}

	CONFIG_DEF(
		"CSharp/compile_after_decompile",
		"Compile after decompile",
		"Compile the C# project after decompiling.\nThis is done to prevent editor errors when first opening the project in the editor.\nThis requires that you have the dotnet sdk installed.",
		true);

	struct _ILSpySettingsSort {
		static inline String get_prefix(const String &name){
			int idx = name.rfind("/");
			if (idx == -1) {
				return name;
			}
			return name.substr(0, idx);
		}
		bool operator()(const Dictionary &a, const Dictionary &b) const {
			return get_prefix(a.get("name", "")) < get_prefix(b.get("name", ""));
		}
	};
	ILSpySettings.sort_custom<_ILSpySettingsSort>();

	for (int i = 0; i < ILSpySettings.size(); i++) {
		Dictionary dict = ILSpySettings[i];
		_add_setting(dict);
	}

}

Vector<String> GodotMonoDecompWrapper::get_global_class_files() {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, Vector<String>(), "Decompiler handle is null");
	int num_files = 0;
	const char **files = GodotMonoDecomp_GetGlobalClassFiles(decompilerHandle, &num_files);
	Vector<String> files_strs;
	for (int i = 0; i < num_files; i++) {
		files_strs.push_back("res://" + String::utf8(files[i]).trim_prefix("res://"));
	}
	GodotMonoDecomp_FreeArray((void *)files, num_files);
	return files_strs;
}

GodotMonoDecompWrapper::~GodotMonoDecompWrapper() {
	if (decompilerHandle != nullptr) {
		GodotMonoDecomp_FreeObjectHandle(decompilerHandle);
	}
}
#else
constexpr const char *GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE = "GodotMonoDecompWrapper is not enabled in this build of GDRE Tools";
Ref<GodotMonoDecompWrapper> GodotMonoDecompWrapper::create(const String &assembly_path, const Vector<String> &originalProjectFiles, const GodotMonoDecompSettings &settings) {
	ERR_FAIL_V_MSG(Ref<GodotMonoDecompWrapper>(), GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
Error GodotMonoDecompWrapper::_load(const String &p_assembly_path, const Vector<String> &p_original_project_files, const GodotMonoDecompSettings &p_settings) {
	ERR_FAIL_V_MSG(ERR_UNAVAILABLE, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
Error GodotMonoDecompWrapper::decompile_module(const String &outputCSProjectPath, const Vector<String> &excludeFiles) {
	ERR_FAIL_V_MSG(ERR_UNAVAILABLE, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
String GodotMonoDecompWrapper::decompile_individual_file(const String &file) {
	ERR_FAIL_V_MSG({}, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
Dictionary GodotMonoDecompWrapper::get_script_info(const String &file) {
	ERR_FAIL_V_MSG({}, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
Vector<String> GodotMonoDecompWrapper::get_files_not_present_in_file_map() {
	ERR_FAIL_V_MSG({}, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
Vector<String> GodotMonoDecompWrapper::get_files_in_file_map() {
	ERR_FAIL_V_MSG({}, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
bool GodotMonoDecompWrapper::is_custom_version_detected() const {
	ERR_FAIL_V_MSG(false, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
Vector<String> GodotMonoDecompWrapper::get_all_strings_in_module() {
	ERR_FAIL_V_MSG({}, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
Error GodotMonoDecompWrapper::set_settings(const GodotMonoDecompSettings &p_settings) {
	ERR_FAIL_V_MSG(ERR_UNAVAILABLE, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
bool GodotMonoDecompWrapper::is_file_assembly(const String &file) const {
	ERR_FAIL_V_MSG(false, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
Vector<String> GodotMonoDecompWrapper::get_global_class_files() {
	ERR_FAIL_V_MSG({}, GODOT_MONO_DECOMP_DISABLED_ERROR_MESSAGE);
}
GodotMonoDecompWrapper::~GodotMonoDecompWrapper() {}
#endif

GodotMonoDecompWrapper::GodotMonoDecompWrapper() {}

void GodotMonoDecompWrapper::_bind_methods() {
	ClassDB::bind_static_method("GodotMonoDecompWrapper", D_METHOD("is_godot_mono_decomp_enabled"), &GodotMonoDecompWrapper::is_godot_mono_decomp_enabled);
	ClassDB::bind_method(D_METHOD("decompile_module", "outputCSProjectPath", "excludeFiles"), &GodotMonoDecompWrapper::decompile_module, DEFVAL(Vector<String>()));
	ClassDB::bind_method(D_METHOD("decompile_individual_file", "file"), &GodotMonoDecompWrapper::decompile_individual_file);
	ClassDB::bind_method(D_METHOD("get_script_info", "file"), &GodotMonoDecompWrapper::get_script_info);
	ClassDB::bind_method(D_METHOD("get_files_not_present_in_file_map"), &GodotMonoDecompWrapper::get_files_not_present_in_file_map);
	ClassDB::bind_method(D_METHOD("get_files_in_file_map"), &GodotMonoDecompWrapper::get_files_in_file_map);
	ClassDB::bind_method(D_METHOD("is_custom_version_detected"), &GodotMonoDecompWrapper::is_custom_version_detected);
	ClassDB::bind_method(D_METHOD("get_global_class_files"), &GodotMonoDecompWrapper::get_global_class_files);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("is_file_assembly", "file"), &GodotMonoDecompWrapper::is_file_assembly);
}
