// Copyright (c) 2020 Siegfried Pammer
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
// to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

using System.Reflection.PortableExecutable;
using System.Runtime.Loader;
using System.Xml;
using GodotMonoDecomp;
using ICSharpCode.Decompiler.CSharp;
using ICSharpCode.Decompiler.CSharp.ProjectDecompiler;
using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.Decompiler.Util;
using NuGet.Configuration;


public interface IGodotProjectWithSettingsProvider : IProjectInfoProvider
{
	/// <summary>
	/// Gets the settings for the project.
	/// </summary>
	GodotMonoDecompSettings Settings { get; }

	// DotNetCoreDepInfo? DepInfo { get; }
	public Dictionary<string, string> SubProjectMap { get; }

	public string ProjectCSProjPath { get; }

	public bool CustomVersionDetected { get; }

	Version? GodotVersionOverride { get; }

}

namespace GodotMonoDecomp
{
	/// <summary>
	/// A <see cref="ProjectFileWriterGodotStyle"/> implementation that creates the projects in the Godot SDK style format.
	/// </summary>
	class ProjectFileWriterGodotStyle : IProjectFileWriter
	{
		const string AspNetCorePrefix = "Microsoft.AspNetCore";
		const string PresentationFrameworkName = "PresentationFramework";
		const string WindowsFormsName = "System.Windows.Forms";
		const string TrueString = "True";
		const string FalseString = "False";
		const string EnableString = "enable";
		const string AnyCpuString = "AnyCPU";

		static readonly HashSet<string> ImplicitReferences = new HashSet<string> {
			"mscorlib",
			"netstandard",
			"PresentationFramework",
			"System",
			"System.Diagnostics.Debug",
			"System.Diagnostics.Tools",
			"System.Drawing",
			"System.Runtime",
			"System.Runtime.Extensions",
			"System.Windows.Forms",
			"System.Xaml",
		};

		public static readonly HashSet<string> ImplicitGodotReferences = new HashSet<string> {
			"GodotSharp",
			"Godot.SourceGenerators",
			"GodotSharpEditor",
			"Godot.NET.Sdk",
			"Godot.NET.Sdk.Editor",
		};

		enum ProjectType { Default, WinForms, Wpf, Web, Godot }


		public DotNetCoreDepInfo? DepInfo;

		public ProjectFileWriterGodotStyle()
		{
		}

		/// <summary>
		/// Creates a new instance of the <see cref="ProjectFileWriterGodotStyle"/> class.
		/// </summary>
		/// <returns>A new instance of the <see cref="ProjectFileWriterGodotStyle"/> class.</returns>
		public static IProjectFileWriter Create() => new ProjectFileWriterGodotStyle();

		/// <inheritdoc />
		public void Write(
			TextWriter target,
			IProjectInfoProvider project,
			IEnumerable<ICSharpCode.Decompiler.CSharp.ProjectDecompiler.ProjectItemInfo> files,
			MetadataFile module)
		{

			var actualProject = project as IGodotProjectWithSettingsProvider;
			if (actualProject == null)
			{
				throw new ArgumentException("Project must implement IGodotProjectWithSettingsProvider to use ProjectFileWriterGodotStyle.");
			}
			using (XmlTextWriter xmlWriter = new XmlTextWriter(target))
			{
				xmlWriter.Formatting = Formatting.Indented;
				Write(xmlWriter, actualProject, files, module, DepInfo, actualProject.Settings);
			}
		}

