// C++ wrapper for GodotMonoDecomp
#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

struct DecompileModuleTaskData;
class GodotMonoDecompWrapper : public RefCounted {
	GDCLASS(GodotMonoDecompWrapper, RefCounted);

protected:
	static void _bind_methods();
	GodotMonoDecompWrapper();
	friend struct DecompileModuleTaskData;

public:
	struct GodotMonoDecompSettings {
		bool WriteNuGetPackageReferences = true;
		bool CopyOutOfTreeReferences = true;
		bool CreateAdditionalProjectsForProjectReferences = true;
		String GodotVersionOverride;
	};

	Error decompile_module(const String &outputCSProjectPath, const Vector<String> &excludeFiles);
	~GodotMonoDecompWrapper();

	static Ref<GodotMonoDecompWrapper> create(const String &assemblyPath, const Vector<String> &originalProjectFiles, const Vector<String> &assemblyReferenceDirs, const GodotMonoDecompSettings &settings);

	bool is_valid() const;

	Error decompile_module_with_progress(const String &outputCSProjectPath, const Vector<String> &excludeFiles);
	String decompile_individual_file(const String &file);
	Vector<String> get_files_not_present_in_file_map();
	Vector<String> get_all_strings_in_module();

private:
	String assembly_path;
	void *decompilerHandle;
};
