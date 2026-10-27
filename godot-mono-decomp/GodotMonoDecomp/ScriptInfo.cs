
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace GodotMonoDecomp;
using System.Text.Json.Serialization;
using System.Text.Json;
using System.IO;
using System.Text.Json.Serialization.Metadata;
using ICSharpCode.Decompiler.TypeSystem;
using ICSharpCode.Decompiler.CSharp.Syntax;

// [JsonSourceGenerationOptions(WriteIndented = false)]
[JsonSerializable(typeof(Dictionary<string, GodotScriptInfo>))]
[JsonSerializable(typeof(PropertyInfoWithDefault))]
[JsonSerializable(typeof(MethodInfo))]
[JsonSerializable(typeof(GodotScriptInfo))]
internal partial class SISrcGenContext : JsonSerializerContext
{
}


public class PropertyInfoWithDefault
{
	[JsonPropertyName("name")]
	public string Name { get; set; } = "";
	[JsonPropertyName("type")]
	public VariantType Type { get; set; } = VariantType.NIL;

	[JsonPropertyName("class_name")]
	public string ClassName { get; set; } = "";

	[JsonPropertyName("hint")]
	public PropertyHint Hint { get; set; } = PropertyHint.PROPERTY_HINT_NONE;
	[JsonPropertyName("hint_string")]
	public string HintString { get; set; } = "";
	[JsonPropertyName("usage")]
	public PropertyUsageFlags Usage { get; set; }

	[JsonPropertyName("default_value")]
	public string DefaultValue { get; set; } = "";

	public PropertyInfoWithDefault(string name, VariantType type, string class_name, PropertyHint propertyHint, string propertyHintString, PropertyUsageFlags propertyUsage, string defaultValue)
	{
		Name = name;
		Type = type;
		ClassName = class_name;
		DefaultValue = defaultValue;
		Hint = propertyHint;
		HintString = propertyHintString;
		Usage = propertyUsage;
	}

	protected PropertyInfoWithDefault()
	{
	}

	public static PropertyInfoWithDefault VoidProp()
	{
		return new PropertyInfoWithDefault("", VariantType.NIL, "", PropertyHint.PROPERTY_HINT_NONE, "", PropertyUsageFlags.PROPERTY_USAGE_NONE, "");
	}

	public static PropertyInfoWithDefault FromSymbolOrType(object entity, SyntaxTree syntaxTree, GodotProjectDecompiler projectDecompiler)
	{
		PropertyHint propHint = PropertyHint.PROPERTY_HINT_NONE;
		string propHintString = "";
		string defaultValue = "";
		IType? retType = null;
		PropertyUsageFlags usage = PropertyUsageFlags.PROPERTY_USAGE_NONE;
		string retTypeName = "";
		string name = "";
		bool isEnum = false;
		var className = "";
		if (entity is IMember prop)
		{
			name = prop.Name;
			retType = prop.ReturnType;
			retTypeName = prop.ReturnType.Name;
			usage = PropertyUsageFlags.PROPERTY_USAGE_DEFAULT;
			var exportAttr = prop.GetAttributes().FirstOrDefault(a => a.AttributeType.Name == "ExportAttribute");
			if (exportAttr?.FixedArguments.Length >= 2)
			{
				long hintValue = 0;
				if (exportAttr.FixedArguments[0].Value != null)
				{
					if (exportAttr.FixedArguments[0].Value is int intVal)
					{
						hintValue = intVal;
					}
					else if (exportAttr.FixedArguments[0].Value is long longVal)
					{
						hintValue = longVal;
					}
				}
				propHint = GodotStuff.StringToPropertyHint(Common.GetEnumValueName(exportAttr.FixedArguments[0].Type, hintValue, "NONE"));
				propHintString = exportAttr.FixedArguments[1].Value?.ToString() ?? "";
			}

			// Godot nodes and resources set to instances of CSharp scripts do not have properties set in the scene/resource files if it is set to a default value;
			// As such, we need to get the default value of the property from the CSharp script.
			// This is primarily being done to allow for scenes with CSharp scripts to be instanced for scene export.
			GetFieldInitializerValueVisitor? visitor = null;
			visitor = new GetFieldInitializerValueVisitor(prop, projectDecompiler);
			syntaxTree.AcceptVisitor(visitor);

			if (visitor == null)
			{
				defaultValue = "";
			}
			else if (string.IsNullOrEmpty(visitor.strVal))
			{
				defaultValue = "";
			}
			else
			{
				defaultValue = visitor.strVal;
			}
		}
		else if (entity is IVariable variable)
		{
			name = variable.Name;
			retType = variable.Type;
			retTypeName = variable.Type.Name;
			usage = PropertyUsageFlags.PROPERTY_USAGE_NONE;
		}
		else if (entity is IMethod method)
		{
			// no name
			retType = method.ReturnType;
			retTypeName = method.ReturnType.Name;
			usage = PropertyUsageFlags.PROPERTY_USAGE_NONE;
		}
		else if (entity is Type type)
		{
			// no name
			isEnum = type.IsEnum;
			retTypeName = type.Name;
			usage = PropertyUsageFlags.PROPERTY_USAGE_NONE;
			if (type.IsEnum)
			{
				retTypeName = type.GetEnumUnderlyingType().Name;
				className = type.Name;
			}
		}
		else
		{
			return VoidProp();
		}

		if (retType != null && retType.Kind == TypeKind.Enum)
		{
			isEnum = true;
			retTypeName = retType.GetEnumUnderlyingType().Name;
			className = retType.Name;
		}
		if (isEnum)
		{
			usage |= PropertyUsageFlags.PROPERTY_USAGE_CLASS_IS_ENUM;
			if (propHint == PropertyHint.PROPERTY_HINT_NONE)
			{
				propHint = PropertyHint.PROPERTY_HINT_ENUM;
			}
		}
		var variantType = GodotStuff.CSharpTypeToVariantType(retTypeName);
		if (variantType == VariantType.OBJECT && string.IsNullOrEmpty(className))
		{
			className = GodotStuff.CSharpTypeToGodotType(retTypeName);
			if (propHint == PropertyHint.PROPERTY_HINT_NONE) {
				propHint = PropertyHint.PROPERTY_HINT_RESOURCE_TYPE;
				propHintString = retTypeName;
			}
		}
		if (variantType == VariantType.ARRAY || variantType == VariantType.DICTIONARY)
		{
			usage |= variantType == VariantType.ARRAY ? PropertyUsageFlags.PROPERTY_USAGE_ARRAY : 0;
			if (string.IsNullOrEmpty(propHintString) && propHint == PropertyHint.PROPERTY_HINT_NONE) {
				propHint = variantType == VariantType.ARRAY ? PropertyHint.PROPERTY_HINT_ARRAY_TYPE : PropertyHint.PROPERTY_HINT_DICTIONARY_TYPE;
				propHintString = GodotStuff.CSharpTypeToGodotType(retTypeName);
			}
		}

		return new PropertyInfoWithDefault(name, variantType, className, propHint, propHintString, usage, defaultValue);
	}
}

