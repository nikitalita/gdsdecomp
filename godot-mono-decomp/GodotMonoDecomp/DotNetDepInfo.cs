using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.Decompiler.Util;
using Newtonsoft.Json.Linq;
using System.Net;
using System;
using System.Collections.Generic;
using System.Linq;

namespace GodotMonoDecomp;

public class DotNetCoreDepInfo : IEquatable<DotNetCoreDepInfo>
{

	public struct RuntimeComponentInfo : IEquatable<RuntimeComponentInfo>, IEquatable<RuntimeComponentInfo?>
	{
		public readonly string Name;

		public readonly string Extension;

		public readonly string? Directory;

		public readonly Version? AssemblyVersion;

		public readonly Version? FileVersion;

		public readonly string FileName => Name + Extension;

		public readonly string Path => System.IO.Path.Combine(Directory ?? "", FileName);

		public readonly AssemblyNameReference AssemblyRef => AssemblyNameReference.Parse($"{Name}, Version={AssemblyVersion?.ToString(4) ?? "1.0.0.0"}, Culture=neutral, PublicKeyToken=null");

		public RuntimeComponentInfo(string name, string extension, string? directory, Version? assemblyVersion, Version? fileVersion)
		{
			Name = name;
			Extension = extension;
			Directory = directory;
			AssemblyVersion = assemblyVersion;
			FileVersion = fileVersion;
		}

		public readonly bool Matches(IAssemblyReference? reference)
		{
			return reference != null && Name == reference.Name && AssemblyVersion == reference.Version;
		}

		public readonly bool Equals(RuntimeComponentInfo other)
		{
			return Name == other.Name && Extension == other.Extension && Directory == other.Directory && AssemblyVersion == other.AssemblyVersion && FileVersion == other.FileVersion;
		}

		public readonly bool Equals(RuntimeComponentInfo? other)
		{
			return other != null && Equals(other.Value);
		}

	}

	public struct NativeComponentInfo : IEquatable<NativeComponentInfo>, IEquatable<NativeComponentInfo?>
	{
		public readonly string Name;
		public readonly string Extension;
		public readonly string? Directory;

		public readonly string FileName => Name + Extension;

		public readonly string Path => System.IO.Path.Combine(Directory ?? "", FileName);

		public readonly Version? FileVersion;

		public NativeComponentInfo(string name, string extension, string? directory, Version? fileVersion)
		{
			Name = name;
			Extension = extension;
			Directory = directory;
			FileVersion = fileVersion;
		}

		public readonly bool Equals(NativeComponentInfo other)
		{
			return Name == other.Name && Extension == other.Extension && Directory == other.Directory && FileVersion == other.FileVersion;
		}

		public readonly bool Equals(NativeComponentInfo? other)
		{
			return other != null && Equals(other.Value);
		}
	}

	public enum HashMatchesNugetOrg
	{
		// This enum is used to determine if the SHA512 hash matches the package downloaded from nuget.org.
		// If it does, we can use the hash to verify the integrity of the package.
		Unknown,
		NoMatch,
		Match
	}
	public readonly string Name;
	public readonly string Version;
	public readonly string Type;
	public readonly string? Sha512;
	public readonly string? Path;
	public readonly string? HashPath;
	public readonly bool Serviceable;
	public readonly DotNetCoreDepInfo[] deps;
	public readonly RuntimeComponentInfo[] runtimeComponents;
	public readonly NativeComponentInfo[] nativeComponents;
	public readonly RuntimeComponentInfo? ThisRuntimeComponent;
	public HashMatchesNugetOrg HashMatchesNugetOrgStatus { get; private set; } = HashMatchesNugetOrg.Unknown;
	public System.Version AssemblyVersion => ThisRuntimeComponent?.AssemblyVersion ?? System.Version.Parse(ConvertToAssemblyVersion(Version));
	public AssemblyNameReference AssemblyRef => ThisRuntimeComponent?.AssemblyRef ?? AssemblyNameReference.Parse($"{Name}, Version={ConvertToAssemblyVersion(Version)}, Culture=neutral, PublicKeyToken=null");

	public bool IsAvailableOnNuget => Serviceable && HashMatchesNugetOrgStatus != HashMatchesNugetOrg.NoMatch;

