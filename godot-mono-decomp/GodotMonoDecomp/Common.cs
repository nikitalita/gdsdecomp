using System.Text;
using ICSharpCode.Decompiler.CSharp;
using ICSharpCode.Decompiler.CSharp.ProjectDecompiler;
using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.Decompiler.TypeSystem;
using ICSharpCode.Decompiler.Util;

namespace GodotMonoDecomp;

public static class Common
{
	/// <summary>
	/// Gets the default C# language version for the given module based on its target framework.
	/// </summary>
	/// <param name="module">The module to determine the language version for.</param>
	/// <returns>The default C# language version for the module.</returns>
	public static LanguageVersion GetDefaultCSharpLanguageLevel(MetadataFile module){
		// Based on this table:
		// Target 			Version   C# language version default
		// .NET 			10.x 	  C# 14
		// .NET 			9.x 	  C# 13
		// .NET 			8.x 	  C# 12
		// .NET 			7.x 	  C# 11
		// .NET 			6.x 	  C# 10
		// .NET 			5.x 	  C# 9.0
		// .NET Core 		3.x 	  C# 8.0
		// .NET Core 		2.x 	  C# 7.3
		// .NET Standard 	2.1 	  C# 8.0
		// .NET Standard 	2.0 	  C# 7.3
		// .NET Standard 	1.x 	  C# 7.3
		// .NET Framework 	all 	  C# 7.3

		// determine the dotnet version
		var dotnetVersion = TargetServices.DetectTargetFramework(module);
		if (dotnetVersion == null)
		{
			return LanguageVersion.CSharp7_3;
		}
		int verMajor = dotnetVersion.VersionNumber / 100;
		int verMinor = dotnetVersion.VersionNumber % 100 / 10;

		// .NET Framework
		// ".NETFramework" applies to all the newer ".NET" as well as the legacy ".NET Framework", so check that the version is less than 5
		if (dotnetVersion.Identifier == ".NETFramework" && verMajor < 5){
			return LanguageVersion.CSharp7_3;
		}

		// .NET Core
		// ".NETCoreApp" applies to all the newer ".NET" if the module is an app, so check if the version number is less than 4
		if ((dotnetVersion.Identifier == ".NETCoreApp" || dotnetVersion.Identifier == ".NETCore") && verMajor < 4){
			if (verMajor == 3) {
				return LanguageVersion.CSharp8_0;
			}
			return LanguageVersion.CSharp7_3;
		}

		// .NET Standard
		if (dotnetVersion.Identifier == ".NETStandard" && verMajor <= 2){
			if (verMinor >= 1){
				return LanguageVersion.CSharp8_0;
			}
			return LanguageVersion.CSharp7_3;
		}
		// .NET
		switch (verMajor){
			case 11:
				return LanguageVersion.CSharp15_0;
			case 10:
				return LanguageVersion.CSharp14_0;
			case 9:
				return LanguageVersion.CSharp13_0;
			case 8:
				return LanguageVersion.CSharp12_0;
			case 7:
				return LanguageVersion.CSharp11_0;
			case 6:
				return LanguageVersion.CSharp10_0;
			case 5:
				return LanguageVersion.CSharp9_0;
			default:
			{
				if (verMajor > 11) {
					return LanguageVersion.Latest;
				}
			}
				return LanguageVersion.CSharp7_3;
		}
	}

	public static string TrimPrefix(string path, string prefix)
	{
		if (!string.IsNullOrEmpty(path) && !string.IsNullOrEmpty(prefix) && path.StartsWith(prefix))
		{
			return path[prefix.Length..];
		}
		return path;
	}

	public static void RemoveDirIfEmpty(string dir)
	{
		if (Directory.Exists(dir) && Directory.GetFiles(dir).Length == 0)
		{
			try
			{
				Directory.Delete(dir);
			}
			catch (IOException)
			{
				// ignore
			}
		}
	}

