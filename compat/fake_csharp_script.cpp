#include "fake_csharp_script.h"

#include "compat/resource_loader_compat.h"
#include "compat/variant_decoder_compat.h"
#include "core/error/error_list.h"
#include "core/io/missing_resource.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
#include "modules/regex/regex.h"
#include "utility/resource_info.h"
#include <utility/gdre_settings.h>
#include "utility/godot_mono_decomp_wrapper.h"
#include "compat/fake_script.h"
#include "compat/variant_writer_compat.h"
#include "core/math/expression.h"

#define FAKECSHARPSCRIPT_FAIL_COND_V_MSG(cond, val, msg) \
	if (unlikely(cond)) {                                 \
		error_message = msg;                              \
		ERR_FAIL_V_MSG(val, msg);                         \
	}

#define FAKECSHARPSCRIPT_FAIL_V_MSG(val, msg) \
	error_message = msg;                       \
	ERR_FAIL_V_MSG(val, msg);

#define FAKECSHARPSCRIPT_FAIL_COND_MSG(cond, msg) \
	if (unlikely(cond)) {                         \
		error_message = msg;                      \
		ERR_FAIL_MSG(msg);                        \
	}

Error FakeCSharpScript::_reload_from_file() {
	error_message.clear();
	source.clear();
	if (!GDRESettings::get_singleton()->has_loaded_dotnet_assembly()) {
		error_message = "No dotnet assembly loaded";
		return ERR_CANT_RESOLVE;
	}
	auto decompiler = GDRESettings::get_singleton()->get_dotnet_decompiler();
	if (decompiler.is_null()) {
		error_message = "No dotnet decompiler loaded";
		return ERR_CANT_RESOLVE;
	}
	script_info = decompiler->get_script_info(script_path);
	if (script_info.is_empty()) {
		error_message = "No script info found";
		return ERR_CANT_ACQUIRE_RESOURCE;
	}
	source = decompiler->decompile_individual_file(script_path);
	if (source.is_empty()) {
		error_message = "Failed to decompile script";
		return ERR_CANT_ACQUIRE_RESOURCE;
	}

	return reload(false);
}

void FakeCSharpScript::reload_from_file() {
	Error err = _reload_from_file();
	FAKECSHARPSCRIPT_FAIL_COND_MSG(err != OK, "Error reloading script: " + script_path);
}

bool FakeCSharpScript::can_instantiate() const {
	return true;
}

Ref<Script> FakeCSharpScript::get_base_script() const {
	if (base_type_paths.size() > 0 && !base_type_paths[0].is_empty()) {
		return ResourceCompatLoader::custom_load(base_type_paths[0], "", ResourceCompatLoader::get_default_load_type());
	}
	return Ref<Script>();
}

StringName FakeCSharpScript::get_global_name() const {
	return global_name;
}

bool FakeCSharpScript::inherits_script(const Ref<Script> &p_script) const {
	// TODO: Implement proper inheritance checking
	return true;
}

StringName FakeCSharpScript::get_instance_base_type() const {
	if (!base_type.is_empty()) {
		return base_type;
	}
	auto path = script_path.is_empty() ? get_path() : script_path;
	if (path.is_empty() || !path.is_resource_file()) {
		return {};
	}
	return "CSharpScript";
}

ScriptInstance *FakeCSharpScript::instance_create(Object *p_this) {
	if (!can_instantiate()) {
		return nullptr;
	}
	auto instance = memnew(FakeScriptInstance());
	instance->script = Ref<FakeCSharpScript>(this);
	instance->owner = p_this;
	instance->update_cached_prop_names();
	return instance;
}

PlaceHolderScriptInstance *FakeCSharpScript::placeholder_instance_create(Object *p_this) {
	// For now, return nullptr as we don't have a proper placeholder implementation
	return nullptr;
}

bool FakeCSharpScript::instance_has(const Object *p_this) const {
	// For now, return false as we don't track instances
	return false;
}

bool FakeCSharpScript::has_source_code() const {
	return !source.is_empty();
}

String FakeCSharpScript::get_source_code() const {
	return source;
}

void FakeCSharpScript::set_source_code(const String &p_code) {
	source = p_code;
}