	public bool IsRuntimePack => Type == "runtimepack";

	public bool IsProject => Type == "project";

	public bool HasNoRuntimeComponent => runtimeComponents == null || runtimeComponents.Length == 0;

	static string ConvertToAssemblyVersion(string ver)
	{
		var parts = ver.TrimStart('v').Split('-')[0].Split('+')[0].Trim().Split('.').ToList();
		for (int i = 0; i < parts.Count; i++)
		{
			if (!UInt64.TryParse(parts[i], out _))
			{
				parts = parts.Take(i).ToList();
				break;
			}
		}
		if (parts.Count == 0)
		{
			return "1.0.0.0";
		}
		while (parts.Count < 4)
		{
			parts.Add("0");
		}
		return string.Join(".", parts);
	}


	public DotNetCoreDepInfo(
		string fullName,
		string version,
		string type,
		bool serviceable,
		string sha512,
		string? path,
		string? hashPath,
		DotNetCoreDepInfo[] deps,
		RuntimeComponentInfo[] runtimeComponents,
		NativeComponentInfo[] nativeComponents,
		RuntimeComponentInfo? thisRuntimeComponent)
	{
		var parts = fullName.Split('/');
		this.Name = parts[0];
		if (parts.Length > 1)
		{
			this.Version = parts[1];
		}
		else
		{
			this.Version = version;
		}

		this.Type = type;
		this.Serviceable = serviceable;
		this.Path = path;
		this.HashPath = hashPath;
		this.Sha512 = sha512;

		this.deps = deps;
		this.runtimeComponents = runtimeComponents;
		this.nativeComponents = nativeComponents;
		this.ThisRuntimeComponent = thisRuntimeComponent;
	}

	static DotNetCoreDepInfo CreateFromJson(string fullName, string version, string target, JObject blob)
	{
		return Create(fullName, version, target, blob, []);
	}

	static System.Version? TryParseVersionOrDefault(string? version, string? defaultVersion)
	{
		if (string.IsNullOrEmpty(version) || !System.Version.TryParse(version, out var versionResult))
		{
			if (string.IsNullOrEmpty(defaultVersion) || !System.Version.TryParse(defaultVersion, out versionResult))
			{
				return null;
			}
		}
		return versionResult;
	}

	static DotNetCoreDepInfo Create(string fullName, string version, string target, JObject blob,
		Dictionary<string, DotNetCoreDepInfo> _deps)
	{
		var parts = fullName.Split('/');
		var Name = parts[0];
		var Version = "<UNKNOWN>";
		if (parts.Length > 1)
		{
			Version = parts[1];
		}
		else
		{
			Version = version;
		}

		string type = "runtimedll";
		bool serviceable = false;
		string sha512 = "";
		string? path = null;
		string? hashPath = null;
		var libraryBlob = blob["libraries"]?[Name + "/" + Version] as JObject;
		if (libraryBlob != null)
		{
			type = libraryBlob["type"]?.ToString() ?? type;
			serviceable = libraryBlob["serviceable"]?.Value<bool>() ?? serviceable;
			sha512 = libraryBlob["sha512"]?.ToString() ?? "";
			path = libraryBlob["path"]?.ToString();
			hashPath = libraryBlob["hashPath"]?.ToString();
		}

		var runtimeComponents = new List<RuntimeComponentInfo>();
		var runtimeBlob = blob["targets"]?[target]?[Name + "/" + Version]?["runtime"] as JObject;
		RuntimeComponentInfo? thisRuntimeComponent = null;
		if (runtimeBlob != null)
		{
			foreach (var prop in runtimeBlob.Properties())
			{
				var name = System.IO.Path.GetFileNameWithoutExtension(prop.Name);
				bool isThisAssembly = name == Name;
				var directory = System.IO.Path.GetDirectoryName(prop.Name);
				var extension = System.IO.Path.GetExtension(prop.Name);
				var assemblyVersion = TryParseVersionOrDefault(
					prop.Value["assemblyVersion"]?.ToString(),
					isThisAssembly ? ConvertToAssemblyVersion(Version) : null
				);
				var fileVersion = TryParseVersionOrDefault(
					prop.Value["fileVersion"]?.ToString(),
					isThisAssembly ? ConvertToAssemblyVersion(Version) : null
				);
				var runtimeComponent = new RuntimeComponentInfo(name, extension, directory, assemblyVersion, fileVersion);
				runtimeComponents.Add(runtimeComponent);
				if (isThisAssembly)
				{
					thisRuntimeComponent = runtimeComponent;
				}
			}
		}

		// string[] nativeComponents = Array.Empty<string>();
		var nativeBlob = blob["targets"]?[target]?[Name + "/" + Version]?["native"] as JObject;
		var nativeComponents = new List<NativeComponentInfo>();
		if (nativeBlob != null)
		{
			foreach (var prop in nativeBlob.Properties())
			{
				var name = System.IO.Path.GetFileNameWithoutExtension(prop.Name);
				var extension = System.IO.Path.GetExtension(prop.Name);
				var directory = System.IO.Path.GetDirectoryName(prop.Name);
				var fileVersion = TryParseVersionOrDefault(
					prop.Value["fileVersion"]?.ToString(),
					null
				);
				nativeComponents.Add(new NativeComponentInfo(name, extension, directory, fileVersion));
			}
		}
		var deps = getDeps(Name, Version, target, blob, _deps);
		return new DotNetCoreDepInfo(Name, Version, type, serviceable, sha512, path, hashPath, deps, runtimeComponents.ToArray(), nativeComponents.ToArray(), thisRuntimeComponent);
	}


