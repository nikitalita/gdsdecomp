using System.Text;
using System.Text.RegularExpressions;
using ICSharpCode.Decompiler.Metadata;

namespace GodotMonoDecomp;

public static partial class SteamHelpers
{
	private const string WorkshopContentMarker = "steamapps/workshop/content/";
	private const string SteamAppsDirName = "steamapps";

	[GeneratedRegex("""^\s*"installdir"\s*"(?<installdir>.*)"\s*$""", RegexOptions.Multiline | RegexOptions.Compiled, "en-US")]
	private static partial Regex InstallDirRegex();

	public static bool IsAssemblyPathInWorkshopFolder(string assemblyPath)
	{
		return GetWorkshopContentMarkerIndex(EnsureUnixSeparators(assemblyPath)) >= 0;
	}

	/// <summary>
	/// Workshop items are built against the assemblies of the game they mod, which live in the
	/// <c>data_*</c> directories of the game installation. Given the path to a workshop assembly, this
	/// resolves those directories by way of the app manifest of the owning app.
	/// </summary>
	public static string[] GetAdditionalAssemblySearchPathsForWorkshopModule(MetadataFile module, string[] existingSearchPaths)
	{
		var path = EnsureUnixSeparators(module.FileName);
		var markerIndex = GetWorkshopContentMarkerIndex(path);
		if (markerIndex < 0)
		{
			return [];
		}

		var steamAppsDir = path.Substring(0, markerIndex + SteamAppsDirName.Length);

		var appIdStart = markerIndex + WorkshopContentMarker.Length;
		var appIdEnd = path.IndexOf('/', appIdStart);
		if (appIdEnd < 0)
		{
			return [];
		}
		var appwsdir = path.Substring(0, appIdEnd + 1);


		// get the workshop id for this module
		// it's directly after the app id and before the first slash
		var workshopIdStart = appIdEnd + 1;
		var workshopIdEnd = path.IndexOf('/', workshopIdStart);
		if (workshopIdEnd < 0)
		{
			return [];
		}
		var workshopId = path.Substring(workshopIdStart, workshopIdEnd - workshopIdStart);
		if (workshopId.Length == 0 || !workshopId.All(char.IsAsciiDigit))
		{
			return [];
		}

		var appId = path.Substring(appIdStart, appIdEnd - appIdStart);
		if (appId.Length == 0 || !appId.All(char.IsAsciiDigit))
		{
			return [];
		}

		var installDir = GetInstallDirFromAppManifest(steamAppsDir, appId);
		if (string.IsNullOrEmpty(installDir))
		{
			return [];
		}

		var gameDir = Path.Combine(steamAppsDir, "common", installDir);
		if (!Directory.Exists(gameDir))
		{
			return [];
		}

		try
		{
			var ret = GetBaseLibSearchPaths(gameDir, appwsdir, appId);
			if (ret.Length == 0) {
				return ret;
			}
			return ret.Concat(GetAdditionalSearchPathsFromDependencies(module, appwsdir, [.. existingSearchPaths, .. ret]))
			.Where(path => !string.IsNullOrEmpty(path) && !existingSearchPaths.Contains(path))
			.ToArray();
		}
		catch (Exception e) when (e is IOException or UnauthorizedAccessException)
		{
			Logging.Error($"Failed to list the data directories of '{gameDir}': {e.Message}");
			return [];
		}
	}