PropertyHint string_to_property_hint(const String &p_string) {
	String name = p_string.to_upper();
	if (name == "NONE") return PROPERTY_HINT_NONE; ///< no hint provided.
	if (name == "RANGE") return PROPERTY_HINT_RANGE; ///< hint_text = "min,max[,step][,or_greater][,or_less][,hide_slider][,radians_as_degrees][,degrees][,exp][,suffix:<keyword>] range.
	if (name == "ENUM") return PROPERTY_HINT_ENUM; ///< hint_text= "val1,val2,val3,etc"
	if (name == "ENUM_SUGGESTION") return PROPERTY_HINT_ENUM_SUGGESTION; ///< hint_text= "val1,val2,val3,etc"
	if (name == "EXP_EASING") return PROPERTY_HINT_EXP_EASING; /// exponential easing function (Math::ease) use "attenuation" hint string to revert (flip h), "positive_only" to exclude in-out and out-in. (ie: "attenuation,positive_only")
	if (name == "LINK") return PROPERTY_HINT_LINK;
	if (name == "FLAGS") return PROPERTY_HINT_FLAGS; ///< hint_text= "flag1,flag2,etc" (as bit flags)
	if (name == "LAYERS_2D_RENDER") return PROPERTY_HINT_LAYERS_2D_RENDER;
	if (name == "LAYERS_2D_PHYSICS") return PROPERTY_HINT_LAYERS_2D_PHYSICS;
	if (name == "LAYERS_2D_NAVIGATION") return PROPERTY_HINT_LAYERS_2D_NAVIGATION;
	if (name == "LAYERS_3D_RENDER") return PROPERTY_HINT_LAYERS_3D_RENDER;
	if (name == "LAYERS_3D_PHYSICS") return PROPERTY_HINT_LAYERS_3D_PHYSICS;
	if (name == "LAYERS_3D_NAVIGATION") return PROPERTY_HINT_LAYERS_3D_NAVIGATION;
	if (name == "FILE") return PROPERTY_HINT_FILE; ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
	if (name == "DIR") return PROPERTY_HINT_DIR; ///< a directory path must be passed
	if (name == "GLOBAL_FILE") return PROPERTY_HINT_GLOBAL_FILE; ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
	if (name == "GLOBAL_DIR") return PROPERTY_HINT_GLOBAL_DIR; ///< a directory path must be passed
	if (name == "RESOURCE_TYPE") return PROPERTY_HINT_RESOURCE_TYPE; ///< a comma-separated resource object type, e.g. "NoiseTexture,GradientTexture2D". Subclasses can be excluded with a "-" prefix if placed *after* the base class, e.g. "Texture2D,-MeshTexture".
	if (name == "MULTILINE_TEXT") return PROPERTY_HINT_MULTILINE_TEXT; ///< used for string properties that can contain multiple lines
	if (name == "EXPRESSION") return PROPERTY_HINT_EXPRESSION; ///< used for string properties that can contain multiple lines
	if (name == "PLACEHOLDER_TEXT") return PROPERTY_HINT_PLACEHOLDER_TEXT; ///< used to set a placeholder text for string properties
	if (name == "COLOR_NO_ALPHA") return PROPERTY_HINT_COLOR_NO_ALPHA; ///< used for ignoring alpha component when editing a color
	if (name == "OBJECT_ID") return PROPERTY_HINT_OBJECT_ID;
	if (name == "TYPE_STRING") return PROPERTY_HINT_TYPE_STRING; ///< a type string, the hint is the base type to choose
	if (name == "NODE_PATH_TO_EDITED_NODE") return PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE; // Deprecated.
	if (name == "OBJECT_TOO_BIG") return PROPERTY_HINT_OBJECT_TOO_BIG; ///< object is too big to send
	if (name == "NODE_PATH_VALID_TYPES") return PROPERTY_HINT_NODE_PATH_VALID_TYPES;
	if (name == "SAVE_FILE") return PROPERTY_HINT_SAVE_FILE; ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,". This opens a save dialog
	if (name == "GLOBAL_SAVE_FILE") return PROPERTY_HINT_GLOBAL_SAVE_FILE; ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,". This opens a save dialog
	if (name == "INT_IS_OBJECTID") return PROPERTY_HINT_INT_IS_OBJECTID; // Deprecated.
	if (name == "INT_IS_POINTER") return PROPERTY_HINT_INT_IS_POINTER;
	if (name == "ARRAY_TYPE") return PROPERTY_HINT_ARRAY_TYPE;
	if (name == "LOCALE_ID") return PROPERTY_HINT_LOCALE_ID;
	if (name == "LOCALIZABLE_STRING") return PROPERTY_HINT_LOCALIZABLE_STRING;
	if (name == "NODE_TYPE") return PROPERTY_HINT_NODE_TYPE; ///< a node object type
	if (name == "HIDE_QUATERNION_EDIT") return PROPERTY_HINT_HIDE_QUATERNION_EDIT; /// Only Node3D::transform should hide the quaternion editor.
	if (name == "PASSWORD") return PROPERTY_HINT_PASSWORD;
	if (name == "LAYERS_AVOIDANCE") return PROPERTY_HINT_LAYERS_AVOIDANCE;
	if (name == "DICTIONARY_TYPE") return PROPERTY_HINT_DICTIONARY_TYPE;
	if (name == "TOOL_BUTTON") return PROPERTY_HINT_TOOL_BUTTON;
	if (name == "ONESHOT") return PROPERTY_HINT_ONESHOT; ///< the property will be changed by self after setting, such as AudioStreamPlayer.playing, Particles.emitting.
	if (name == "NO_NODEPATH") return PROPERTY_HINT_NO_NODEPATH; /// < this property will not contain a NodePath, regardless of type (Array, Dictionary, List, etc.). Needed for SceneTreeDock.
	if (name == "GROUP_ENABLE") return PROPERTY_HINT_GROUP_ENABLE; ///< used to make the property's group checkable. Only use for boolean types.
	if (name == "INPUT_NAME") return PROPERTY_HINT_INPUT_NAME;
	if (name == "FILE_PATH") return PROPERTY_HINT_FILE_PATH;
	if (name == "MAX") return PROPERTY_HINT_MAX;
	return PROPERTY_HINT_NONE;
}

