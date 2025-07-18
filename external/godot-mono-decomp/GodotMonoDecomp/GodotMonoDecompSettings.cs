

using ICSharpCode.Decompiler;

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
}
