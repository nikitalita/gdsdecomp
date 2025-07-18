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
using System.Xml;
using ICSharpCode.Decompiler.CSharp.ProjectDecompiler;
using ICSharpCode.Decompiler.Metadata;
using ICSharpCode.Decompiler.Util;

using LightJson;
using LightJson.Serialization;

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

		static readonly HashSet<string> ImplicitGodotReferences = new HashSet<string> {
			"GodotSharp",
			"Godot.SourceGenerators",
			"GodotSharpEditor",
			"Godot.NET.Sdk",
			"Godot.NET.Sdk.Editor",
		};

		enum ProjectType { Default, WinForms, Wpf, Web, Godot }


		class ProjectFileWriterGodotStyleSettings
		{
			public bool writePackageReferences = true;
			public bool copyOutOfTreeRefsToOutputDir = true;
			public bool writeProjectReferences = true;
		}

		readonly ProjectFileWriterGodotStyleSettings settings;

		public DotNetCoreDepInfo? DepInfo;

		public ProjectFileWriterGodotStyle(bool writePackageReferences, bool copyRelativePackageRefsToOutputDir, bool createAdditionalProjectsForProjectReferences)
		{
			this.settings = new ProjectFileWriterGodotStyleSettings() {
				writePackageReferences = writePackageReferences,
				copyOutOfTreeRefsToOutputDir = copyRelativePackageRefsToOutputDir,
				writeProjectReferences = createAdditionalProjectsForProjectReferences
			};
		}

		/// <summary>
		/// Creates a new instance of the <see cref="ProjectFileWriterSdkStyle"/> class.
		/// </summary>
		/// <returns>A new instance of the <see cref="ProjectFileWriterSdkStyle"/> class.</returns>
		public static IProjectFileWriter Create(bool writeNugetRefs = true, bool copyRelativePackageRefsToOutputDir = true, bool createAdditionalProjectsForProjectReferences = true) =>
			new ProjectFileWriterGodotStyle(writeNugetRefs, copyRelativePackageRefsToOutputDir, createAdditionalProjectsForProjectReferences);

		/// <inheritdoc />
		public void Write(
			TextWriter target,
			IProjectInfoProvider project,
			IEnumerable<ICSharpCode.Decompiler.CSharp.ProjectDecompiler.ProjectItemInfo> files,
			MetadataFile module)
		{
			using (XmlTextWriter xmlWriter = new XmlTextWriter(target))
			{
				xmlWriter.Formatting = Formatting.Indented;
				Write(xmlWriter, project, files, module, DepInfo, settings);
			}
		}

		static void Write(XmlTextWriter xml, IProjectInfoProvider project, IEnumerable<ICSharpCode.Decompiler.CSharp.ProjectDecompiler.ProjectItemInfo> files,
			MetadataFile module, DotNetCoreDepInfo? depInfo, ProjectFileWriterGodotStyleSettings settings)
		{
			xml.WriteStartElement("Project");
			var projectType = GetProjectType(module);
			var sdkString = GetSdkString(projectType);
			if (projectType == ProjectType.Godot)
			{
				var gdver = GodotStuff.GetGodotVersion(module);
				if (gdver == null)
				{
					Console.Error.WriteLine($"Could not determine Godot version for module {module.FileName}, assuming 3.0");
					gdver = new Version(3, 0, 0);
				}
				var godotVersion = gdver?.ToString();
				// GodotSharp for 3.x always wrote the version number as "1.0.0" in the project file
				if (gdver == null || gdver.Major < 3)
				{
					// we'll indicate that this is a Godot 3.x project but use an invalid version number to force the editor to rewrite the project file
					godotVersion = "3.x.x";
				}
				if (godotVersion?.Count(f => f == '.') >= 3)
				{
					godotVersion = godotVersion.Substring(0, godotVersion.LastIndexOf('.'));
				}
				sdkString = sdkString + "/" + godotVersion;
			}

			xml.WriteAttributeString("Sdk", sdkString);

			PlaceIntoTag("PropertyGroup", xml, () => WriteAssemblyInfo(xml, module, project, projectType));
			PlaceIntoTag("PropertyGroup", xml, () => WriteProjectInfo(xml, project));
			PlaceIntoTag("PropertyGroup", xml, () => WriteMiscellaneousPropertyGroup(xml, files));
			PlaceIntoTag("ItemGroup", xml, () => WriteResources(xml, files));
			if (settings.writeProjectReferences && depInfo != null)
			{
				PlaceIntoTag("ItemGroup", xml, () => WriteProjectReferences(xml, module, project, projectType, depInfo, settings));
			}
			if (settings.writePackageReferences && depInfo != null){
				PlaceIntoTag("ItemGroup", xml,
					() => WritePackageReferences(xml, module, project, projectType, depInfo, settings));
			}

			PlaceIntoTag("ItemGroup", xml,
				() => WriteReferences(xml, module, project, projectType, depInfo, settings));

			xml.WriteEndElement();
		}

		static bool IsImplicitReference(string name)
		{

			return ImplicitReferences.Contains(name) || name.StartsWith("runtimepack") ||
			       ImplicitGodotReferences.Contains(name);
		}

		static void WritePackageReferences(XmlTextWriter xml, MetadataFile module, IProjectInfoProvider project,
			ProjectType projectType, DotNetCoreDepInfo? deps, ProjectFileWriterGodotStyleSettings settings)
		{
			void WritePackageRefs(XmlTextWriter xml)
			{
				foreach (var dep in deps.deps)
				{
					if (IsImplicitReference(dep.Name) || dep.Serviceable == false || dep.Type != "package")
					{
						continue;
					}

					xml.WriteStartElement("PackageReference");
					xml.WriteAttributeString("Include", dep.Name);
					xml.WriteAttributeString("Version", dep.Version);
					xml.WriteEndElement();
				}
			}

			if (deps == null || deps.deps.Length == 0)
			{
				return;
			}

			if (settings.writePackageReferences)
			{
				WritePackageRefs(xml);
			}
			else
			{
				writeBlockComment(xml, WritePackageRefs, "Uncomment these to download the nuget packages.");
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
			if (GetProjectType(module) == ProjectType.Godot)
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
			// Actually, we don't.
			// xml.WriteElementString("GenerateAssemblyInfo", FalseString);
			xml.WriteElementString("EnableDynamicLoading", TrueString);


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

		static void WriteProjectInfo(XmlTextWriter xml, IProjectInfoProvider project)
		{
			xml.WriteElementString("LangVersion",
				project.LanguageVersion.ToString().Replace("CSharp", "").Replace('_', '.'));
			xml.WriteElementString("AllowUnsafeBlocks", TrueString);

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

		static void WriteProjectReferences(XmlTextWriter xml, MetadataFile module, IProjectInfoProvider project,
			ProjectType projectType, DotNetCoreDepInfo? deps, ProjectFileWriterGodotStyleSettings settings)
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
					// TODO: Pass in a hashmap with the map of project names to paths;
					// right now we're forcing project references to be in a subdirectory named after the project
					xml.WriteAttributeString("Include", Path.Join(dep.Name, dep.Name + ".csproj"));
					xml.WriteEndElement();
				}
			}
		}

		static void WriteReferences(XmlTextWriter xml, MetadataFile module, IProjectInfoProvider project,
			ProjectType projectType, DotNetCoreDepInfo? deps, ProjectFileWriterGodotStyleSettings settings)
		{
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

			List<AssemblyReference> godotSharpRefs = new List<AssemblyReference>();

			List<AssemblyReference> packageReferences = new List<AssemblyReference>();

			List<AssemblyReference> projectReferences = new List<AssemblyReference>();

			HashSet<string> seenRefs = new HashSet<string>();


			foreach (var reference in module.AssemblyReferences.Where(r => !ImplicitReferences.Contains(r.Name)))
			{
				if (isNetCoreApp &&
				    project.AssemblyReferenceClassifier.IsSharedAssembly(reference, out string? runtimePack) &&
				    targetPacks.Contains(runtimePack))
				{
					continue;
				}

				if (ImplicitGodotReferences.Contains(reference.Name))
				{
					godotSharpRefs.Add(reference);
					continue;
				}

				if (DepExistsInPackages(reference))
				{
					packageReferences.Add(reference);
					continue;
				}

				if (IsProjectReference(reference))
				{
					projectReferences.Add(reference);
					continue;
				}

				WriteRef(xml, reference, true);
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

			bool IsProjectReference(AssemblyReference reference)
			{
				return settings.writeProjectReferences && deps != null && deps.HasDep(reference.Name, "project", false);
			}

			bool DepExistsInPackages(AssemblyReference reference)
			{
				return settings.writePackageReferences && deps != null && deps.HasDep(reference.Name, "package", true);
			}

			string GetNewRefOutputPath(string path)
			{
				string monoPart;
				if (path.Contains("/.mono"))
				{
					// get the part of the path that begins with ".mono"
					monoPart = path.Substring(path.IndexOf("/.mono") + 1);
					// copy the file to the output directory
				}
				else if (path.Contains("\\.mono")) {
					monoPart = path.Substring(path.IndexOf("\\.mono") + 1);
				}
				else
				{
					monoPart = Path.Combine(".mono", "referenced_assemblies", Path.GetFileName(path));
				}
				return Path.Combine(project.TargetDirectory, monoPart);
			}

			void CopyRef(MetadataFile asembly, string outputPath)
			{
				if (!seenRefs.Add(outputPath))
				{
					// we already copied this reference
					return;
				}

				// if it already exists, we don't need to copy it
				if (!File.Exists(outputPath)) {
					try
					{
						_ = Directory.CreateDirectory(Path.GetDirectoryName(outputPath));
					}
					catch (Exception e)
					{
						Console.Error.WriteLine($"Error creating directory {Path.GetDirectoryName(outputPath)}: {e.Message}");
					}
					// copy the file to the output directory
					try
					{
						File.Copy(asembly.FileName, outputPath, true);
					}
					catch (Exception e)
					{
						Console.Error.WriteLine($"Error copying file {asembly.FileName} to {outputPath}: {e.Message}");
					}
					// use the relative path to the output directory
				}
				// check if a pdb exists at the original path
				var pdbPath = Path.ChangeExtension(asembly.FileName, ".pdb");
				var pdbOutputPath = Path.ChangeExtension(outputPath, ".pdb");
				if (File.Exists(pdbPath) && !File.Exists(pdbOutputPath))
				{
					// copy the pdb file to the output directory
					try
					{
						File.Copy(pdbPath, pdbOutputPath);
					}
					catch (Exception e)
					{
						Console.Error.WriteLine($"Error copying pdb file {pdbPath} to {pdbOutputPath}: {e.Message}");
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

			void WriteRef(XmlTextWriter newXml, AssemblyReference reference, bool realRef)
			{
				newXml.WriteStartElement("Reference");
				newXml.WriteAttributeString("Include", reference.Name);

				var asembly = project.AssemblyResolver.Resolve(reference);
				if (asembly != null && !project.AssemblyReferenceClassifier.IsGacAssembly(reference))
				{
					var path = FileUtility.GetRelativePath(project.TargetDirectory, asembly.FileName);
					if (realRef && settings.copyOutOfTreeRefsToOutputDir && path.StartsWith(".."))
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