		static void Write(XmlTextWriter xml, IGodotProjectWithSettingsProvider project, IEnumerable<ICSharpCode.Decompiler.CSharp.ProjectDecompiler.ProjectItemInfo> files,
			MetadataFile module, DotNetCoreDepInfo? depInfo, GodotMonoDecompSettings settings)
		{
			xml.WriteStartElement("Project");
			var projectType = GetProjectType(module);
			var sdkString = GetSdkString(projectType);
			if (projectType == ProjectType.Godot)
			{
				var gdver = project.GodotVersionOverride ?? GodotStuff.GetGodotVersion(module);
				if (gdver == null)
				{
					Logging.Warn($"Could not determine Godot version for module {module.FileName}, assuming 3.0");
					gdver = new Version(3, 0, 0);
				}
				var godotVersion = gdver?.ToString(3);
				// if (project.CustomVersionDetected)
				// {
				// 	godotVersion = GodotStuff.GetGodotSharpPackageDep(depInfo)?.Version ?? godotVersion;
				// }
				// GodotSharp for 3.x always wrote the version number as "1.0.0" in the project file
				if (gdver == null || gdver.Major < 3)
				{
					// we'll indicate that this is a Godot 3.x project but use an invalid version number to force the editor to rewrite the project file
					godotVersion = "3.x.x";
				}
				else if (gdver.Major == 3)
				{
					// There were only two versions of the 3.x SDK; 3.2.3 and 3.3.0
					if (gdver.Minor < 3) // Godot 3.2.x and earlier
					{
						godotVersion = "3.2.3";
					}
					else // Godot 3.3.x and later
					{
						godotVersion = "3.3.0";
					}
				}
				sdkString = sdkString + "/" + godotVersion;
			}

			xml.WriteAttributeString("Sdk", sdkString);

			PlaceIntoTag("PropertyGroup", xml, () => WriteAssemblyInfo(xml, module, project, projectType));
			PlaceIntoTag("PropertyGroup", xml, () => WriteProjectInfo(xml, project, projectType));
			PlaceIntoTag("PropertyGroup", xml, () => WriteMiscellaneousPropertyGroup(xml, files));
			PlaceIntoTag("ItemGroup", xml, () => WriteResources(xml, files));
			if (settings.CreateAdditionalProjectsForProjectReferences && depInfo != null)
			{
				PlaceIntoTag("ItemGroup", xml, () => WriteProjectReferences(xml, module, project, projectType, depInfo, settings));
				HashSet<string> excludes = [];
				HashSet<string> alreadyProcessed = [];
				GetProjectReferenceExcludes(project, depInfo, excludes, alreadyProcessed);
				if (excludes.Count > 0)
				{
					PlaceIntoTag("ItemGroup", xml, () => {
						WriteCompileExcludes(xml, excludes);
					});
					PlaceIntoTag("ItemGroup", xml, () => {
						WriteEmbeddedResourceExcludes(xml, excludes);
					});
				}

			}
			HashSet<DotNetCoreDepInfo> includedDeps = [];
			if (depInfo != null){
				PlaceIntoTag("ItemGroup", xml,
					() => includedDeps = WritePackageReferences(xml, module, project, projectType, depInfo, settings));
			}

			PlaceIntoTag("ItemGroup", xml,
				() => WriteReferences(xml, module, project, projectType, depInfo, settings, includedDeps));

			xml.WriteEndElement();
		}

		static bool IsImplicitReference(string name)
		{

			return ImplicitReferences.Contains(name) || name.StartsWith("runtimepack") ||
			       ImplicitGodotReferences.Contains(name);
		}

