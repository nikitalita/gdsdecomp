#include "csharp_script_info.h"
#include "core/math/expression.h"
#include "core/object/property_info.h"
#include "core/string/regex.h"
#include "core/variant/variant_parser.h"
#include "core/variant/variant_utility.h"
#include "compat/variant_writer_compat.h"

static PropertyHint string_to_property_hint(const String &p_string) {
	String name = p_string.to_upper();
	if (name == "NONE") {
		return PROPERTY_HINT_NONE; ///< no hint provided.
	}
	if (name == "RANGE") {
		return PROPERTY_HINT_RANGE; ///< hint_text = "min,max[,step][,or_greater][,or_less][,hide_slider][,radians_as_degrees][,degrees][,exp][,suffix:<keyword>] range.
	}
	if (name == "ENUM") {
		return PROPERTY_HINT_ENUM; ///< hint_text= "val1,val2,val3,etc"
	}
	if (name == "ENUM_SUGGESTION") {
		return PROPERTY_HINT_ENUM_SUGGESTION; ///< hint_text= "val1,val2,val3,etc"
	}
	if (name == "EXP_EASING") {
		return PROPERTY_HINT_EXP_EASING; /// exponential easing function (Math::ease) use "attenuation" hint string to revert (flip h), "positive_only" to exclude in-out and out-in. (ie: "attenuation,positive_only")
	}
	if (name == "LINK") {
		return PROPERTY_HINT_LINK;
	}
	if (name == "FLAGS") {
		return PROPERTY_HINT_FLAGS; ///< hint_text= "flag1,flag2,etc" (as bit flags)
	}
	if (name == "LAYERS_2D_RENDER") {
		return PROPERTY_HINT_LAYERS_2D_RENDER;
	}
	if (name == "LAYERS_2D_PHYSICS") {
		return PROPERTY_HINT_LAYERS_2D_PHYSICS;
	}
	if (name == "LAYERS_2D_NAVIGATION") {
		return PROPERTY_HINT_LAYERS_2D_NAVIGATION;
	}
	if (name == "LAYERS_3D_RENDER") {
		return PROPERTY_HINT_LAYERS_3D_RENDER;
	}
	if (name == "LAYERS_3D_PHYSICS") {
		return PROPERTY_HINT_LAYERS_3D_PHYSICS;
	}
	if (name == "LAYERS_3D_NAVIGATION") {
		return PROPERTY_HINT_LAYERS_3D_NAVIGATION;
	}
	if (name == "FILE") {
		return PROPERTY_HINT_FILE; ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
	}
	if (name == "DIR") {
		return PROPERTY_HINT_DIR; ///< a directory path must be passed
	}
	if (name == "GLOBAL_FILE") {
		return PROPERTY_HINT_GLOBAL_FILE; ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
	}
	if (name == "GLOBAL_DIR") {
		return PROPERTY_HINT_GLOBAL_DIR; ///< a directory path must be passed
	}
	if (name == "RESOURCE_TYPE") {
		return PROPERTY_HINT_RESOURCE_TYPE; ///< a comma-separated resource object type, e.g. "NoiseTexture,GradientTexture2D". Subclasses can be excluded with a "-" prefix if placed *after* the base class, e.g. "Texture2D,-MeshTexture".
	}
	if (name == "MULTILINE_TEXT") {
		return PROPERTY_HINT_MULTILINE_TEXT; ///< used for string properties that can contain multiple lines
	}
	if (name == "EXPRESSION") {
		return PROPERTY_HINT_EXPRESSION; ///< used for string properties that can contain multiple lines
	}
	if (name == "PLACEHOLDER_TEXT") {
		return PROPERTY_HINT_PLACEHOLDER_TEXT; ///< used to set a placeholder text for string properties
	}
	if (name == "COLOR_NO_ALPHA") {
		return PROPERTY_HINT_COLOR_NO_ALPHA; ///< used for ignoring alpha component when editing a color
	}
	if (name == "OBJECT_ID") {
		return PROPERTY_HINT_OBJECT_ID;
	}
	if (name == "TYPE_STRING") {
		return PROPERTY_HINT_TYPE_STRING; ///< a type string, the hint is the base type to choose
	}
	if (name == "NODE_PATH_TO_EDITED_NODE") {
		return PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE; // Deprecated.
	}
	if (name == "OBJECT_TOO_BIG") {
		return PROPERTY_HINT_OBJECT_TOO_BIG; ///< object is too big to send
	}
	if (name == "NODE_PATH_VALID_TYPES") {
		return PROPERTY_HINT_NODE_PATH_VALID_TYPES;
	}
	if (name == "SAVE_FILE") {
		return PROPERTY_HINT_SAVE_FILE; ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,". This opens a save dialog
	}
	if (name == "GLOBAL_SAVE_FILE") {
		return PROPERTY_HINT_GLOBAL_SAVE_FILE; ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,". This opens a save dialog
	}
	if (name == "INT_IS_OBJECTID") {
		return PROPERTY_HINT_INT_IS_OBJECTID; // Deprecated.
	}
	if (name == "INT_IS_POINTER") {
		return PROPERTY_HINT_INT_IS_POINTER;
	}
	if (name == "ARRAY_TYPE") {
		return PROPERTY_HINT_ARRAY_TYPE;
	}
	if (name == "LOCALE_ID") {
		return PROPERTY_HINT_LOCALE_ID;
	}
	if (name == "LOCALIZABLE_STRING") {
		return PROPERTY_HINT_LOCALIZABLE_STRING;
	}
	if (name == "NODE_TYPE") {
		return PROPERTY_HINT_NODE_TYPE; ///< a node object type
	}
	if (name == "HIDE_QUATERNION_EDIT") {
		return PROPERTY_HINT_HIDE_QUATERNION_EDIT; /// Only Node3D::transform should hide the quaternion editor.
	}
	if (name == "PASSWORD") {
		return PROPERTY_HINT_PASSWORD;
	}
	if (name == "LAYERS_AVOIDANCE") {
		return PROPERTY_HINT_LAYERS_AVOIDANCE;
	}
	if (name == "DICTIONARY_TYPE") {
		return PROPERTY_HINT_DICTIONARY_TYPE;
	}
	if (name == "TOOL_BUTTON") {
		return PROPERTY_HINT_TOOL_BUTTON;
	}
	if (name == "ONESHOT") {
		return PROPERTY_HINT_ONESHOT; ///< the property will be changed by self after setting, such as AudioStreamPlayer.playing, Particles.emitting.
	}
	if (name == "NO_NODEPATH") {
		return PROPERTY_HINT_NO_NODEPATH; /// < this property will not contain a NodePath, regardless of type (Array, Dictionary, List, etc.). Needed for SceneTreeDock.
	}
	if (name == "GROUP_ENABLE") {
		return PROPERTY_HINT_GROUP_ENABLE; ///< used to make the property's group checkable. Only use for boolean types.
	}
	if (name == "INPUT_NAME") {
		return PROPERTY_HINT_INPUT_NAME;
	}
	if (name == "FILE_PATH") {
		return PROPERTY_HINT_FILE_PATH;
	}
	if (name == "MAX") {
		return PROPERTY_HINT_MAX;
	}
	return PROPERTY_HINT_NONE;
}