	static DotNetCoreDepInfo[] getDeps(string Name, string Version, string target, JObject blob,
		Dictionary<string, DotNetCoreDepInfo>? _deps = null)
	{
		if (_deps == null)
		{
			_deps = [];
		}

		var targetBlob = blob["targets"]?[target] as JObject;
		if (targetBlob == null)
		{
			return Empty<DotNetCoreDepInfo>.Array;
		}

		var depsBlob = targetBlob[Name + "/" + Version]?["dependencies"] as JObject;
		var runtimeBlob = targetBlob[Name + "/" + Version]?["runtime"] as JObject;
		if (depsBlob == null && runtimeBlob == null)
		{
			return Empty<DotNetCoreDepInfo>.Array;
		}

		List<DotNetCoreDepInfo> result = new List<DotNetCoreDepInfo>();
		Dictionary<String, String> deps = new Dictionary<string, string>();
		if (depsBlob != null)
		{
			foreach (var dep in depsBlob)
			{
				var dep_key = dep.Key + "/" + dep.Value?.ToString();
				if (_deps.ContainsKey(dep_key))
				{
					result.Add(_deps[dep_key]);
				}
				else
				{
					var new_dep = Create(dep.Key, dep.Value?.ToString() ?? string.Empty, target, blob, _deps);
					_deps.Add(dep_key, new_dep);
					result.Add(new_dep);
				}
			}
		}

		return result.ToArray();
	}

	public bool Equals(DotNetCoreDepInfo? other)
	{
		return other != null
			&& Name == other.Name
			&& Version == other.Version
			&& Type == other.Type
			&& Serviceable == other.Serviceable
			&& Path == other.Path
			&& HashPath == other.HashPath
			&& Sha512 == other.Sha512
			&& deps.SequenceEqual(other.deps)
			&& runtimeComponents.SequenceEqual(other.runtimeComponents)
			&& nativeComponents.SequenceEqual(other.nativeComponents)
			&& (ThisRuntimeComponent?.Equals(other.ThisRuntimeComponent) ?? other.ThisRuntimeComponent == null);
	}

	private List<DotNetCoreDepInfo> GetAllDeps(bool followProjectReferences, string? type, HashSet<DotNetCoreDepInfo>? seen)
	{
		var result = new HashSet<DotNetCoreDepInfo>();

		result.AddRange(string.IsNullOrEmpty(type) ? deps : deps.Where(d => d.Type == type));
		var unseenDeps = deps.Where(d => !seen?.Contains(d) ?? true);
		seen ??= [this, .. deps];
		foreach (var dep in unseenDeps)
		{
			if (!followProjectReferences && dep.Type == "project")
			{
				continue;
			}
			var found = dep.GetAllDeps(followProjectReferences, type, seen);
			result.AddRange(found);
			seen.AddRange(found);
		}
		return result.ToList();
	}

