#include "fake_csharp_script.h"

#include "compat/fake_script.h"
#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "godot-mono-decomp/wrapper/csharp_script_info.h"
#include "godot-mono-decomp/wrapper/godot_mono_decomp_wrapper.h"
#include "utility/resource_info.h"
#include <utility/gdre_settings.h>

#define FAKECSHARPSCRIPT_FAIL_COND_V_MSG(cond, val, msg) \
	if (unlikely(cond)) {                                \
		error_message = msg;                             \
		ERR_FAIL_V_MSG(val, msg);                        \
	}

#define FAKECSHARPSCRIPT_FAIL_V_MSG(val, msg) \
	error_message = msg;                      \
	ERR_FAIL_V_MSG(val, msg);

#define FAKECSHARPSCRIPT_FAIL_COND_MSG(cond, msg) \
	if (unlikely(cond)) {                         \
		error_message = msg;                      \
		ERR_FAIL_MSG(msg);                        \
	}

Error FakeCSharpScript::_reload_from_file() {
	error_message.clear();
	script_info.clear();
	source.clear();
	valid = false;
	loaded = false;
	if (!GDRESettings::get_singleton()->has_loaded_dotnet_assembly()) {
		error_message = "No dotnet assembly loaded";
		return ERR_CANT_RESOLVE;
	}
	auto decompiler = GDRESettings::get_singleton()->get_dotnet_decompiler();
	if (decompiler.is_null()) {
		error_message = "No dotnet decompiler loaded";
		return ERR_CANT_RESOLVE;
	}
	auto script_info_dict = decompiler->get_script_info(script_path);
	if (script_info_dict.is_empty()) {
		error_message = "No script info found";
		return ERR_CANT_ACQUIRE_RESOURCE;
	}
	script_info = CSharpScriptInfo::from_dict(script_info_dict);
	source = script_info.script_text;
	if (source.is_empty()) {
		error_message = "Failed to decompile script";
		return ERR_CANT_ACQUIRE_RESOURCE;
	}

	return reload(false);
}

void FakeCSharpScript::reload_from_file() {
	Error err = _reload_from_file();
	ERR_FAIL_COND_MSG(err != OK, vformat("Error reloading script %s: %s", script_path, error_message));
}

Ref<Script> FakeCSharpScript::get_base_script() const {
	if (base.is_null()) {
		base = load_base_script();
	}
	return base;
}

StringName FakeCSharpScript::get_global_name() const {
	return global_name;
}

bool FakeCSharpScript::inherits_script(const Ref<Script> &p_script) const {
	// TODO: Implement proper inheritance checking
	return true;
}

StringName FakeCSharpScript::get_instance_base_type() const {
	auto s = get_base_script();
	if (s.is_valid()) {
		return s->get_instance_base_type();
	}
	return base_type;
}

ScriptInstance *FakeCSharpScript::instance_create(Object *p_this) {
	auto instance = memnew(FakeScriptInstance());
	instance->script = Ref<FakeCSharpScript>(this);
	instance->owner = p_this;
	instance->update_cached_prop_names();
	return instance;
}

PlaceHolderScriptInstance *FakeCSharpScript::placeholder_instance_create(Object *p_this) {
	PlaceHolderScriptInstance *si = memnew(PlaceHolderScriptInstance(/*CSharpLanguage::get_singleton()*/ nullptr, Ref<Script>(this), p_this));
	return si;
}

Ref<Script> FakeCSharpScript::load_base_script() const {
	if (base_type_paths.size() > 0 && !base_type_paths[0].is_empty()) {
		return ResourceCompatLoader::custom_load(base_type_paths[0], "", load_type);
	}
	return Ref<Script>();
}

Error FakeCSharpScript::reload(bool p_keep_state) {
	error_message.clear();
	valid = false;
	loaded = false;

	if (script_info.path.is_empty()) {
		error_message = "Script info is empty";
		return ERR_INVALID_DATA;
	}

	// String ns = script_info.get("namespace", "");
	local_name = script_info.class_name;
	base_classes = script_info.base_classes;
	base_type_paths = script_info.base_type_paths;
	icon_path = script_info.icon_path;
	globally_available = script_info.is_global_class;
	global_name = globally_available ? local_name : "";
	base_type = script_info.godot_sharp_base_type;
	tool = script_info.is_tool;
	abstract = script_info.is_abstract;

	members.clear();
	_signals.clear();
	_methods.clear();
	for (auto &property : script_info.properties) {
		if (property.info.name.is_empty()) {
			continue;
		}
		members.insert(property.info.name, property.info);
		member_default_values.insert(property.info.name, property.default_value);
	}

	for (auto &signal : script_info.signals) {
		if (signal.name.is_empty()) {
			continue;
		}
		_signals.insert(signal.name, signal);
	}

	for (auto &method : script_info.signals) {
		if (method.name.is_empty()) {
			continue;
		}
		_methods.insert(method.name, method);
	}

	bool is_real_load = get_load_type() == ResourceInfo::LoadType::REAL_LOAD || get_load_type() == ResourceInfo::LoadType::GLTF_LOAD;
	if (base.is_null() && is_real_load) {
		base = load_base_script();
	}

	valid = true;
	loaded = true;

	return OK;
}

