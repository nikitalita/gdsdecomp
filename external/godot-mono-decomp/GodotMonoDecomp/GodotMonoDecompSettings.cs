

using ICSharpCode.Decompiler;
using ICSharpCode.Decompiler.CSharp;

namespace GodotMonoDecomp;

public class GodotMonoDecompSettings : DecompilerSettings
{
	/// <summary>
	/// Whether to write NuGet package references to the project file if dependency information is available.
	/// </summary>
	public bool WriteNuGetPackageReferences { get; set; } = true;

	/// <summary>
	/// Whether to copy out-of-tree references (i.e. references that
	/// are not within the same directory structure as the project file) to the project file.
	/// </summary>
	public bool CopyOutOfTreeReferences { get; set; } = true;

	/// <summary>
	/// Whether to create additional projects for project references in main module.
	/// </summary>
	public bool CreateAdditionalProjectsForProjectReferences { get; set; } = true;

	/// <summary>
	/// Godot version override for writing the SDK string in the project file.
	/// </summary>
	public Version? GodotVersionOverride { get; set; } = null;

	public GodotMonoDecompSettings() : base()
	{
		UseNestedDirectoriesForNamespaces = true;
	}

	public GodotMonoDecompSettings(LanguageVersion languageVersion) : base(languageVersion)
	{
		UseNestedDirectoriesForNamespaces = true;
	}


	public new GodotMonoDecompSettings Clone()
	{
		var settings = (GodotMonoDecompSettings) base.Clone();
		settings.WriteNuGetPackageReferences = WriteNuGetPackageReferences;
		settings.CopyOutOfTreeReferences = CopyOutOfTreeReferences;
		settings.CreateAdditionalProjectsForProjectReferences = CreateAdditionalProjectsForProjectReferences;
		return settings;
	}

}