	/// <summary>
	/// Workshop modules sometimes rely on additional assemblies in dependent workshop items
	/// TODO: No current way to detect which workshop items are dependencies of a given module, so we search all of them.
	/// Find a way to not do this.
	/// </summary>
	private static string[] GetAdditionalSearchPathsFromDependencies(MetadataFile module, string parentAppWorkshopDir, string[] paths)
	{
		var resolver = new UniversalAssemblyResolver(module.FileName, false, module.Metadata.DetectTargetFrameworkId());
		foreach (var path in paths)
		{
			resolver.AddSearchDirectory(path);
		}
		var missingRefs = module.AssemblyReferences.Where(r => resolver.Resolve(r) == null).ToArray();
		Dictionary<AssemblyReference, string[]> fileToPaths = RecursivelySearchDirectoryForAssemblies(parentAppWorkshopDir, missingRefs);
		HashSet<string> addedPaths = new();
		foreach (var (assemblyRef, additionalPaths) in fileToPaths)
		{
			foreach (var path in additionalPaths)
			{
				if (addedPaths.Contains(path) || paths.Contains(path))
				{
					continue;
				}
				resolver.AddSearchDirectory(path);
				var resolved = resolver.Resolve(assemblyRef);
				resolver.RemoveSearchDirectory(path);
				if (resolved != null)
				{
					addedPaths.Add(path);
					break;
				}
			}

		}
		return NormalizeReturnPaths(addedPaths);
	}

	private static Dictionary<AssemblyReference, string[]> RecursivelySearchDirectoryForAssemblies(string directory, AssemblyReference[] assemblyRefs)
	{
		var fileNames = assemblyRefs
			.Where(r => !string.IsNullOrEmpty(r.Name))
			.Select(r => (r, r.Name + ".dll"))
			.ToDictionary(entry => entry.Item2, entry => entry.Item1);
		var thing = Directory.EnumerateFiles(directory, "*.dll", SearchOption.AllDirectories)
			.Where(file => fileNames.ContainsKey(Path.GetFileName(file)))
			.Select(file => (fileNames[Path.GetFileName(file)], Path.GetDirectoryName(file) ?? ""))
			.Where(entry => !string.IsNullOrEmpty(entry.Item2))
			.ToDictionary(entry => entry.Item2, entry => entry.Item1);

		var ret = new Dictionary<AssemblyReference, string[]>();
		foreach (var entry in thing)
		{
			if (!ret.ContainsKey(entry.Value))
			{
				ret[entry.Value] = [];
			}
			ret[entry.Value] = [.. ret[entry.Value], entry.Key];
		}
		return ret;
	}

	private static string[] NormalizeReturnPaths(IEnumerable<string> paths)
	{
		return paths.Select(dir =>
		{
			try
			{
				return Path.GetFullPath(dir);
			}
			catch (Exception) // ignore errors
			{
				return "";
			}
		}).Where(dir => !string.IsNullOrEmpty(dir)).ToArray();
	}

	private static string[] GetBaseLibSearchPaths(string gameDir, string appwsdir, string appId)
	{
		try
		{
			var ret = Directory.EnumerateDirectories(gameDir)
				.Where(dir => Path.GetFileName(dir).StartsWith("data_", StringComparison.OrdinalIgnoreCase))
				.OrderBy(dir => dir, StringComparer.OrdinalIgnoreCase)
				.ToList();
			return NormalizeReturnPaths(ret.ToArray());
		}
		catch (Exception e) when (e is IOException or UnauthorizedAccessException)
		{
			Logging.Error($"Failed to list the data directories of '{gameDir}': {e.Message}");
			return [];
		}
	}

	private static string? GetInstallDirFromAppManifest(string steamAppsDir, string appId)
	{
		var manifestPath = Path.Combine(steamAppsDir, $"appmanifest_{appId}.acf");
		if (!File.Exists(manifestPath))
		{
			return null;
		}

		try
		{
			foreach (var line in File.ReadLines(manifestPath))
			{
				var match = InstallDirRegex().Match(line);
				if (match.Success)
				{
					return match.Groups["installdir"].Value;
				}
			}
		}
		catch (Exception e) when (e is IOException or UnauthorizedAccessException)
		{
			Logging.Error($"Failed to read the Steam app manifest '{manifestPath}': {e.Message}");
		}

		return null;
	}

	private static int GetWorkshopContentMarkerIndex(string normalizedPath)
	{
		return normalizedPath.IndexOf(WorkshopContentMarker, StringComparison.OrdinalIgnoreCase);
	}

	private static string EnsureUnixSeparators(string path)
	{
		return path.Replace('\\', '/');
	}

}
