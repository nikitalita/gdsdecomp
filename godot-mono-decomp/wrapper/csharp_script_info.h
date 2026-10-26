#pragma once
#include "core/object/property_info.h"
#include "core/object/method_info.h"
#include "core/variant/variant.h"
#include "core/variant/variant_parser.h"

struct PropertyInfoWithDefault {
	PropertyInfo info;
	Variant default_value;

	static PropertyInfoWithDefault from_dict(const Dictionary &p_dict, VariantParser::ResourceParser *p_res_parser = nullptr);
protected:
	void _piwd_from_dict(const Dictionary &p_dict, VariantParser::ResourceParser *p_res_parser = nullptr);
};

struct CSharpScriptInfo {
	String path;
	String namespace_name;
	String class_name;
	Vector<String> base_classes;
	Vector<String> base_type_paths;
	String godot_sharp_base_type;
	Vector<PropertyInfoWithDefault> properties;
	Vector<MethodInfo> signals;
	Vector<MethodInfo> methods;
	String icon_path;
	String script_text;
	bool is_tool;
	bool is_abstract;
	bool is_global_class;

	void clear();

	static MethodInfo dict_to_method_info(const Dictionary &p_dict);

	static CSharpScriptInfo from_dict(const Dictionary &p_dict, VariantParser::ResourceParser *p_res_parser = nullptr);
};

