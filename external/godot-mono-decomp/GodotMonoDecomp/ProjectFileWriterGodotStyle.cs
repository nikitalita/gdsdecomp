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

		enum ProjectType { Default, WinForms, Wpf, Web }

		readonly bool writePackageReferences = true;

		public ProjectFileWriterGodotStyle(bool writePackageReferences)
		{
			this.writePackageReferences = writePackageReferences;
		}

		/// <summary>
		/// Creates a new instance of the <see cref="ProjectFileWriterSdkStyle"/> class.
		/// </summary>
		/// <returns>A new instance of the <see cref="ProjectFileWriterSdkStyle"/> class.</returns>
		public static IProjectFileWriter Create(bool writeNugetRefs = true) =>
			new ProjectFileWriterGodotStyle(writeNugetRefs);

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
				Write(xmlWriter, project, files, module, this.writePackageReferences);
			}
		}

		static void Write(XmlTextWriter xml, IProjectInfoProvider project, IEnumerable<ICSharpCode.Decompiler.CSharp.ProjectDecompiler.ProjectItemInfo> files,
			MetadataFile module, bool writePackageReferences)
		{
			xml.WriteStartElement("Project");
			var deps = LoadDeps(module);
			var projectType = GetProjectType(module);
			var godotVersion = GetGodotVersion(deps);
			var sdkString = GetSdkString(projectType);
			if (godotVersion != "")
			{
				sdkString = sdkString + "/" + godotVersion;
			}

			xml.WriteAttributeString("Sdk", sdkString);

			PlaceIntoTag("PropertyGroup", xml, () => WriteAssemblyInfo(xml, module, project, projectType));
			PlaceIntoTag("PropertyGroup", xml, () => WriteProjectInfo(xml, project));
			PlaceIntoTag("PropertyGroup", xml, () => WriteMiscellaneousPropertyGroup(xml, files));
			PlaceIntoTag("ItemGroup", xml, () => WriteResources(xml, files));
			PlaceIntoTag("ItemGroup", xml,
				() => WritePackageReferences(xml, module, project, projectType, deps, writePackageReferences));

			PlaceIntoTag("ItemGroup", xml,
				() => WriteReferences(xml, module, project, projectType, deps, writePackageReferences));

			xml.WriteEndElement();
		}

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
				HashSet<string> _deps = null)
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

			public bool HasDep(string name, bool parentIsPackage)
			{
				if (this.runtimeComponents.Contains(name))
				{
					return true;
				}
				for (int i = 0; i < deps.Length; i++)
				{
					if (parentIsPackage && (deps[i].Type != "package"))
					{
						continue;
					}
					
					if (deps[i].Name == name)
					{
						return true;
					}

					if (deps[i].HasDep(name, false))
					{
						return true;
					}
				}

				return false;
			}

			public string PathName { get => System.IO.Path.Combine(Name, Version); }
		}

		class DotNetCorePackageInfo
		{
			public readonly string Name;
			public readonly string Version;
			public readonly string Type;
			public readonly string Path;
			public readonly string[] RuntimeComponents;

			public DotNetCorePackageInfo(string fullName, string type, string path, string[] runtimeComponents)
			{
				var parts = fullName.Split('/');
				this.Name = parts[0];
				if (parts.Length > 1)
				{
					this.Version = parts[1];
				}
				else
				{
					this.Version = "<UNKNOWN>";
				}

				this.Type = type;
				this.Path = path;
				this.RuntimeComponents = runtimeComponents ?? Empty<string>.Array;
			}
		}

		static IEnumerable<DotNetCorePackageInfo> LoadPackageInfos(string depsJsonFileName, string targetFramework)
		{
			var dependencies = JsonReader.Parse(File.ReadAllText(depsJsonFileName));
			var runtimeInfos = dependencies["targets"][targetFramework].AsJsonObject;
			var libraries = dependencies["libraries"].AsJsonObject;
			if (runtimeInfos == null || libraries == null)
				yield break;
			foreach (var library in libraries)
			{
				var type = library.Value["type"].AsString;
				var path = library.Value["path"].AsString;
				var runtimeInfo = runtimeInfos[library.Key].AsJsonObject?["runtime"].AsJsonObject;
				string[] components = new string[runtimeInfo?.Count ?? 0];
				if (runtimeInfo != null)
				{
					int i = 0;
					foreach (var component in runtimeInfo)
					{
						components[i] = component.Key;
						i++;
					}
				}

				yield return new DotNetCorePackageInfo(library.Key, type, path, components);
			}
		}

		static DotNetCoreDepInfo LoadDeps(MetadataFile module)
		{
			// remove the .dll extension
			var depsJsonFileName = module.FileName.Substring(0, module.FileName.Length - 4) + ".deps.json";
			var dependencies = JsonReader.Parse(File.ReadAllText(depsJsonFileName));
			// go through each target framework, find the one that matches the module
			Dictionary<String, JsonValue> targetBlob = new Dictionary<string, JsonValue>();
			foreach (var target in dependencies["targets"].AsJsonObject)
			{
				foreach (var dependency in target.Value.AsJsonObject)
				{
					if (dependency.Key.StartsWith(module.Name))
					{
						return DotNetCoreDepInfo.Create(dependency.Key, "", target.Key, dependencies.AsJsonObject);
					}
				}
			}

			return null;
		}

		static void WritePackageReferences(XmlTextWriter xml, MetadataFile module, IProjectInfoProvider project,
			ProjectType projectType, DotNetCoreDepInfo deps, bool writePackageReferences)
		{
			void WritePackageRefs(XmlTextWriter xml)
			{
				foreach (var dep in deps.deps)
				{
					if (dep.Name == "GodotSharp" || dep.Name == "Godot.SourceGenerators" ||
					    dep.Name.StartsWith("runtimepack") || dep.Serviceable == false || dep.Type != "package")
					{
						continue;
					}

					xml.WriteStartElement("PackageReference");
					xml.WriteAttributeString("Include", dep.Name);
					xml.WriteAttributeString("Version", dep.Version);
					xml.WriteEndElement();
				}
			}

			if (deps.deps.Length == 0)
			{
				return;
			}

			if (writePackageReferences)
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

		static TargetFramework GetActualTargetFramework(MetadataFile module)
		{
			var framework = TargetServices.DetectTargetFramework(module);

			return new TargetFramework(".NETFramework", framework.VersionNumber, framework.Profile);
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
			if (module is PEFile { Reader.PEHeaders: var headers } peFile)
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

			var targetFramework = GetActualTargetFramework(module);

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
		static void writeBlockComment(XmlTextWriter oldXml, Action<XmlTextWriter> write, string prefixComment = null)
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


		static void WriteReferences(XmlTextWriter xml, MetadataFile module, IProjectInfoProvider project,
			ProjectType projectType, DotNetCoreDepInfo deps, bool writePackageReferences = false)
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

			List<AssemblyReference> commentedReferences = new List<AssemblyReference>();

			foreach (var reference in module.AssemblyReferences.Where(r => !ImplicitReferences.Contains(r.Name)))
			{
				if (isNetCoreApp &&
				    project.AssemblyReferenceClassifier.IsSharedAssembly(reference, out string runtimePack) &&
				    targetPacks.Contains(runtimePack))
				{
					continue;
				}

				if (reference.Name is "GodotSharp" or "Godot.SourceGenerators" or "GodotSharpEditor")
				{
					godotSharpRefs.Add(reference);
					continue;
				}

				if (writePackageReferences && deps.HasDep(reference.Name, true))
				{
					commentedReferences.Add(reference);
					continue;
				}

				WriteRef(xml, reference);
			}

			writeBlockComment(xml, (newXml) => {
					foreach (var reference in godotSharpRefs)
					{
						WriteRef(newXml, reference);
					}
				},
				"The following references were not added to the project file because they are automatically added by the Godot SDK.");

			writeBlockComment(xml, (newXml) => {
					foreach (var reference in commentedReferences)
					{
						WriteRef(newXml, reference);
					}
				},
				"The following references were not added to the project file because they are part of the package references above.");

			return;

			void WriteRef(XmlTextWriter newXml, AssemblyReference reference)
			{
				newXml.WriteStartElement("Reference");
				newXml.WriteAttributeString("Include", reference.Name);

				var asembly = project.AssemblyResolver.Resolve(reference);
				if (asembly != null && !project.AssemblyReferenceClassifier.IsGacAssembly(reference))
				{
					newXml.WriteElementString("HintPath",
						FileUtility.GetRelativePath(project.TargetDirectory, asembly.FileName));
				}

				newXml.WriteEndElement();
			}
		}

		static string GetGodotVersion(DotNetCoreDepInfo deps)
		{
			foreach (var reference in deps.deps)
			{
				if (reference.Name == "GodotSharp")
				{
					return reference.Version;
				}
			}

			return "";
		}

		static string GetSdkString(ProjectType projectType)
		{
			return "Godot.NET.Sdk";
		}

		static ProjectType GetProjectType(MetadataFile module)
		{
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