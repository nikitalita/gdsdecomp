#include "csharp_script_info.h"
#include "core/math/expression.h"
#include "core/object/property_info.h"
#include "core/string/regex.h"
#include "core/variant/variant_parser.h"
#include "core/variant/variant_utility.h"
#include "compat/variant_writer_compat.h"

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

MethodInfo CSharpScriptInfo::dict_to_method_info(const Dictionary &p_dict) {
	MethodInfo info;
	info.name = p_dict["name"];
	info.return_val = PropertyInfo::from_dict(p_dict.get("return_type", ""));
	Array params = p_dict.get("parameters", Array());
	for (int i = 0; i < params.size(); i++) {
		Dictionary param_dict = params[i];
		info.arguments.push_back(PropertyInfo::from_dict(param_dict));
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
	info = PropertyInfo::from_dict(p_dict);
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
		auto pinfo = PropertyInfo::from_dict(property_dict);
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