	public static IEnumerable<string> ListCSharpFiles(string directory, bool absolute)
	{
		try
		{
			// check if the directory exists
			if (!Directory.Exists(directory))
			{
				return Enumerable.Empty<string>();
			}

			var files = Directory.GetFiles(directory, "*.cs", SearchOption.AllDirectories);
			if (!absolute)
			{
				// if not absolute, return the relative paths
				files = files.Select(f => FileUtility.GetRelativePath(directory, f)).ToArray();
			}

			return files;
		}
		catch (IOException)
		{
			// if the directory doesn't exist, return an empty list
			return Enumerable.Empty<string>();
		}
	}

	public static string RemoveNamespacePartOfPath(string path, string ns)
	{
		// remove the namespace part of the path
		if (ns == "")
		{
			return path;
		}

		// find the ns in the path
		if (!path.ToLower().StartsWith(ns.ToLower()))
		{
			ns = ns.Replace('.', '/');
		}

		if (path.ToLower().StartsWith(ns.ToLower()))
		{
			return path.Substring(ns.Length + 1);
		}

		return path;
	}

	public static string? FindCommonRoot(IEnumerable<string> paths)
	{
		if (paths == null || !paths.Any())
		{
			return null;
		}

		// sort by length to find the shortest path first
		paths = paths.OrderBy(p => p.Length);

		var commonRoot = paths.First();
		foreach (var path in paths.Skip(1))
		{
			while (!path.StartsWith(commonRoot, StringComparison.OrdinalIgnoreCase))
			{
				commonRoot = Path.GetDirectoryName(commonRoot);
				if (commonRoot == null)
				{
					return null;
				}
			}
		}

		return commonRoot;
	}

	public static void EnsureDir(string targetDirectory)
	{

		// ensure the directory exists for new_path
		if (string.IsNullOrEmpty(targetDirectory) || Directory.Exists(targetDirectory)) return;
		try
		{
			Directory.CreateDirectory(targetDirectory);
		}
		catch (IOException)
		{
			// File.Delete(dir);
			try
			{
				Directory.CreateDirectory(targetDirectory);
			}
			catch (IOException)
			{
				try
				{
					Directory.CreateDirectory(targetDirectory);
				}
				catch (IOException)
				{
					// ignore
				}
			}
		}
	}

	public static string RemoveNamespaceFromTypeName(string input)
	{
		if (string.IsNullOrEmpty(input))
		{
			return input;
		}
		if (input.Contains("::"))
		{
			input = input.Substring(input.LastIndexOf("::") + 2);
		}
		if (input.Contains("."))
		{
			input = input.Substring(input.LastIndexOf(".") + 1);
		}
		return input;
	}

	public static string CamelCaseToSnakeCase(string input)
	{
		if (string.IsNullOrEmpty(input))
			return input;

		StringBuilder sb = new StringBuilder();
		sb.Append(char.ToLowerInvariant(input[0]));
		for (int i = 1; i < input.Length - 1; i++) // Don't add an underscore if the first or last character is uppercase
		{
			char c = input[i];
			if (char.IsUpper(c) && char.IsLower(input[i - 1]))
			{
				sb.Append('_');
			}
			sb.Append(char.ToLowerInvariant(c));
		}
		if (input.Length > 1)
		{
			sb.Append(char.ToLowerInvariant(input[input.Length - 1]));
		}
		return sb.ToString();
	}

	public static string[] GetEnumValueNames(IType type)
	{
		if (type.Kind != TypeKind.Enum)
		{
			return [];
		}

		return type
			.GetFields()
			.Where(field => !field.FullName.StartsWith("System.Enum") && !field.Name.EndsWith("value__"))
			.Select(field => field.Name).ToArray();
	}

	public static string GetEnumValueName(IType type, long value, string defaultValue = "")
	{
		var names = GetEnumValueNames(type);
		if (names.Length == 0 || value >= names.Length)
		{
			return defaultValue;
		}
		return names[value];
	}