		static HashSet<DotNetCoreDepInfo> WritePackageReferences(XmlTextWriter xml, MetadataFile module, IProjectInfoProvider project,
			ProjectType projectType, DotNetCoreDepInfo? deps, GodotMonoDecompSettings settings)
		{
			List<DotNetCoreDepInfo> excludedDepsToComment = new List<DotNetCoreDepInfo>();
			List<DotNetCoreDepInfo> nonNuGetOrgDepsToComment = new List<DotNetCoreDepInfo>();
			HashSet<DotNetCoreDepInfo> includedDeps = [];
			HashSet<DotNetCoreDepInfo> includeWarningComment = [];

			if (deps == null || deps.deps.Length == 0)
			{
				return [];
			}

			HashSet<DotNetCoreDepInfo> depsToAdd = [];

			foreach (var dep in deps?.deps ?? [])
			{
				// not a package reference, skip it
				if (IsImplicitReference(dep.Name) || dep.Serviceable == false || dep.Type != "package")
				{
					continue;
				}
				// We do not want to include source generator packages in the project file because they will attempt
				// to generate code which we already have decompiled and create build errors.
				// Check if it's a possible source generator package; it will have no runtime components
				if (dep.HasNoRuntimeComponent)
				{
					// double check to see if the module has a reference to this
					if (module.AssemblyReferences.Any(r => dep.Matches(r)))
					{
						includeWarningComment.Add(dep);
						includedDeps.Add(dep);
						continue;
					}
					excludedDepsToComment.Add(dep);
					if (dep.HashMatchesNugetOrgStatus == DotNetCoreDepInfo.HashMatchesNugetOrg.NoMatch)
					{
						continue;
					}
					// check if it has any dependencies that do have runtime components that the module has a reference to
					depsToAdd.AddRange(dep.deps.Where(d => !IsImplicitReference(d.Name) && !d.HasNoRuntimeComponent && d.IsAvailableOnNuget && module.AssemblyReferences.Any(r => d.Matches(r))));
					continue;
				}

				if (dep.HashMatchesNugetOrgStatus == DotNetCoreDepInfo.HashMatchesNugetOrg.NoMatch)
				{
					nonNuGetOrgDepsToComment.Add(dep);
					continue;
				}
				includedDeps.Add(dep);
			}
			includedDeps.AddRange(depsToAdd.Where(d => !includedDeps.Any(d2 => d2.HasServicableDep(d.AssemblyRef, "package", !settings.CreateAdditionalProjectsForProjectReferences))));

			void WritePackageRef(XmlTextWriter xml, DotNetCoreDepInfo dep)
			{
				if (settings.WriteNuGetPackageReferences && includeWarningComment.Contains(dep))
				{
					xml.WriteComment(
						"WARNING: Possible source generator package: has no listed runtime components, but the module has a reference to it.");
				}
				xml.WriteStartElement("PackageReference");
				xml.WriteAttributeString("Include", dep.Name);
				xml.WriteAttributeString("Version", dep.Version);
				xml.WriteEndElement();
			}

			void WritePackageRefs(XmlTextWriter xml, IEnumerable<DotNetCoreDepInfo> deps)
			{
				foreach (var dep in deps)
				{
					WritePackageRef(xml, dep);
				}
			}


			if (settings.WriteNuGetPackageReferences)
			{
				WritePackageRefs(xml, includedDeps);

			}
			else
			{
				writeBlockComment(xml, (newXml) => WritePackageRefs(newXml, includedDeps), "Uncomment these to download the nuget packages.");
			}
			if (excludedDepsToComment.Count > 0)
			{
				writeSeriesOfLineComments(xml, (newXml) => WritePackageRefs(newXml, excludedDepsToComment),
					"The following packages are not referenced by the assembly and may be source generators. Including these may cause build errors.");
			}
			if (nonNuGetOrgDepsToComment.Count > 0)
			{
				writeSeriesOfLineComments(xml, (newXml) => WritePackageRefs(newXml, nonNuGetOrgDepsToComment),
					"The following packages are not from nuget.org and may not be available. Including these may cause build errors.");
			}
			if (settings.WriteNuGetPackageReferences){
				return includedDeps;
			} else {
				return [];
			}
		}

		static void PlaceIntoTag(string tagName, XmlTextWriter xml, Action content)
		{
			xml.WriteStartElement(tagName);
			try
			{
				content();
			}
			finally
			{
				xml.WriteEndElement();
			}
		}

		static TargetFramework GetActualTargetFramework(MetadataFile module, IProjectInfoProvider project)
		{
			var targetFramework = TargetServices.DetectTargetFramework(module);
			if (GetProjectType(module) == ProjectType.Godot && targetFramework.VersionNumber < 500)
			{
				return new TargetFramework(".NETFramework", targetFramework.VersionNumber, targetFramework.Profile);
			}
			if (targetFramework.Identifier == ".NETFramework" && targetFramework.VersionNumber == 200)
				targetFramework = TargetServices.DetectTargetFrameworkNET20(module, project.AssemblyResolver, targetFramework);
			return targetFramework;
		}