static Variant::Type string_to_variant_type(const String &name) {
	if (name == "Variant") {
		return Variant::NIL;
	}
	if (name.is_empty() || name == "Nil" || name == "null" || name == "<null>" || name == "None" || name == "void") {
		return Variant::NIL;
	}
	auto tp = Variant::get_type_by_name(name);
	if (tp == Variant::VARIANT_MAX) {
		if (name.begins_with("Array")) {
			return Variant::ARRAY;
		}
		if (name.begins_with("Dictionary")) {
			return Variant::DICTIONARY;
		}
	}
	return tp;
}


static String parasable_class_or_none(const String &p_type) {
	if (p_type.is_empty()) {
		return "";
	}
	if (p_type.contains("<") || !p_type.is_valid_identifier()) {
		return "";
	}
	return p_type;
}


static String replace_variant_constants(const String &p_string) {
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
			match = re->search(new_string, end);
			continue;
		}
		String constant_name = match->get_string(2);
		if (!Variant::has_constant(type, constant_name)) {
			match = re->search(new_string, end);
			continue;
		}
		Variant constant = Variant::get_constant_value(type, constant_name);
		if (constant == Variant()) {
			match = re->search(new_string, end);
			continue;
		}
		String old_string = new_string;
		new_string = new_string.substr(0, start) + constant.get_construct_string() + new_string.substr(end);
		if (new_string == old_string) {
			match = re->search(new_string, end);
			continue;
		}
		match = re->search(new_string);
	}
	return new_string;
}