Variant::Type string_to_variant_type(const String &name) {
	if (name == "Variant") return Variant::NIL;
	if (name == "Nil" || name == "null" || name == "None" || name == "void") return Variant::NIL;
	auto tp = Variant::get_type_by_name(name);
	if (tp == Variant::VARIANT_MAX){
		if (name.begins_with("Array")) {
			return Variant::ARRAY;
		}
		if (name.begins_with("Dictionary")) {
			return Variant::DICTIONARY;
		}
	}
	return tp;
}

String parasable_class_or_none(const String &p_type) {
	if (p_type.is_empty()) {
		return "";
	}
	if (p_type.contains("<") || !p_type.is_valid_identifier()){
		return "";
	}
	return p_type;
}


PropertyInfo get_pi_from_type(const String &p_type, const String &p_name = {}) {
	Variant::Type return_type = string_to_variant_type(p_type);
	PropertyInfo info;
	if (return_type == Variant::VARIANT_MAX) {
		String tp = parasable_class_or_none(p_type);
		if (!tp.is_empty()) {
			info.class_name = tp;
			info.type = Variant::OBJECT;
		} else {
			info.type = Variant::NIL;
		}
		info.name = p_name;
		return info;
	}
	info.type = return_type;
	info.name = p_name;
	return info;
}

MethodInfo dict_to_method_info(const Dictionary &p_dict) {
	MethodInfo info;
	info.name = p_dict["name"];
	String type = p_dict.get("return_type", "void");
	info.return_val = get_pi_from_type(type);
	Vector<String> parameter_names = p_dict.get("parameter_names", Vector<String>());
	Vector<String> parameter_types = p_dict.get("parameter_types", Vector<String>());
	for (int i = 0; i < parameter_names.size(); i++) {
		info.arguments.push_back(get_pi_from_type(parameter_types[i], parameter_names[i]));
	}
	if (p_dict.get("is_static", false)) {
		info.flags |= METHOD_FLAG_STATIC;
	}
	if (p_dict.get("is_abstract", false)) {
		info.flags |= METHOD_FLAG_VIRTUAL_REQUIRED;
	}
	// if (p_dict.get("is_virtual", false)) {
	// 	info.flags |= METHOD_FLAG_VIRTUAL;
	// }
	return info;
}