		static void WriteAssemblyInfo(XmlTextWriter xml, MetadataFile module, IProjectInfoProvider project,
			ProjectType projectType)
		{
			xml.WriteElementString("AssemblyName", module.Name);

			// Since we create AssemblyInfo.cs manually, we need to disable the auto-generation
			// We only write this for non-Godot projects
			if (projectType != ProjectType.Godot)
			{
				xml.WriteElementString("GenerateAssemblyInfo", FalseString);
				xml.WriteElementString("GenerateTargetFrameworkAttribute", FalseString);
			}
			xml.WriteElementString("EnableDynamicLoading", TrueString);
			// This is only supported in C# 8.0 and above
			if (project.LanguageVersion >= LanguageVersion.CSharp8_0)
			{
				xml.WriteElementString("Nullable", EnableString);
			}


			string platformName;
			CorFlags flags;
			if (module is PEFile { Reader.PEHeaders: { PEHeader: not null, CorHeader: not null } headers } peFile)
			{
				WriteOutputType(xml, headers.IsDll, headers.PEHeader.Subsystem, projectType);
				platformName = TargetServices.GetPlatformName(peFile);
				flags = headers.CorHeader.Flags;
			}
			else
			{
				WriteOutputType(xml, isDll: true, Subsystem.Unknown, projectType);
				platformName = AnyCpuString;
				flags = 0;
			}

			// Force AnyCPU
			platformName = AnyCpuString;

			WriteDesktopExtensions(xml, projectType);

			var targetFramework = GetActualTargetFramework(module, project);

			xml.WriteElementString("TargetFramework", targetFramework.Moniker);

			// 'AnyCPU' is default, so only need to specify platform if it differs
			if (platformName != AnyCpuString)
			{
				xml.WriteElementString("PlatformTarget", platformName);
			}

			if (platformName == AnyCpuString && (flags & CorFlags.Prefers32Bit) != 0)
			{
				xml.WriteElementString("Prefer32Bit", TrueString);
			}
		}

		static void WriteOutputType(XmlTextWriter xml, bool isDll, Subsystem moduleSubsystem, ProjectType projectType)
		{
			if (!isDll)
			{
				switch (moduleSubsystem)
				{
					case Subsystem.WindowsGui:
						xml.WriteElementString("OutputType", "WinExe");
						break;
					case Subsystem.WindowsCui:
						xml.WriteElementString("OutputType", "Exe");
						break;
				}
			}
			else
			{
				// 'Library' is default, so only need to specify output type for executables (excludes ProjectType.Web)
				if (projectType == ProjectType.Web)
				{
					xml.WriteElementString("OutputType", "Library");
				}
			}
		}

		static void WriteDesktopExtensions(XmlTextWriter xml, ProjectType projectType)
		{
			if (projectType == ProjectType.Wpf)
			{
				xml.WriteElementString("UseWPF", TrueString);
			}
			else if (projectType == ProjectType.WinForms)
			{
				xml.WriteElementString("UseWindowsForms", TrueString);
			}
		}

		static void WriteProjectInfo(XmlTextWriter xml, IGodotProjectWithSettingsProvider project, ProjectType projectType)
		{
			xml.WriteElementString("LangVersion",
				project.LanguageVersion.ToString().Replace("CSharp", "").Replace('_', '.'));
			xml.WriteElementString("AllowUnsafeBlocks", TrueString);
			xml.WriteElementString("CheckForOverflowUnderflow", project.CheckForOverflowUnderflow ? TrueString : FalseString);
			if (project.CustomVersionDetected && projectType == ProjectType.Godot)
			{
				xml.WriteElementString("DisableImplicitGodotSharpReferences", TrueString);
			}

			if (project.StrongNameKeyFile != null)
			{
				xml.WriteElementString("SignAssembly", TrueString);
				xml.WriteElementString("AssemblyOriginatorKeyFile", Path.GetFileName(project.StrongNameKeyFile));
			}
		}

		static void WriteMiscellaneousPropertyGroup(XmlTextWriter xml, IEnumerable<ICSharpCode.Decompiler.CSharp.ProjectDecompiler.ProjectItemInfo> files)
		{
			var (itemType, fileName) = files.FirstOrDefault(t => t.ItemType == "ApplicationIcon");
			if (fileName != null)
				xml.WriteElementString("ApplicationIcon", fileName);

			(itemType, fileName) = files.FirstOrDefault(t => t.ItemType == "ApplicationManifest");
			if (fileName != null)
				xml.WriteElementString("ApplicationManifest", fileName);

			if (files.Any(t => t.ItemType == "EmbeddedResource"))
				xml.WriteElementString("RootNamespace", string.Empty);
			// TODO: We should add CustomToolNamespace for resources, otherwise we should add empty RootNamespace
		}