static bool parse_expression(const String &p_expression, Variant &r_value) {
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

static inline Variant ensure_type(const Variant &p_value, const Variant::Type &p_type) {
	if (p_type != Variant::NIL) {
		if (p_type == Variant::STRING && p_value == Variant()) {
			return String();
		}
		return VariantUtilityFunctions::type_convert(p_value, p_type);
	}
	return p_value;
}

static inline Variant get_default_value_from_string(const String &p_default_value, VariantParser::ResourceParser *p_res_parser) {
	String default_value = p_default_value;
	// quick hack for Color() with 3 arguments
	if (default_value.begins_with("Color(") && default_value.ends_with(")")) {
		if (default_value.count(",") == 2) {
			default_value = default_value.trim_suffix(")") + ",1.0)";
		}
	}
	VariantParser::ResourceParser default_parser;
	if (p_res_parser == nullptr) {
		default_parser.userdata = nullptr;
		default_parser.func = VariantParserCompat::parse_and_create_missing_resource;
		p_res_parser = &default_parser;
	}
	// it'll be like `GD.Load<PackedScene>("res://node/scene/transition/DefaultTransition.tscn")`
	static const String GD_LOAD_PREFIX = "GD.Load<";
	if (default_value.begins_with(GD_LOAD_PREFIX)){
		// let the parser handle it
		default_value = "Resource" + default_value.substr(default_value.find(">"));
	}
	VariantParser::StreamString ss;
	ss.s = default_value;
	Variant v;
	String err_string;
	int line = 0;
	Error err = VariantParserCompat::parse(&ss, v, err_string, line, p_res_parser);
	if (err == OK) {
		return v;
	} else {
		if (parse_expression(default_value, v)) {
			return v;
		} else {
			default_value = replace_variant_constants(default_value);
			if (parse_expression(default_value, v)) {
				return v;
			}
		}
	}
	return v;
}

static PropertyInfo get_pi_from_type(const String &p_type, const String &p_name, const String &p_class_name) {
	Variant::Type return_type = string_to_variant_type(p_type);
	PropertyInfo info;
	info.class_name = p_class_name;
	info.type = return_type;
	// TODO: Don't do this
	if (info.type == Variant::VARIANT_MAX) {
		String tp = parasable_class_or_none(p_type);
		if (!tp.is_empty()) {
			info.class_name = tp;
			info.type = Variant::OBJECT;
		} else {
			info.type = Variant::NIL;
		}
	}
	info.name = p_name;
	return info;
}

static inline PropertyInfo get_pi_from_dict(const Dictionary &p_dict) {
	PropertyInfo info = get_pi_from_type(p_dict.get("type", "void"), p_dict.get("name", ""), p_dict.get("class_name", ""));
	info.usage = p_dict.get("usage", PROPERTY_USAGE_DEFAULT);
	info.hint = string_to_property_hint(p_dict.get("hint", "NONE"));
	info.hint_string = p_dict.get("hint_string", "");
	return info;
}


MethodInfo CSharpScriptInfo::dict_to_method_info(const Dictionary &p_dict) {
	MethodInfo info;
	info.name = p_dict["name"];
	info.return_val = get_pi_from_dict(p_dict.get("return_type", ""));
	Array params = p_dict.get("parameters", Array());
	for (int i = 0; i < params.size(); i++) {
		Dictionary param_dict = params[i];
		info.arguments.push_back(get_pi_from_dict(param_dict));
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

static inline Vector<MethodInfo> _get_methods_from_arr(const Array &p_dict) {
	Vector<MethodInfo> methods;
	for (int i = 0; i < p_dict.size(); i++) {
		Dictionary dict = p_dict[i];
		MethodInfo method = CSharpScriptInfo::dict_to_method_info(dict);
		methods.push_back(method);
	}
	return methods;
}

PropertyInfoWithDefault PropertyInfoWithDefault::from_dict(const Dictionary &p_dict, VariantParser::ResourceParser *p_res_parser) {
	PropertyInfoWithDefault info;
	info._piwd_from_dict(p_dict, p_res_parser);
	return info;
}

void PropertyInfoWithDefault::_piwd_from_dict(const Dictionary &p_dict, VariantParser::ResourceParser *p_res_parser) {
	info = get_pi_from_dict(p_dict);
	default_value = ensure_type(get_default_value_from_string(p_dict.get("default_value", ""), p_res_parser), info.type);
}

CSharpScriptInfo CSharpScriptInfo::from_dict(const Dictionary &p_dict, VariantParser::ResourceParser *p_res_parser) {
	CSharpScriptInfo script_info;
	script_info.path = p_dict.get("path", "");
	script_info.namespace_name = p_dict.get("namespace", "");
	script_info.class_name = p_dict.get("class_name", "");
	script_info.base_classes = p_dict.get("base_classes", Vector<String>());
	script_info.base_type_paths = p_dict.get("base_class_paths", Vector<String>());
	script_info.godot_sharp_base_type = p_dict.get("godot_sharp_base_type", script_info.base_classes.size() > 0 ? script_info.base_classes[0] : "RefCounted");

	Array properties_array = p_dict.get("properties", Array());
	for (int i = 0; i < properties_array.size(); i++) {
		Dictionary property_dict = properties_array[i];
		auto pinfo = get_pi_from_dict(property_dict);
		String default_value_string = property_dict.get("default_value", "");
		Variant default_value;
		if (!default_value_string.is_empty()) {
			default_value = get_default_value_from_string(default_value_string, p_res_parser);
	#ifdef DEBUG_ENABLED
			if (default_value == Variant() && default_value_string != "null" && default_value_string != "<null>") {
				print_line(vformat("%s: Failed to parse expression for property %s with value %s", script_info.path, pinfo.name, default_value_string));
			}
	#endif
		}
		default_value = ensure_type(default_value, pinfo.type);
		script_info.properties.push_back({pinfo, default_value});
	}

	script_info.signals = _get_methods_from_arr(p_dict.get("signals", Array()));
	script_info.methods = _get_methods_from_arr(p_dict.get("methods", Array()));
	script_info.is_tool = p_dict.get("is_tool", false);
	script_info.is_abstract = p_dict.get("is_abstract", false);
	script_info.is_global_class = p_dict.get("is_global_class", false);
	script_info.icon_path = p_dict.get("icon_path", "");
	script_info.script_text = p_dict.get("script_text", "");

	return script_info;
}

void CSharpScriptInfo::clear() {
	path.clear();
	namespace_name.clear();
	class_name.clear();
	base_classes.clear();
	base_type_paths.clear();
	godot_sharp_base_type.clear();
	properties.clear();
	signals.clear();
	methods.clear();
	is_tool = false;
	is_abstract = false;
	is_global_class = false;
	icon_path.clear();
	script_text.clear();
}
