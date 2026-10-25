

using System.ComponentModel;
using System.Globalization;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using System.Text.Encodings.Web;
using System.Text.RegularExpressions;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.Json.Serialization.Metadata;
using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.CSharp;
using ReflectedProperty = System.Reflection.PropertyInfo;
using ICSharpCode.Decompiler.CSharp.OutputVisitor;

namespace GodotMonoDecomp;


[JsonSerializable(typeof(GodotMonoDecompSettings))]
[JsonSerializable(typeof(CSharpFormattingOptions))]
internal partial class GodotMonoDecompSettingsSrcGenContext : JsonSerializerContext
{
}

[DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.All)]
public class GodotMonoDecompSettings : DecompilerSettings
{
	/// <summary>
	/// Whether to write NuGet package references to the project file if dependency information is available.
	/// </summary>
	[Description("Whether to write NuGet package references to the project file if dependency information is available.")]
	[JsonPropertyName("write_nuget_package_references")]
	public bool WriteNuGetPackageReferences { get; set; } = true;

	/// <summary>
	/// Whether to check the hash of the NuGet package against the hash on nuget.org before writing project references.
	/// WARNING: This involves downloading the package from nuget.org and checking the hash of the downloaded package.
	/// </summary>
	[Description("Whether to check the hash of the NuGet package against the hash on nuget.org before writing project references.\nWARNING: This involves downloading the package from nuget.org and checking the hash of the downloaded package.")]
	[JsonPropertyName("verify_nuget_package_is_from_nuget_org")]
	public bool VerifyNuGetPackageIsFromNugetOrg { get; set; } = false;

	/// <summary>
	/// Whether to copy out-of-tree references (i.e. references that
	/// are not within the same directory structure as the project file) to the project file.
	/// </summary>
	[Description("Whether to copy out-of-tree references \n(i.e. references that are not within the same directory structure as the project file) to the project file.")]
	[JsonPropertyName("copy_out_of_tree_references")]
	public bool CopyOutOfTreeReferences { get; set; } = true;

	/// <summary>
	/// Whether to create additional projects for project references in main module.
	/// </summary>
	[Description("Whether to create additional projects for project references in main module.")]
	[JsonPropertyName("create_additional_projects_for_project_references")]
	public bool CreateAdditionalProjectsForProjectReferences { get; set; } = true;

	/// <summary>
	/// Override the language version for the decompilation.
	/// </summary>
	[Description("Override the language version for the decompilation.")]
	[JsonPropertyName("force_language_version")]
	[JsonConverter(typeof(JsonStringEnumConverter<LanguageVersion>))]
	public LanguageVersion? OverrideLanguageVersion { get; set; } = null;

	/// <summary>
	/// Whether to remove the body of the generated JsonSourceGeneration context classes.
	/// </summary>
	[Description("Whether to remove the body of the generated JsonSourceGeneration context classes.")]
	[JsonPropertyName("remove_generated_json_context_body")]
	public bool RemoveGeneratedJsonContextBody { get; set; } = false;

	/// <summary>
	/// Whether to run LiftCollectionInitializers.
	/// If false, the legacy RemoveBogusBaseConstructorCalls transform is used instead.
	/// </summary>
	[Description("Whether to run LiftCollectionInitializers.\nIf false, the legacy RemoveBogusBaseConstructorCalls transform is used instead.")]
	[JsonPropertyName("enable_collection_initializer_lifting")]
	public bool EnableCollectionInitializerLifting { get; set; } = true;

	/// <summary>
	/// Emit ILInstruction annotations as comments for statement/expression nodes.
	/// Intended for debug verification of annotation propagation.
	/// </summary>
	[Description("Emit ILInstruction annotations as comments for statement/expression nodes.\nIntended for debug verification of annotation propagation.")]
	[JsonPropertyName("emit_il_annotation_comments")]
	public bool EmitILAnnotationComments { get; set; } = false;

	/// <summary>
	/// Additional assembly search paths to use for resolving assembly references.
	/// This is useful for resolving assembly references in Steam workshop modules that rely on additional assemblies.
	/// </summary>
	[Description("Additional assembly search paths to use for resolving assembly references.\nThis is useful for resolving assembly references in Steam workshop modules that rely on additional assemblies.")]
	[JsonPropertyName("additional_assembly_search_paths")]
	public string[] AdditionalAssemblySearchPaths { get; set; } = Array.Empty<string>();

