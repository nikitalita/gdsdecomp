// C++ wrapper for GodotMonoDecomp
#pragma once

#include "core/string/ustring.h"
class GodotMonoDecompWrapper {
public:
	GodotMonoDecompWrapper(const String &assemblyPath, const Vector<String> &originalProjectFiles, const Vector<String> &assemblyReferenceDirs);
	~GodotMonoDecompWrapper();

	Error decompile_module(const String &outputCSProjectPath);
	String decompile_individual_file(const String &file);
	Vector<String> get_files_not_present_in_file_map();

private:
	void *decompilerHandle;
};