PropertyInfo dict_to_property_info(const Dictionary &p_dict) {
	PropertyInfo info = get_pi_from_type(p_dict.get("type", "void"), p_dict.get("name", ""));
	info.hint = string_to_property_hint(p_dict.get("property_hint", "NONE"));
	info.hint_string = p_dict.get("property_hint_string", "");
	return info;
}

String replace_variant_constants(const String &p_string) {
	String new_string = p_string;
	Ref<RegEx> re = memnew(RegEx());
	re->compile("\\b([A-Za-z_][A-Za-z0-9_]+)\\.([A-Za-z_][A-Za-z0-9_]+)\\b");
	Ref<RegExMatch> match = re->search(new_string);
	while (match.is_valid()) {
		auto start = match->get_start(0);
		auto end = match->get_end(0);
		String type_name = match->get_string(1);
		Variant::Type type = string_to_variant_type(type_name);
		if (type == Variant::VARIANT_MAX) {
			re->search(new_string, end);
			continue;
		}
		String constant_name = match->get_string(2);
		if (!Variant::has_constant(type, constant_name)) {
			re->search(new_string, end);
			continue;
		}
		Variant constant = Variant::get_constant_value(type, constant_name);
		if (constant == Variant()) {
			re->search(new_string, end);
			continue;
		}
		new_string = new_string.substr(0, start) + constant.get_construct_string() + new_string.substr(end);
		match = re->search(new_string);
	}
	return new_string;
}

bool parse_expression(const String &p_expression, Variant &r_value) {
	Ref<Expression> expr = Ref<Expression>(memnew(Expression()));
	Error err = expr->parse(p_expression);
	if (err == OK) {
		r_value = expr->execute(Array(), nullptr, false, true);
		if (!expr->has_execute_failed()) {
			return true;
		}
	}
	return false;
}

Error FakeCSharpScript::reload(bool p_keep_state) {
	error_message.clear();
	valid = false;
	loaded = false;

	if (script_info.is_empty()) {
		error_message = "Script info is empty";
		return ERR_INVALID_DATA;
	}

	String ns = script_info.get("namespace", "");
	global_name = script_info.get("class_name", script_path);
	local_name = global_name;
	base_classes = script_info.get("base_classes", Vector<String>());
	base_type_paths = script_info.get("base_type_paths", Vector<String>());
	base_type = base_classes.size() > 0 ? base_classes[0] : "RefCounted";
	TypedArray<Dictionary> properties = script_info.get("properties", TypedArray<Dictionary>());
	TypedArray<Dictionary> signals = script_info.get("signals", TypedArray<Dictionary>());
	TypedArray<Dictionary> methods = script_info.get("methods", TypedArray<Dictionary>());

	members.clear();
	_signals.clear();
	_methods.clear();
	for (int i = 0; i < properties.size(); i++) {
		Dictionary property = properties[i];
		String name = property.get("name", "");
		if (name.is_empty()) {
			continue;
		}
		members.insert(name, dict_to_property_info(property));
		String default_value = property.get("default_value", "");
		if (!default_value.is_empty()) {
			// quick hack for Color() with 3 arguments
			if (default_value.begins_with("Color(") && default_value.ends_with(")")) {
				if (default_value.count(",") == 2) {
					default_value = default_value.trim_suffix(")") + ",1.0)";
				}
			}
			VariantParser::StreamString ss;
			ss.s = default_value;
			Variant v;
			String err_string;
			int line = 0;
			Error err = VariantParserCompat::parse(&ss, v, err_string, line);
			if (err == OK) {
				member_default_values.insert(name, v);
			} else {
				if (parse_expression(default_value, v)) {
					member_default_values.insert(name, v);
				} else {
					default_value = replace_variant_constants(default_value);
					if (parse_expression(default_value, v)){
						member_default_values.insert(name, v);
					}
				}
			}
			if (!member_default_values.has(name)) {
				member_default_values.insert(name, Variant());
			}
		}
	}

	for (int i = 0; i < signals.size(); i++) {
		Dictionary signal = signals[i];
		String name = signal.get("name", "");
		if (name.is_empty()) {
			continue;
		}
		_signals.insert(name, dict_to_method_info(signal));
	}

	for (int i = 0; i < methods.size(); i++) {
		Dictionary method = methods[i];
		String name = method.get("name", "");
		if (name.is_empty()) {
			continue;
		}
		_methods.insert(name, dict_to_method_info(method));
	}

	// For now, just mark as valid and loaded
	valid = true;
	loaded = true;

	return OK;
}