	/// <summary>
	/// Whether to automatically search for additional assembly search paths for Steam workshop modules.
	/// </summary>
	[Description("Whether to automatically search for additional assembly search paths for Steam workshop modules.")]
	[JsonPropertyName("automatically_search_workshop_dependencies")]
	public bool AutomaticallySearchWorkshopDependencies { get; set; } = true;

	/// <summary>
	/// Indent string for the decompiled project file.
	/// </summary>
	[EditorBrowsable(EditorBrowsableState.Never)]
	[Description("Indent string for the decompiled project file.")]
	[JsonPropertyName("indentation_string")]
	public string IndentationString
	{
		get
		{
			return CSharpFormattingOptions.IndentationString;
		}
		set
		{
			CSharpFormattingOptions.IndentationString = value;
		}
	}

	// CSharpFormattingOptions only has an internal constructor, so the serializer can't instantiate it on its
	// own. Supply a factory that starts from the decompiler's default formatting; properties present in the JSON
	// then override those defaults, and properties that are absent keep them.
	private static readonly JsonSerializerOptions SerializerOptions = new()
	{
		TypeInfoResolver = GodotMonoDecompSettingsSrcGenContext.Default.WithAddedModifier(typeInfo =>
		{
			if (typeInfo.Type != typeof(GodotMonoDecompSettings))
			{
				return;
			}
			for (int i = typeInfo.Properties.Count - 1; i >= 0; i--)
			{
				if (typeInfo.Properties[i].Name == nameof(DecompilerSettings.CSharpFormattingOptions))
				{
					typeInfo.Properties.RemoveAt(i);
				}
			}
		}),
	};

	private static JsonTypeInfo<GodotMonoDecompSettings> SettingsTypeInfo =>
		(JsonTypeInfo<GodotMonoDecompSettings>)SerializerOptions.GetTypeInfo(typeof(GodotMonoDecompSettings));

	private void InitializeDefaultSettings()
	{
		UseNestedDirectoriesForNamespaces = true;
		// This avoids certain race conditions during static initialization when attempting to run the decompiled project.
		AlwaysMoveInitializer = true;
	}

	public GodotMonoDecompSettings() : base()
	{
		InitializeDefaultSettings();
	}

	public GodotMonoDecompSettings(LanguageVersion languageVersion) : base(languageVersion)
	{
		InitializeDefaultSettings();
	}


	public new GodotMonoDecompSettings Clone()
	{
		var settings = (GodotMonoDecompSettings)base.Clone();
		settings.WriteNuGetPackageReferences = WriteNuGetPackageReferences;
		settings.VerifyNuGetPackageIsFromNugetOrg = VerifyNuGetPackageIsFromNugetOrg;
		settings.CopyOutOfTreeReferences = CopyOutOfTreeReferences;
		settings.CreateAdditionalProjectsForProjectReferences = CreateAdditionalProjectsForProjectReferences;
		settings.OverrideLanguageVersion = OverrideLanguageVersion;
		settings.RemoveGeneratedJsonContextBody = RemoveGeneratedJsonContextBody;
		settings.EnableCollectionInitializerLifting = EnableCollectionInitializerLifting;
		settings.EmitILAnnotationComments = EmitILAnnotationComments;
		settings.AutomaticallySearchWorkshopDependencies = AutomaticallySearchWorkshopDependencies;
		settings.AdditionalAssemblySearchPaths = AdditionalAssemblySearchPaths;
		settings.AutomaticallySearchWorkshopDependencies = AutomaticallySearchWorkshopDependencies;
		settings.IndentationString = IndentationString;
		return settings;
	}

	public string ToJson(bool indented = false)
	{
		var buf = new MemoryStream();
		var opts = new JsonWriterOptions();
		opts.Indented = indented;
		opts.Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping;
		var strWriter = new Utf8JsonWriter(buf, opts);
		JsonSerializer.Serialize(strWriter, this, SettingsTypeInfo);
		strWriter.Flush();
		var arr = buf.ToArray();
		var str = System.Text.Encoding.UTF8.GetString(arr);
		buf.Dispose();
		return str;
	}