		static void WriteResources(XmlTextWriter xml, IEnumerable<ICSharpCode.Decompiler.CSharp.ProjectDecompiler.ProjectItemInfo> files)
		{
			// remove phase
			foreach (var item in files.Where(t => t.ItemType == "EmbeddedResource"))
			{
				string buildAction = Path.GetExtension(item.FileName).ToUpperInvariant() switch {
					".CS" => "Compile",
					".RESX" => "EmbeddedResource",
					_ => "None"
				};
				if (buildAction == "EmbeddedResource")
					continue;

				xml.WriteStartElement(buildAction);
				xml.WriteAttributeString("Remove", item.FileName);
				xml.WriteEndElement();
			}

			// include phase
			foreach (var item in files.Where(t => t.ItemType == "EmbeddedResource"))
			{
				if (Path.GetExtension(item.FileName) == ".resx")
					continue;

				xml.WriteStartElement("EmbeddedResource");
				xml.WriteAttributeString("Include", item.FileName);
				if (item.AdditionalProperties != null)
				{
					foreach (var (key, value) in item.AdditionalProperties)
						xml.WriteAttributeString(key, value);
				}

				xml.WriteEndElement();
			}
		}

		static void writeSeriesOfLineComments(XmlTextWriter oldXml, Action<XmlTextWriter> write, string? prefixComment = null)
		{
			var writer = new StringWriter();
			var xml = new XmlTextWriter(writer);
			xml.Indentation = oldXml.Indentation;
			xml.IndentChar = oldXml.IndentChar;
			xml.Formatting = oldXml.Formatting;
			xml.QuoteChar = oldXml.QuoteChar;
			write(xml);
			xml.Flush();
			var text = writer.ToString();
			if (text?.Length > 0)
			{
				if (prefixComment != null)
				{
					oldXml.WriteComment(prefixComment);
				}
				var lines = text.Split('\n');
				foreach (var line in lines)
				{
					oldXml.WriteComment(line);
				}
			}
		}

		// takes in a void function and returns a string
		static void writeBlockComment(XmlTextWriter oldXml, Action<XmlTextWriter> write, string? prefixComment = null)
		{
			var writer = new StringWriter();
			var xml = new XmlTextWriter(writer);
			xml.Indentation = oldXml.Indentation;
			xml.IndentChar = oldXml.IndentChar;
			xml.Formatting = oldXml.Formatting;
			xml.QuoteChar = oldXml.QuoteChar;
			write(xml);
			xml.Flush();
			var text = writer.ToString();
			if (text?.Length > 0)
			{
				if (prefixComment != null)
				{
					oldXml.WriteComment(prefixComment);
				}

				oldXml.WriteComment("\n" + text + "\n");
			}
		}

		static void WriteProjectReferences(XmlTextWriter xml, MetadataFile module, IGodotProjectWithSettingsProvider project,
			ProjectType projectType, DotNetCoreDepInfo? deps, GodotMonoDecompSettings settings)
		{
			if (deps == null)
			{
				return;
			}
			foreach (var dep in deps.deps)
			{
				if (dep.Type == "project")
				{
					xml.WriteStartElement("ProjectReference");
					var path = project.SubProjectMap.TryGetValue(dep.Name, out var p) ? p : Path.Join(dep.Name, dep.Name + ".csproj");
					var relativePath = FileUtility.GetRelativePath(Path.GetDirectoryName(project.ProjectCSProjPath), path);
					xml.WriteAttributeString("Include", relativePath);
					xml.WriteEndElement();
				}
			}
			return;
		}


		static void GetProjectReferenceExcludes(IGodotProjectWithSettingsProvider project, DotNetCoreDepInfo? deps, HashSet<string> excludes, HashSet<string> alreadyProcessed)
		{
			if (deps == null)
			{
				return;
			}

			alreadyProcessed.Add(deps.Name);
			foreach (var dep in deps.deps)
			{

				if (dep.Type == "project")
				{
					if (alreadyProcessed.Contains(dep.Name))
					{
						continue;
					}

					alreadyProcessed.Add(dep.Name);
					var path = project.SubProjectMap.TryGetValue(dep.Name, out var p) ? p : Path.Join(dep.Name, dep.Name + ".csproj");
					var relativePath = FileUtility.GetRelativePath(Path.GetDirectoryName(project.ProjectCSProjPath), path);
					// if relativePath is a subdirectory of the project directory, add it to the excludes
					if (!relativePath.StartsWith(".."))
					{
						var dirName = Path.GetDirectoryName(relativePath);
						if (dirName != null)
						{
							excludes.Add(dirName);
						}
					}
					// this is recursive, so we need to get the excludes for the referenced project as well
					GetProjectReferenceExcludes(project, dep, excludes, alreadyProcessed);
				}
			}
		}