#ifdef TOOLS_ENABLED
StringName FakeCSharpScript::get_doc_class_name() const {
	return global_name;
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

bool FakeCSharpScript::is_valid() const {
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
	return _signals.has(p_signal);
}

void FakeCSharpScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	for (const KeyValue<StringName, MethodInfo> &E : _signals) {
		r_signals->push_back(E.value);
	}
}

bool FakeCSharpScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	if (member_default_values.has(p_property)) {
		r_value = member_default_values[p_property];
		return true;
	}
	return false;
}

void FakeCSharpScript::update_exports() {
	// For now, do nothing
}

void FakeCSharpScript::get_script_method_list(List<MethodInfo> *p_list) const {
	for (auto &E : _methods) {
		p_list->push_back(E.value);
	}
}

void FakeCSharpScript::get_script_property_list(List<PropertyInfo> *p_list) const {
	Script *parent = (Script *)this;
	while (parent) {
		for (auto &E : members) {
			p_list->push_back(E.value);
		}
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

String FakeCSharpScript::get_error_message() const {
	return error_message;
}

void FakeCSharpScript::set_override_bytecode_revision(int p_revision) {
	override_bytecode_revision = p_revision;
}

int FakeCSharpScript::get_override_bytecode_revision() const {
	return override_bytecode_revision;
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

void FakeCSharpScript::set_original_class(const String &p_class) {
	original_class = p_class;
}

String FakeCSharpScript::get_original_class() const {
	return original_class;
}

bool FakeCSharpScript::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "script/source") {
		r_ret = get_source_code();
		return true;
	}
	if (properties.has(p_name)) {
		r_ret = properties[p_name];
		return true;
	}
	return false;
}

bool FakeCSharpScript::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "script/source") {
		set_source_code(p_value);
		return true;
	}
	properties[p_name] = p_value;
	return true;
}

void FakeCSharpScript::_get_property_list(List<PropertyInfo> *p_properties) const {
	p_properties->push_back(PropertyInfo(Variant::STRING, "script/source", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
	for (const KeyValue<StringName, Variant> &E : properties) {
		p_properties->push_back(PropertyInfo(E.value.get_type(), E.key));
	}
}

Variant FakeCSharpScript::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	// For now, return empty variant as we don't support method calls
	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	return Variant();
}

void FakeCSharpScript::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_script_path"), &FakeCSharpScript::get_script_path);
	ClassDB::bind_method(D_METHOD("load_source_code", "path"), &FakeCSharpScript::load_source_code);
	ClassDB::bind_method(D_METHOD("get_error_message"), &FakeCSharpScript::get_error_message);
	ClassDB::bind_method(D_METHOD("set_override_bytecode_revision", "revision"), &FakeCSharpScript::set_override_bytecode_revision);
	ClassDB::bind_method(D_METHOD("get_override_bytecode_revision"), &FakeCSharpScript::get_override_bytecode_revision);
	ClassDB::bind_method(D_METHOD("set_autoload", "autoload"), &FakeCSharpScript::set_autoload);
	ClassDB::bind_method(D_METHOD("is_autoload"), &FakeCSharpScript::is_autoload);
	ClassDB::bind_method(D_METHOD("is_loaded"), &FakeCSharpScript::is_loaded);
	ClassDB::bind_method(D_METHOD("set_original_class", "class_name"), &FakeCSharpScript::set_original_class);
	ClassDB::bind_method(D_METHOD("get_original_class"), &FakeCSharpScript::get_original_class);
}