	public List<DotNetCoreDepInfo> GetAllDeps(bool followProjectReferences, string? type = null) {
		return GetAllDeps(followProjectReferences, type, null);
	}


	public bool Matches(IAssemblyReference? reference)
	{
		return reference != null && reference.Name == Name && reference.Version == AssemblyVersion;
	}

	private bool HasDepInternal(IAssemblyReference reference, string? type, bool serviceableAndNuGetOnly, bool followProjectReferences)
	{
		bool ShouldFilter(DotNetCoreDepInfo dep) => !((string.IsNullOrEmpty(type) || dep.Type == type) && (!serviceableAndNuGetOnly || dep.IsAvailableOnNuget));
		if (runtimeComponents.Any(c => c.Matches(reference) && !c.Equals(ThisRuntimeComponent)) && !ShouldFilter(this))
		{
			return true;
		}
		return deps.Any(
			d => !ShouldFilter(d) &&
				(d.Matches(reference) ||
					((followProjectReferences || d.Type != "project")
						&& d.HasDepInternal(reference, type, serviceableAndNuGetOnly, followProjectReferences))));
	}

	public bool HasAnyDep(IAssemblyReference reference, string? type, bool followProjectReferences)
	{
		return HasDepInternal(reference, type, false, followProjectReferences);
	}

	public bool HasServicableDep(IAssemblyReference reference, string? type, bool followProjectReferences)
	{
		return HasDepInternal(reference, type, true, followProjectReferences);
	}

	public DotNetCoreDepInfo? GetDep(IAssemblyReference reference, bool followProjectReferences)
	{
		return GetAllDeps(followProjectReferences).FirstOrDefault(d => d.Matches(reference));
	}

	public static string GetDepPath(string assemblyPath)
	{
		return System.IO.Path.ChangeExtension(assemblyPath, ".deps.json");
	}


	public static DotNetCoreDepInfo? LoadDepInfoFromFile(string depsJsonFileName, string moduleName)
	{
		// remove the .dll extension
		if (string.IsNullOrEmpty(depsJsonFileName) || !System.IO.File.Exists(depsJsonFileName))
		{
			return null;
		}
		var depsJson = File.ReadAllText(depsJsonFileName);
		var dependencies = JObject.Parse(depsJson);
		// go through each target framework, find the one that matches the module
		var targets = dependencies["targets"] as JObject;
		if (targets != null)
		{
			foreach (var target in targets)
			{
				var targetObj = target.Value as JObject;
				if (targetObj == null)
					continue;
				foreach (var dependency in targetObj)
				{
					if (dependency.Key.StartsWith(moduleName))
					{
						return DotNetCoreDepInfo.CreateFromJson(dependency.Key, "", target.Key, dependencies);
					}
				}
			}
		}

		return null;
	}

	public async Task StartResolvePackageAndCheckHash(bool checkOnline, CancellationToken cancellationToken)
	{
		if (!Serviceable || Type != "package" || string.IsNullOrEmpty(Sha512) || !Sha512.StartsWith("sha512-"))
		{
			// only resolve packages that are serviceable and of type package
			HashMatchesNugetOrgStatus = HashMatchesNugetOrg.Unknown;
			return;
		}

		string? hash;
		try
		{
			hash = await NugetDetails.ResolvePackageAndGetContentHash(Name, Version, checkOnline, cancellationToken);
		}
		catch (HttpRequestException e) when (e.StatusCode == HttpStatusCode.NotFound)
		{
			// If the package/version is missing from nuget.org, treat it as a definitive mismatch.
			HashMatchesNugetOrgStatus = HashMatchesNugetOrg.NoMatch;
			return;
		}
		if (hash == null)
		{
			HashMatchesNugetOrgStatus = HashMatchesNugetOrg.Unknown;
		}
		else if (hash == Sha512)
		{
			HashMatchesNugetOrgStatus = HashMatchesNugetOrg.Match;
		}
		else
		{
			HashMatchesNugetOrgStatus = HashMatchesNugetOrg.NoMatch;
		}

	}
}