		static void WriteCompileExcludes(XmlTextWriter xml, HashSet<string> excludes)
		{
			foreach (var exclude in excludes)
			{
				xml.WriteStartElement("Compile");
				xml.WriteAttributeString("Remove", Path.Join(exclude, "**"));
				xml.WriteEndElement();
			}
		}

		static void WriteEmbeddedResourceExcludes(XmlTextWriter xml, HashSet<string> excludes)
		{
			foreach (var exclude in excludes)
			{
				xml.WriteStartElement("EmbeddedResource");
				xml.WriteAttributeString("Remove", Path.Join(exclude, "**"));
				xml.WriteEndElement();
			}
		}

		static void WriteReferences(XmlTextWriter xml, MetadataFile module, IGodotProjectWithSettingsProvider project,
			ProjectType projectType, DotNetCoreDepInfo? deps, GodotMonoDecompSettings settings, HashSet<DotNetCoreDepInfo> writtenPackageDeps)
		{
			string copyToDir = Path.Combine(project.TargetDirectory, "_mono_referenced_assemblies");
			// hashset of copied assemblies
			bool isNetCoreApp = TargetServices.DetectTargetFramework(module).Identifier == ".NETCoreApp";
			var targetPacks = new HashSet<string>();
			if (isNetCoreApp)
			{
				targetPacks.Add("Microsoft.NETCore.App");
				switch (projectType)
				{
					case ProjectType.WinForms:
					case ProjectType.Wpf:
						targetPacks.Add("Microsoft.WindowsDesktop.App");
						break;
					case ProjectType.Web:
						targetPacks.Add("Microsoft.AspNetCore.App");
						targetPacks.Add("Microsoft.AspNetCore.All");
						break;
				}
			}

			HashSet<IAssemblyReference> godotSharpRefs = new HashSet<IAssemblyReference>();

			HashSet<IAssemblyReference> packageReferences = new HashSet<IAssemblyReference>();

			HashSet<IAssemblyReference> projectReferences = new HashSet<IAssemblyReference>();

			HashSet<string> seenRefs = new HashSet<string>();
			bool NotProjectRef(DotNetCoreDepInfo dep) => !settings.CreateAdditionalProjectsForProjectReferences || !dep.IsProject;

			List<IAssemblyReference> RefsToWrite = module.AssemblyReferences.Where(r => !ImplicitReferences.Contains(r.Name)).Select(IAssemblyReference (r) => r).ToList();

			var allDeps = deps?.GetAllDeps(!settings.CreateAdditionalProjectsForProjectReferences) ?? [];
			var runtimepackComponents = allDeps.Where(d => d.IsRuntimePack).SelectMany(d => d.runtimeComponents).ToHashSet() ?? [];
			var nonruntimepackComponents = allDeps.Where(d => !d.IsRuntimePack && NotProjectRef(d)).SelectMany(d => d.runtimeComponents).ToHashSet() ?? [];
			runtimepackComponents = [.. runtimepackComponents.Where(c => !nonruntimepackComponents.Contains(c))];


			foreach (var reference in RefsToWrite)
			{
				if (isNetCoreApp && (runtimepackComponents.Any(c => c.Matches(reference)) || (
				    project.AssemblyReferenceClassifier.IsSharedAssembly(reference, out string? runtimePack) &&
				    targetPacks.Contains(runtimePack))))
				{
					continue;
				}

				if (ImplicitGodotReferences.Contains(reference.Name))
				{
					if (project.CustomVersionDetected) {
						xml.WriteComment($"Implicitly-included assembly '{reference.Name}' is force-included because a custom version is detected");
					} else {
						godotSharpRefs.Add(reference);
						continue;
					}
				}
				else if (DepExistsInPackages(reference))
				{
					packageReferences.Add(reference);
					continue;
				}
				else if (IsProjectReference(reference))
				{
					projectReferences.Add(reference);
					continue;
				}
				else if (IsProjectPackageReference(reference))
				{
					projectReferences.Add(reference);
					continue;
				}
				else
				{
					Logging.Debug($"Reference '{reference.Name}' is not a package reference, project reference, or GodotSharp reference.");
				}

				var referenceDep = deps?.GetDep(reference, true);
				if (referenceDep?.HasNoRuntimeComponent ?? false) {
					xml.WriteComment($"Reference '{reference.Name}' has no runtime components, but the assembly makes a reference to it. This may be a source generator package.");
				}

				WriteRef(xml, reference, true);
				// if the reference is GodotSharp and there is no GodotSharpEditor reference, we have to add it manually
				if (reference.Name.Equals("GodotSharp", StringComparison.OrdinalIgnoreCase) && !module.AssemblyReferences.Any(r => r.Name.Equals("GodotSharpEditor", StringComparison.OrdinalIgnoreCase)))
				{
					WriteRef(xml, reference, true, "GodotSharpEditor");
				}
			}

			if (godotSharpRefs.Count > 0)
			{
				writeBlockComment(xml, (newXml) => {
						foreach (var reference in godotSharpRefs)
						{
							WriteRef(newXml, reference, false);
						}
					},
					"The following references were not added to the project file because they are automatically added by the Godot SDK.");
			}

			if (packageReferences.Count > 0)
			{
				writeBlockComment(xml, (newXml) => {
						foreach (var reference in packageReferences)
						{
							WriteRef(newXml, reference, false);
						}
					},
					"The following references were not added to the project file because they are part of the package references above.");
			}

			if (projectReferences.Count > 0)
			{
				writeBlockComment(xml, (newXml) => {
						foreach (var reference in projectReferences)
						{
							WriteRef(newXml, reference, false);
						}
					},
					"The following references were not added to the project file because they are part of the project references above.");
			}
			HashSet<IAssemblyReference> AdditionalRefs = GetAdditionalRefsToWrite(deps);
			if (AdditionalRefs.Count > 0) {
				writeBlockComment(xml, (newXml) => {
						foreach (var reference in AdditionalRefs)
						{
							// realRef is true because we want to copy the file to the output directory if necessary, but not include it in the assembly references
							WriteRef(newXml, reference, true);
						}
					},
					"The following references were found in the dependency manifest but the assembly makes no reference to them.");
			}


			bool IsProjectReference(IAssemblyReference reference)
			{
				return settings.CreateAdditionalProjectsForProjectReferences && deps != null && deps.HasAnyDep(reference, "project", true);
			}

			bool IsProjectPackageReference(IAssemblyReference reference)
			{
				if (!settings.CreateAdditionalProjectsForProjectReferences || deps == null || deps.HasAnyDep(reference, null, false))
				{
					return false;
				}
				return deps.GetAllDeps(true, "project").Any(d => d.HasAnyDep(reference, null, true));
			}

			bool DepExistsInPackages(IAssemblyReference reference)
			{
				return writtenPackageDeps.Any(d2 => d2.Matches(reference) || d2.HasServicableDep(reference, "package", true));
			}

			HashSet<IAssemblyReference> GetAdditionalRefsToWrite(DotNetCoreDepInfo? deps){
				if (deps == null){
					return [];
				}
				var DepsMatchingAssemblyReferences = deps?.deps.Where(d => module.AssemblyReferences.Any(r => d.Matches(r))).ToHashSet() ?? [];
				bool ShouldNotFilter(DotNetCoreDepInfo d){
					return !(
							DepExistsInPackages(d.AssemblyRef) ||
							IsProjectReference(d.AssemblyRef) ||
							IsImplicitReference(d.Name) ||
							d.HasNoRuntimeComponent ||
							DepsMatchingAssemblyReferences.Contains(d) ||
							DepsMatchingAssemblyReferences.Any(d2 => d2.HasAnyDep(d.AssemblyRef, null, true)));
				}
				return deps?.deps
					.Where(ShouldNotFilter)
					.Select(d => d.AssemblyRef as IAssemblyReference)
					.Where(ar => project.AssemblyResolver.Resolve(ar) != null)
					.ToHashSet() ?? [];
			}

			string GetNewRefOutputPath(string path)
			{
				return Path.Combine(copyToDir, Path.GetFileName(path));
			}

			void CopyRef(MetadataFile asembly, string outputPath)
			{
				if (!seenRefs.Add(outputPath))
				{
					// we already copied this reference
					return;
				}

				try
				{
					_ = Directory.CreateDirectory(Path.GetDirectoryName(outputPath) ?? "");
				}
				catch (Exception e)
				{
					Logging.Error($"Error creating directory {Path.GetDirectoryName(outputPath)}: {e.Message}");
				}
				// copy the file to the output directory
				try
				{
					File.Copy(asembly.FileName, outputPath, true);
				}
				catch (Exception e)
				{
					Logging.Error($"Error copying file {asembly.FileName} to {outputPath}: {e.Message}");
				}

				// create an empty file named ".gdignore" in the mono directory
				if (!File.Exists(Path.Combine(copyToDir, ".gdignore")))
				{
					try
					{
						if (!Directory.Exists(copyToDir))
						{
							Directory.CreateDirectory(copyToDir);
						}
						File.Create(Path.Combine(copyToDir, ".gdignore")).Close();
					}
					catch (Exception e)
					{
						Logging.Error($"Error creating .gdignore file in {copyToDir}: {e.Message}");
					}
				}

				// use the relative path to the output directory
				// check if a pdb exists at the original path
				var pdbPath = Path.ChangeExtension(asembly.FileName, ".pdb");
				var pdbOutputPath = Path.ChangeExtension(outputPath, ".pdb");
				if (File.Exists(pdbPath))
				{
					// copy the pdb file to the output directory
					try
					{
						File.Copy(pdbPath, pdbOutputPath);
					}
					catch (Exception e)
					{
						Logging.Error($"Error copying pdb file {pdbPath} to {pdbOutputPath}: {e.Message}");
					}
				}
				// we need to copy the dependencies too
				var asemblyDeps = asembly.AssemblyReferences.Where(
					r =>
						!IsImplicitReference(r.Name) &&
						!projectReferences.Contains(r) &&
						!packageReferences.Contains(r) &&
						!godotSharpRefs.Contains(r) &&
						!project.AssemblyReferenceClassifier.IsGacAssembly(r));
				foreach (var dep in asemblyDeps)
				{
					var depAssembly = project.AssemblyResolver.Resolve(dep);
					if (depAssembly != null)
					{
						var depOutputPath = GetNewRefOutputPath(depAssembly.FileName);
						CopyRef(depAssembly, depOutputPath);
					}
				}
			}

			void WriteRef(XmlTextWriter newXml, IAssemblyReference reference, bool realRef, string? nameOverride = null)
			{
				newXml.WriteStartElement("Reference");
				newXml.WriteAttributeString("Include", nameOverride ?? reference.Name);

				var asembly = project.AssemblyResolver.Resolve(reference);
				if (asembly != null && !project.AssemblyReferenceClassifier.IsGacAssembly(reference))
				{
					var path = FileUtility.GetRelativePath(project.TargetDirectory, asembly.FileName);
					if (realRef && settings.CopyOutOfTreeReferences && path.StartsWith(".."))
					{
						// we need to copy the file to the output directory
						// check if one of the directories in the path is ".mono"
						var outputPath = GetNewRefOutputPath(asembly.FileName);
						CopyRef(asembly, outputPath);

						path = FileUtility.GetRelativePath(project.TargetDirectory, outputPath);

					}
					newXml.WriteElementString("HintPath", path);
				}

				newXml.WriteEndElement();
			}
		}

		static string GetSdkString(ProjectType projectType)
		{
			switch (projectType)
			{
				case ProjectType.WinForms:
				case ProjectType.Wpf:
					return "Microsoft.NET.Sdk.WindowsDesktop";
				case ProjectType.Web:
					return "Microsoft.NET.Sdk.Web";
				case ProjectType.Godot:
					return "Godot.NET.Sdk";
				default:
					return "Microsoft.NET.Sdk";
			}
		}

		static ProjectType GetProjectType(MetadataFile module)
		{
			foreach (var referenceName in module.AssemblyReferences.Select(r => r.Name))
			{

				if (referenceName == "GodotSharp")
				{
					return ProjectType.Godot;
				}
			}

			foreach (var referenceName in module.AssemblyReferences.Select(r => r.Name))
			{
				if (referenceName.StartsWith(AspNetCorePrefix, StringComparison.Ordinal))
				{
					return ProjectType.Web;
				}

				if (referenceName == PresentationFrameworkName)
				{
					return ProjectType.Wpf;
				}

				if (referenceName == WindowsFormsName)
				{
					return ProjectType.WinForms;
				}
			}

			return ProjectType.Default;
		}
	}
}
