using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.Decompiler.Util;
using LightJson;
using LightJson.Serialization;
using System;
using System.Collections.Generic;
using System.Linq;

namespace GodotMonoDecomp;

class DotNetCoreDepInfo
{
	public readonly string Name;
	public readonly string Version;
	public readonly string Type;
	public readonly string Path;
	public readonly string Sha512;
	public readonly bool Serviceable;
	public readonly DotNetCoreDepInfo[] deps;
	public readonly string[] runtimeComponents;

	public DotNetCoreDepInfo(string fullName, string version, string type, bool serviceable, string path,
		string sha512,
		DotNetCoreDepInfo[] deps, string[] runtimeComponents)
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
		this.Sha512 = sha512;

		this.deps = deps;
		this.runtimeComponents = runtimeComponents;
	}

	static public DotNetCoreDepInfo Create(string fullName, string version, string target, JsonObject blob)
	{
		return Create(fullName, version, target, blob, new HashSet<string>());
	}

	static DotNetCoreDepInfo Create(string fullName, string version, string target, JsonObject blob,
		HashSet<string> _deps)
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

		var type = "runtimedll";
		var serviceable = false;
		var path = "";
		var sha512 = "";
		var libraryBlob = blob["libraries"][Name + "/" + Version].AsJsonObject;
		if (libraryBlob != null)
		{
			type = libraryBlob["type"].AsString;
			serviceable = libraryBlob["serviceable"].AsBoolean;
			path = libraryBlob["path"].AsString ?? "";
			sha512 = libraryBlob["sha512"].AsString ?? "";
		}

		string[] runtimeComponents = Array.Empty<string>();
		var runtimeBlob = blob["targets"][target].AsJsonObject?[Name + "/" + Version].AsJsonObject?["runtime"]
			.AsJsonObject;
		if (runtimeBlob != null)
		{
			runtimeComponents = new string[runtimeBlob.Count];
			int i = 0;
			foreach (var component in runtimeBlob)
			{
				runtimeComponents[i] = System.IO.Path.GetFileNameWithoutExtension(component.Key);
				i++;
			}
		}

		var deps = getDeps(Name, Version, target, blob, _deps);
		return new DotNetCoreDepInfo(Name, Version, type, serviceable, path, sha512, deps, runtimeComponents);
	}


	static DotNetCoreDepInfo[] getDeps(string Name, string Version, string target, JsonObject blob,
		HashSet<string>? _deps = null)
	{
		if (_deps == null)
		{
			_deps = new HashSet<string>();
		}

		var targetBlob = blob["targets"][target].AsJsonObject;
		if (targetBlob == null)
		{
			return Empty<DotNetCoreDepInfo>.Array;
		}

		var depsBlob = targetBlob[Name + "/" + Version].AsJsonObject?["dependencies"].AsJsonObject;
		var runtimeBlob = targetBlob[Name + "/" + Version].AsJsonObject?["runtime"].AsJsonObject;
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
				if (!_deps.Add(dep.Key + "/" + dep.Value.AsString))
				{
					continue;
				}

				deps.Add(dep.Key, dep.Value.AsString);
			}
		}

		for (int i = 0; i < deps.Count; i++)
		{
			var dep = deps.ElementAt(i);
			var new_dep = Create(dep.Key, dep.Value, target, blob, _deps);
			result.Add(new_dep);
		}

		return result.ToArray();
	}

	public bool HasDep(string name, bool parentIsPackage, bool serviceableOnly = false)
	{
		if (runtimeComponents.Contains(name) && !((parentIsPackage && Type != "package") || (serviceableOnly && !Serviceable)))
		{
			return true;
		}
		for (int i = 0; i < deps.Length; i++)
		{
			if ((parentIsPackage && deps[i].Type != "package") || (serviceableOnly && !deps[i].Serviceable))
			{
				// skip non-package dependencies if parent is a package
				continue;
			}

			if (deps[i].Name == name)
			{
				return true;
			}

			if (deps[i].HasDep(name, false, false))
			{
				return true;
			}
		}

		return false;
	}

	public string PathName { get => System.IO.Path.Combine(Name, Version); }


	public static DotNetCoreDepInfo? LoadDepInfoFromFile(string depsJsonFileName, string moduleName)
	{
		// remove the .dll extension
		var depsJson = File.ReadAllText(depsJsonFileName);
		var dependencies = JsonReader.Parse(depsJson);
		// go through each target framework, find the one that matches the module
		foreach (var target in dependencies["targets"].AsJsonObject)
		{
			foreach (var dependency in target.Value.AsJsonObject)
			{
				if (dependency.Key.StartsWith(moduleName))
				{
					return DotNetCoreDepInfo.Create(dependency.Key, "", target.Key, dependencies.AsJsonObject);
				}
			}
		}

		return null;
	}

}

