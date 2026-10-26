
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

public enum PropertyUsageFlags
{
	PROPERTY_USAGE_NONE = 0,
	// 1 << 0
	PROPERTY_USAGE_STORAGE = 1 << 1,
	PROPERTY_USAGE_EDITOR = 1 << 2,
	PROPERTY_USAGE_INTERNAL = 1 << 3,
	PROPERTY_USAGE_CHECKABLE = 1 << 4,
	PROPERTY_USAGE_CHECKED = 1 << 5,
	PROPERTY_USAGE_GROUP = 1 << 6,
	PROPERTY_USAGE_CATEGORY = 1 << 7,
	PROPERTY_USAGE_SUBGROUP = 1 << 8,
	PROPERTY_USAGE_CLASS_IS_BITFIELD = 1 << 9,
	PROPERTY_USAGE_NO_INSTANCE_STATE = 1 << 10,
	PROPERTY_USAGE_RESTART_IF_CHANGED = 1 << 11,
	PROPERTY_USAGE_SCRIPT_VARIABLE = 1 << 12,
	PROPERTY_USAGE_STORE_IF_NULL = 1 << 13,
	PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED = 1 << 14,
	PROPERTY_USAGE_SCRIPT_DEFAULT_VALUE = 1 << 15,
	PROPERTY_USAGE_CLASS_IS_ENUM = 1 << 16,
	PROPERTY_USAGE_NIL_IS_VARIANT = 1 << 17,
	PROPERTY_USAGE_ARRAY = 1 << 18,
	PROPERTY_USAGE_ALWAYS_DUPLICATE = 1 << 19,
	PROPERTY_USAGE_NEVER_DUPLICATE = 1 << 20,
	PROPERTY_USAGE_HIGH_END_GFX = 1 << 21,
	PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT = 1 << 22,
	PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT = 1 << 23,
	PROPERTY_USAGE_KEYING_INCREMENTS = 1 << 24,
	PROPERTY_USAGE_DEFERRED_SET_RESOURCE = 1 << 25,
	PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT = 1 << 26,
	PROPERTY_USAGE_EDITOR_BASIC_SETTING = 1 << 27,
	PROPERTY_USAGE_READ_ONLY = 1 << 28,
	PROPERTY_USAGE_SECRET = 1 << 29,

	PROPERTY_USAGE_DEFAULT = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR,
	PROPERTY_USAGE_NO_EDITOR = PROPERTY_USAGE_STORAGE,
};

public class PropertyInfoWithDefault
{
	[JsonIgnore]
	public const string PROP_HINT_NONE = "None";

	[JsonPropertyName("name")]
	public string Name { get; set; } = "";
	[JsonPropertyName("type")]
	public string Type { get; set; } = "Nil";

	[JsonPropertyName("class_name")]
	public string ClassName { get; set; } = "";

	[JsonPropertyName("hint")]
	public string PropertyHint { get; set; } = PROP_HINT_NONE;
	[JsonPropertyName("hint_string")]
	public string PropertyHintString { get; set; } = "";
	[JsonPropertyName("usage")]
	public PropertyUsageFlags PropertyUsage { get; set; }

	[JsonPropertyName("default_value")]
	public string DefaultValue { get; set; } = "";

	public PropertyInfoWithDefault(string name, string type, string class_name, string propertyHint, string propertyHintString, PropertyUsageFlags propertyUsage, string defaultValue)
	{
		Name = name;
		Type = type;
		ClassName = class_name;
		DefaultValue = defaultValue;
		PropertyHint = string.IsNullOrEmpty(propertyHint) ? PROP_HINT_NONE : Common.CamelCaseToSnakeCase(propertyHint).ToUpper();
		PropertyHintString = propertyHintString;
		PropertyUsage = propertyUsage;
	}

	protected PropertyInfoWithDefault()
	{
	}

	public static PropertyInfoWithDefault VoidProp()
	{
		return new PropertyInfoWithDefault("", "Nil", "", "", "", PropertyUsageFlags.PROPERTY_USAGE_NONE, "");
	}

	public static PropertyInfoWithDefault FromSymbolOrType(object entity, SyntaxTree syntaxTree, GodotProjectDecompiler projectDecompiler)
	{
		string propHint = PROP_HINT_NONE;
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
				propHint = Common.GetEnumValueName(exportAttr.FixedArguments[0].Type, hintValue, PROP_HINT_NONE);
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
			if (propHint == PROP_HINT_NONE)
			{
				propHint = "ENUM";
			}
		}
		// there are no usage annotations on C# properties in Godot
		var variantType = GodotStuff.CSharpTypeToVariantType(retTypeName);
		if (variantType == "Object" && string.IsNullOrEmpty(className))
		{
			className = GodotStuff.CSharpTypeToGodotType(retTypeName);
		}
		if ((variantType == "Array" || variantType == "Dictionary") && string.IsNullOrEmpty(propHintString) && propHint == PROP_HINT_NONE)
		{
			propHint = variantType.ToUpper();
			propHintString = GodotStuff.CSharpTypeToGodotType(retTypeName);
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