#ifdef TOOLS_ENABLED
StringName FakeCSharpScript::get_doc_class_name() const {
	return globally_available ? global_name : local_name;
}

Vector<DocData::ClassDoc> FakeCSharpScript::get_documentation() const {
	return Vector<DocData::ClassDoc>();
}

String FakeCSharpScript::get_class_icon_path() const {
	return "";
}

PropertyInfo FakeCSharpScript::get_class_category() const {
	return PropertyInfo();
}
#endif // TOOLS_ENABLED

bool FakeCSharpScript::has_method(const StringName &p_method) const {
	return _methods.has(p_method);
}

bool FakeCSharpScript::has_static_method(const StringName &p_method) const {
	return _methods.has(p_method) && _methods[p_method].flags & METHOD_FLAG_STATIC;
}

int FakeCSharpScript::get_script_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	if (_methods.has(p_method)) {
		if (r_is_valid) {
			*r_is_valid = true;
		}
		return _methods[p_method].arguments.size();
	}
	if (r_is_valid) {
		*r_is_valid = false;
	}
	return 0;
}

MethodInfo FakeCSharpScript::get_method_info(const StringName &p_method) const {
	if (_methods.has(p_method)) {
		return _methods[p_method];
	}
	return MethodInfo();
}

bool FakeCSharpScript::is_tool() const {
	return tool;
}

bool FakeCSharpScript::is_script_valid() const {
	return valid;
}

bool FakeCSharpScript::is_abstract() const {
	return abstract;
}

ScriptLanguage *FakeCSharpScript::get_language() const {
	// For now, return nullptr as we don't have a C# script language implementation
	return nullptr;
}

bool FakeCSharpScript::has_script_signal(const StringName &p_signal) const {
	if (_signals.has(p_signal)) {
		return true;
	}
	auto parent = get_base_script();
	if (parent.is_valid()) {
		return parent->has_script_signal(p_signal);
	}
	return false;
}

void FakeCSharpScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	for (const KeyValue<StringName, MethodInfo> &E : _signals) {
		r_signals->push_back(E.value);
	}
	auto parent_script = get_base_script();
	if (parent_script.is_valid()) {
		parent_script->get_script_signal_list(r_signals);
	}
}

bool FakeCSharpScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	if (member_default_values.has(p_property)) {
		r_value = member_default_values[p_property];
		return true;
	}
	if (!members.has(p_property)) {
		auto parent_script = get_base_script();
		if (parent_script.is_valid()) {
			return parent_script->get_property_default_value(p_property, r_value);
		}
		return false;
	}
	r_value = Variant();
	return true;
}

void FakeCSharpScript::update_exports() {
	// For now, do nothing
}

void FakeCSharpScript::get_script_method_list(List<MethodInfo> *p_list) const {
	for (auto &E : _methods) {
		p_list->push_back(E.value);
	}
	auto parent_script = get_base_script();
	if (parent_script.is_valid()) {
		parent_script->get_script_method_list(p_list);
	}
}

void FakeCSharpScript::get_script_property_list(List<PropertyInfo> *p_list) const {
	// TODO: Parse types, default values, etc.

	for (const auto &E : members) {
		p_list->push_back(E.value);
	}
	auto parent_script = get_base_script();
	if (parent_script.is_valid()) {
		parent_script->get_script_property_list(p_list);
	}
}

int FakeCSharpScript::get_member_line(const StringName &p_member) const {
	// For now, return -1 as we don't track line numbers
	return -1;
}

void FakeCSharpScript::get_constants(HashMap<StringName, Variant> *p_constants) {
	// For now, do nothing as we don't parse C# constants
}

void FakeCSharpScript::get_members(HashSet<StringName> *p_members) {
	for (auto &E : members) {
		p_members->insert(E.key);
	}
}

bool FakeCSharpScript::is_placeholder_fallback_enabled() const {
	return false;
}

const Variant FakeCSharpScript::get_rpc_config() const {
	return Variant();
}

String FakeCSharpScript::get_script_path() const {
	return script_path;
}

Error FakeCSharpScript::load_source_code(const String &p_path) {
	script_path = p_path;
	path_valid = true;
	return _reload_from_file();
}

bool FakeCSharpScript::is_global_class() const {
	return globally_available;
}

String FakeCSharpScript::get_icon_path() const {
	return icon_path;
}

void FakeCSharpScript::set_autoload(bool p_autoload) {
	autoload = p_autoload;
}

bool FakeCSharpScript::is_autoload() const {
	return autoload;
}

bool FakeCSharpScript::is_loaded() const {
	return loaded;
}

Variant FakeCSharpScript::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	return {};
}

FakeCSharpScript::FakeCSharpScript() {
	original_class = "CSharpScript";
	set_instance_recording_properties(true);
}

void FakeCSharpScript::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_autoload", "autoload"), &FakeCSharpScript::set_autoload);
	ClassDB::bind_method(D_METHOD("is_autoload"), &FakeCSharpScript::is_autoload);
}
