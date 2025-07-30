// C++ wrapper for GodotMonoDecomp
#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class GodotMonoDecompWrapper : public RefCounted {
	GDCLASS(GodotMonoDecompWrapper, RefCounted);

protected:
	static void _bind_methods();
	GodotMonoDecompWrapper();

public:
	~GodotMonoDecompWrapper();

	static Ref<GodotMonoDecompWrapper> create(const String &assemblyPath, const Vector<String> &originalProjectFiles, const Vector<String> &assemblyReferenceDirs);

	bool is_valid() const;

	Error decompile_module(const String &outputCSProjectPath, const Vector<String> &excludeFiles);
	String decompile_individual_file(const String &file);
	Vector<String> get_files_not_present_in_file_map();

private:
	String assembly_path;
	void *decompilerHandle;
};