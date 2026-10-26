// public class PropertyInfo
// {
// 	[JsonPropertyName("name")]
// 	public string Name { get; set; }
// 	[JsonPropertyName("type")]
// 	public string Type { get; set; }
// 	[JsonPropertyName("default_value")]
// 	public string DefaultValue { get; set; }

// 	[JsonPropertyName("property_hint")]
// 	public string PropertyHint { get; set; }
// 	[JsonPropertyName("property_hint_string")]
// 	public string PropertyHintString { get; set; }
// 	[JsonPropertyName("property_usage")]
// 	public string PropertyUsage { get; set; }


// 	public PropertyInfo(string name, string type, string defaultValue, string propertyHint, string propertyUsage)
// 	{
// 		Name = name;
// 		Type = GodotStuff.CSharpTypeToGodotType(type);
// 		DefaultValue = defaultValue;
// 		PropertyHint = Common.CamelCaseToSnakeCase(propertyHint).ToUpper();
// 		PropertyHintString = propertyUsage;
// 	}
// }


// public class MethodInfo
// {
// 	[JsonPropertyName("name")]
// 	public string Name { get; set; }
// 	[JsonPropertyName("return_type")]
// 	public string ReturnType { get; set; }
// 	[JsonPropertyName("parameter_names")]
// 	public string[] ParameterNames { get; set; }
// 	[JsonPropertyName("parameter_types")]
// 	public string[] ParameterTypes { get; set; }

// 	[JsonPropertyName("is_static")]
// 	public bool IsStatic { get; set; }

// 	[JsonPropertyName("is_abstract")]
// 	public bool IsAbstract { get; set; }

// 	[JsonPropertyName("is_virtual")]
// 	public bool IsVirtual { get; set; }

// 	public MethodInfo(string name, string returnType, string[] parameterNames, string[] parameterTypes, bool isStatic, bool isAbstract, bool isVirtual)
// 	{
// 		Name = name;
// 		ReturnType = GodotStuff.CSharpTypeToGodotType(returnType);
// 		ParameterNames = parameterNames;
// 		ParameterTypes = parameterTypes.Select(GodotStuff.CSharpTypeToGodotType).ToArray();
// 		IsStatic = isStatic;
// 		IsAbstract = isAbstract;
// 	}
// }

// public class GodotScriptInfo
// {
// 	[JsonPropertyName("path")]

// 	public string Path { get; set; }
// 	[JsonPropertyName("namespace")]
// 	public string Namespace { get; set; }
// 	[JsonPropertyName("class_name")]
// 	public string ClassName { get; set; }
// 	[JsonPropertyName("base_classes")]
// 	public string[] BaseClasses { get; set; }

// 	[JsonPropertyName("base_type_paths")]
// 	public string[] BaseTypePaths { get; set; }

// 	[JsonPropertyName("godot_sharp_base_type")]
// 	public string? GodotSharpBaseType { get; set; }

// 	[JsonPropertyName("properties")]
// 	public PropertyInfo[] Properties { get; set; }
// 	[JsonPropertyName("signals")]
// 	public MethodInfo[] Signals { get; set; }
// 	[JsonPropertyName("methods")]
// 	public MethodInfo[] Methods { get; set; }

// 	[JsonPropertyName("is_tool")]
// 	public bool IsTool { get; set; }

// 	[JsonPropertyName("is_abstract")]
// 	public bool IsAbstract { get; set; }

// 	[JsonPropertyName("is_global_class")]
// 	public bool IsGlobalClass { get; set; }

// 	[JsonPropertyName("icon_path")]
// 	public string IconPath { get; set; }

// 	[JsonPropertyName("script_text")]
// 	public string ScriptText { get; set; }

// }


#pragma once
#include "core/object/property_info.h"
#include "core/object/method_info.h"
#include "core/variant/variant.h"
#include "core/variant/variant_parser.h"

struct PropertyInfoWithDefault {
	PropertyInfo info;
	Variant default_value;

	static PropertyInfoWithDefault from_dict(const Dictionary &p_dict, VariantParser::ResourceParser *p_res_parser = nullptr);
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