	public static GodotMonoDecompSettings FromJson(string json)
	{
		var settings = JsonSerializer.Deserialize(json, SettingsTypeInfo)!;
		if (settings.OverrideLanguageVersion != null && settings.OverrideLanguageVersion.Value == 0)
		{
			settings.OverrideLanguageVersion = null;
		}
		if (settings.OverrideLanguageVersion != null)
		{
			settings.SetLanguageVersion(settings.OverrideLanguageVersion.Value);
		}
		return settings;
	}

	private static string _GetDefaultSettingsJSON(
		[DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.All)]
		GodotMonoDecompSettings godotMonoDecompSettings,

		[DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.All)]
		DecompilerSettings decompilerSettings,
		[DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.All)]
		CSharpFormattingOptions formattingOptions

		)
	{
		const string ILSpyPrefix = "ILSpy";
		const BindingFlags flags = BindingFlags.Public | BindingFlags.Instance;

		// Default values are always read from the GodotMonoDecompSettings instance, since its constructor
		// overrides some of the defaults inherited from DecompilerSettings.
		// The DecompilerSettings instance is only needed to keep the member annotations on its type.
		_ = decompilerSettings;
		_ = formattingOptions;

		var buf = new MemoryStream();
		var opts = new JsonWriterOptions
		{
			Indented = false,
			Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
		};
		using (var writer = new Utf8JsonWriter(buf, opts))
		{
			writer.WriteStartArray();

			foreach (var prop in typeof(GodotMonoDecompSettings).GetProperties(flags | BindingFlags.DeclaredOnly))
			{
				var jsonName = prop.GetCustomAttribute<JsonPropertyNameAttribute>()?.Name;
				if (jsonName == null || !IsSettingProperty(prop) || prop.GetCustomAttribute<EditorBrowsableAttribute>()?.State == EditorBrowsableState.Never)
				{
					continue;
				}
				WriteSettingEntry(writer, jsonName, prop, godotMonoDecompSettings);
			}

			foreach (var prop in typeof(DecompilerSettings).GetProperties(flags))
			{
				if (!IsSettingProperty(prop))
				{
					continue;
				}
				WriteSettingEntry(writer, prop.Name, prop, godotMonoDecompSettings, ILSpyPrefix);
			}

			// Don't do this right now
			// const string CSharpFormattingOptionsPrefix = "ILSpy/CSharpFormattingOptions";

			// foreach (var prop in typeof(CSharpFormattingOptions).GetProperties(flags))
			// {
			// 	if (!IsSettingProperty(prop))
			// 	{
			// 		continue;
			// 	}
			// 	WriteSettingEntry(writer, prop.Name, prop, formattingOptions, CSharpFormattingOptionsPrefix);
			// }

			writer.WriteEndArray();
		}
		var str = System.Text.Encoding.UTF8.GetString(buf.ToArray());
		buf.Dispose();
		return str;
	}

	private static bool IsSettingProperty(ReflectedProperty prop)
	{
		if (prop.GetMethod == null || !prop.GetMethod.IsPublic || prop.SetMethod == null || !prop.SetMethod.IsPublic)
		{
			return false;
		}
		if (prop.GetIndexParameters().Length != 0)
		{
			return false;
		}
		if (prop.GetCustomAttribute<BrowsableAttribute>()?.Browsable == false
			|| prop.GetCustomAttribute<ObsoleteAttribute>() != null
			|| prop.GetCustomAttribute<JsonIgnoreAttribute>() != null)
		{
			return false;
		}
		var type = Nullable.GetUnderlyingType(prop.PropertyType) ?? prop.PropertyType;
		return type.IsPrimitive || type.IsEnum || type == typeof(string) || type == typeof(string[]) || type == typeof(decimal) || type == typeof(Version);
	}

	private static bool ValueIsNumber(object? value)
	{
		return value is int || value is uint || value is long || value is ulong || value is float || value is double || value is decimal;
	}

	private static bool ArrayContains(Array array, object? value)
	{
		foreach (var item in array)
		{
			if (item == value)
			{
				return true;
			}
		}
		return false;
	}

	private static void WriteSettingEntry(Utf8JsonWriter writer, string name, ReflectedProperty prop, object defaults, string? group_prefix = null)
	{
		var group = (prop.GetCustomAttribute<CategoryAttribute>()?.Category.Replace('/', '-'))?.Replace("DecompilerSettings.", "") ?? "";
		// Skip `ProjectExport` group, we override all of its settings
		if (group == "ProjectExport")
		{
			return;
		}
		if (group_prefix != null)
		{
			if (group != "")
			{
				group = group_prefix + "/" + group;
			}
			else
			{
				group = group_prefix;
			}
		}
		writer.WriteStartObject();
		writer.WriteString("name", string.IsNullOrEmpty(group) ? name : group + "/" + name);
		writer.WriteString("brief", PascalCaseWordBoundary.Replace(prop.Name, " ").Replace("Nu Get", "NuGet"));
		writer.WriteString("description", prop.GetCustomAttribute<DescriptionAttribute>()?.Description ?? "");
		var enumType = Nullable.GetUnderlyingType(prop.PropertyType) ?? prop.PropertyType;
		writer.WritePropertyName("default_value");
		var defaultValue = prop.GetValue(defaults);

		if (enumType.IsEnum)
		{
			Array values = Enum.GetValuesAsUnderlyingType(enumType);
			if (values.Length == 0)
			{
				WriteJsonValue(writer, defaultValue);
				writer.WriteEndObject();
				return;
			}

			var hasNullDefaultValue = defaultValue is null;
			var realDefaultValue = defaultValue;
			// if defaultValue is null and the underlying type is a number, and the values don't contain 0, then write 0 as the default value
			if (ValueIsNumber(values.GetValue(0)) && hasNullDefaultValue)
			{
				if (!ArrayContains(values, 0))
				{
					realDefaultValue = 0;
				}
				else if (!ArrayContains(values, -1))
				{
					realDefaultValue = -1;
				}
			}
			WriteJsonValue(writer, realDefaultValue);
			// Maps each underlying value (as a JSON string key) to its name.
			writer.WriteStartObject("enum_values");
			if (hasNullDefaultValue)
			{
				writer.WritePropertyName("Auto-detect");
				WriteJsonValue(writer, realDefaultValue);
			}
			foreach (var enumValue in Enum.GetValuesAsUnderlyingType(enumType))
			{
				var enumName = Enum.GetName(enumType, enumValue);
				if (enumName != null)
				{
					writer.WritePropertyName(enumName);
					WriteJsonValue(writer, enumValue);
				}
			}
			writer.WriteEndObject();
		}
		else
		{
			WriteJsonValue(writer, defaultValue);
		}
		writer.WriteEndObject();
	}

	private static void WriteJsonValue(Utf8JsonWriter writer, object? value)
	{
		switch (value)
		{
			case null:
				writer.WriteNullValue();
				break;
			case bool b:
				writer.WriteBooleanValue(b);
				break;
			case int i:
				writer.WriteNumberValue(i);
				break;
			case uint ui:
				writer.WriteNumberValue(ui);
				break;
			case long l:
				writer.WriteNumberValue(l);
				break;
			case ulong ul:
				writer.WriteNumberValue(ul);
				break;
			case float f:
				writer.WriteNumberValue(f);
				break;
			case double d:
				writer.WriteNumberValue(d);
				break;
			case decimal m:
				writer.WriteNumberValue(m);
				break;
			case string[] strings:
				writer.WriteStartArray();
				foreach (var str in strings)
				{
					writer.WriteStringValue(str);
				}
				writer.WriteEndArray();
				break;
			case Enum e:
				// Written as the literal underlying value; names are provided separately via "enum_values".
				var underlying = Convert.ChangeType(e, Enum.GetUnderlyingType(e.GetType()), CultureInfo.InvariantCulture);
				if (underlying is ulong enumUlong)
				{
					writer.WriteNumberValue(enumUlong);
				}
				else
				{
					writer.WriteNumberValue(Convert.ToInt64(underlying, CultureInfo.InvariantCulture));
				}
				break;
			default:
				// Strings, versions, and any other primitive.
				writer.WriteStringValue(Convert.ToString(value, CultureInfo.InvariantCulture));
				break;
		}
	}

	private static readonly Regex PascalCaseWordBoundary = new("(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])", RegexOptions.Compiled);

	public static string GetDefaultSettingsJSON()
	{
		var settings = new GodotMonoDecompSettings();
		var formattingOptions = settings.CSharpFormattingOptions;
		return _GetDefaultSettingsJSON(new GodotMonoDecompSettings(), new DecompilerSettings(), formattingOptions);
	}


}