	public static Guid GenerateDeterministicGuidFromString(string input)
	{
		const byte Variant10xxMask = 0xC0;
		const byte Variant10xxValue = 0x80;

		const ushort VersionMask = 0xF000;
		const ushort Version4Value = 0x4000;
		using (var md5 = System.Security.Cryptography.MD5.Create())
		{
			byte[] inputBytes = Encoding.UTF8.GetBytes(input);
			byte[] hashBytes = md5.ComputeHash(inputBytes);
			short _c = (short)(hashBytes[3] | hashBytes[4] << 8);
			byte _d = hashBytes[5];
			_c = (short)((_c & ~VersionMask) | Version4Value);
			_d = (byte)((_d & ~Variant10xxMask) | Variant10xxValue);

			hashBytes[3] = (byte)(_c & 0xFF);
			hashBytes[4] = (byte)((_c >> 8) & 0xFF);
			hashBytes[5] = _d;

			return new Guid(hashBytes);
		}
	}

	private const string GeneratedCodeAttributeFullName = "System.CodeDom.Compiler.GeneratedCodeAttribute";
	public static bool RemoveGeneratedCodeAttributes(ICSharpCode.Decompiler.CSharp.Syntax.AstNodeCollection<ICSharpCode.Decompiler.CSharp.Syntax.AttributeSection> attributeSections, string generatorName)
	{
		bool removedAny = false;
		foreach (var section in attributeSections.ToArray())
		{
			foreach (var attribute in section.Attributes.ToArray())
			{
				if (IsGeneratedCodeAttributeForTool(attribute, generatorName))
				{
					attribute.Remove();
					removedAny = true;
				}
			}

			if (section.Attributes.Count == 0)
			{
				section.Remove();
			}
		}

		return removedAny;
	}

	public static bool HasMatchingShortAttributeName(string typeName, string expectedShortName)
	{
		if (typeName.StartsWith("global::", StringComparison.Ordinal))
		{
			typeName = typeName.Substring("global::".Length);
		}

		int lastDot = typeName.LastIndexOf('.');
		if (lastDot >= 0 && lastDot < typeName.Length - 1)
		{
			typeName = typeName.Substring(lastDot + 1);
		}

		if (typeName.EndsWith("Attribute", StringComparison.Ordinal))
		{
			typeName = typeName.Substring(0, typeName.Length - "Attribute".Length);
		}

		return string.Equals(typeName, expectedShortName, StringComparison.Ordinal);
	}

	public static bool IsAttribute(ICSharpCode.Decompiler.CSharp.Syntax.Attribute attribute, string expectedFullName, string expectedShortName)
	{
		if (attribute.Type.Annotation<ICSharpCode.Decompiler.Semantics.TypeResolveResult>() is { Type: { } typeResult })
		{
			if (string.Equals(typeResult.FullName, expectedFullName, StringComparison.Ordinal))
			{
				return true;
			}
		}

		if (attribute.GetSymbol() is IMethod method && method.DeclaringType != null)
		{
			if (string.Equals(method.DeclaringType.FullName, expectedFullName, StringComparison.Ordinal))
			{
				return true;
			}
		}

		return HasMatchingShortAttributeName(attribute.Type.ToString(), expectedShortName);
	}


	public static bool IsGeneratedCodeAttributeForTool(ICSharpCode.Decompiler.CSharp.Syntax.Attribute attribute, string generatorName)
	{
		if (!IsAttribute(attribute, GeneratedCodeAttributeFullName, "GeneratedCode"))
		{
			return false;
		}

		if (attribute.Arguments.FirstOrDefault() is ICSharpCode.Decompiler.CSharp.Syntax.PrimitiveExpression { Value: string toolName })
		{
			return string.Equals(toolName, generatorName, StringComparison.Ordinal);
		}

		return false;
	}
}