public class MethodInfo
{
	[JsonPropertyName("name")]
	public string Name { get; set; }
	[JsonPropertyName("return_type")]
	public PropertyInfoWithDefault ReturnType { get; set; }
	[JsonPropertyName("parameters")]
	public PropertyInfoWithDefault[] Parameters { get; set; }

	[JsonPropertyName("is_static")]
	public bool IsStatic { get; set; }

	[JsonPropertyName("is_abstract")]
	public bool IsAbstract { get; set; }

	[JsonPropertyName("is_virtual")]
	public bool IsVirtual { get; set; }

	public MethodInfo(string name, PropertyInfoWithDefault returnType, IEnumerable<PropertyInfoWithDefault> parameters, bool isStatic, bool isAbstract, bool isVirtual)
	{
		Name = name;
		ReturnType = returnType;
		Parameters = parameters.ToArray();
		IsStatic = isStatic;
		IsAbstract = isAbstract;
	}
}

public class GodotScriptInfo
{
	[JsonPropertyName("path")]

	public string Path { get; set; }
	[JsonPropertyName("namespace")]
	public string Namespace { get; set; }
	[JsonPropertyName("class_name")]
	public string ClassName { get; set; }
	[JsonPropertyName("base_classes")]
	public string[] BaseClasses { get; set; }

	[JsonPropertyName("base_class_paths")]
	public string[] BaseClassPaths { get; set; }

	[JsonPropertyName("godot_sharp_base_type")]
	public string? GodotSharpBaseType { get; set; }

	[JsonPropertyName("properties")]
	public PropertyInfoWithDefault[] Properties { get; set; }
	[JsonPropertyName("signals")]
	public MethodInfo[] Signals { get; set; }
	[JsonPropertyName("methods")]
	public MethodInfo[] Methods { get; set; }

	[JsonPropertyName("is_tool")]
	public bool IsTool { get; set; }

	[JsonPropertyName("is_abstract")]
	public bool IsAbstract { get; set; }

	[JsonPropertyName("is_global_class")]
	public bool IsGlobalClass { get; set; }

	[JsonPropertyName("icon_path")]
	public string IconPath { get; set; }

	[JsonPropertyName("script_text")]
	public string ScriptText { get; set; }

	public GodotScriptInfo(string path, string @namespace, string className, string[] baseClasses, string[] baseTypePaths, string? godotSharpBaseType,
		PropertyInfoWithDefault[] properties, MethodInfo[] signals, MethodInfo[] methods, bool isTool, bool isAbstract, bool isGlobalClass, string iconPath, string scriptText)
	{
		Path = path;
		Namespace = @namespace;
		ClassName = className;
		BaseClasses = baseClasses;
		BaseClassPaths = baseTypePaths;
		GodotSharpBaseType = godotSharpBaseType;
		Properties = properties;
		Signals = signals;
		Methods = methods;
		IsTool = isTool;
		IsAbstract = isAbstract;
		IsGlobalClass = isGlobalClass;
		IconPath = iconPath;
		// json escape the script text
		ScriptText = scriptText;
	}
	public static readonly JsonSerializerOptions DefaultJsonOptions = new()
	{
		WriteIndented = false,
		Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
		DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
	};

	public string ToJson(bool indented = false)
	{
		var buf = new MemoryStream();
		var opts = new JsonWriterOptions();
		opts.Indented = indented;
		opts.Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping;
		var strWriter = new Utf8JsonWriter(buf, opts);
		JsonSerializer.Serialize(strWriter, this, SISrcGenContext.Default.GodotScriptInfo);
		strWriter.Flush();
		var arr = buf.ToArray();
		var str = System.Text.Encoding.UTF8.GetString(arr);
		buf.Dispose();
		return str;
	}
}

