#include "godot_mono_decomp_wrapper.h"
#include "godot_mono_decomp.h"

GodotMonoDecompWrapper::GodotMonoDecompWrapper(const String &assembly_path, const Vector<String> &originalProjectFiles, const Vector<String> &assemblyReferenceDirs) {
	CharString assembly_path_chrstr = assembly_path.utf8();
	const char *assembly_path_c = assembly_path_chrstr.get_data();
	String ref_path = assembly_path.get_base_dir();
	CharString ref_path_chrstr = ref_path.utf8();
	const char *ref_path_c = ref_path_chrstr.get_data();
	const char *ref_path_c_array[] = { ref_path_c };

	const char **originalProjectFiles_c_array = new const char *[originalProjectFiles.size()];
	Vector<CharString> originalProjectFiles_chrstrs;
	for (int i = 0; i < originalProjectFiles.size(); i++) {
		CharString originalProjectFiles_chrstr = originalProjectFiles[i].utf8();
		// to keep them from being freed
		originalProjectFiles_chrstrs.push_back(originalProjectFiles_chrstr);
		originalProjectFiles_c_array[i] = originalProjectFiles_chrstr.get_data();
	}

	decompilerHandle = GodotMonoDecomp_CreateGodotModuleDecompiler(assembly_path_c, originalProjectFiles_c_array, originalProjectFiles.size(), ref_path_c_array, 1);
	delete[] originalProjectFiles_c_array;
}

GodotMonoDecompWrapper::~GodotMonoDecompWrapper() {
	GodotMonoDecomp_FreeObjectHandle(decompilerHandle);
}

Error GodotMonoDecompWrapper::decompile_module(const String &outputCSProjectPath) {
	CharString outputCSProjectPath_chrstr = outputCSProjectPath.utf8();
	const char *outputCSProjectPath_c = outputCSProjectPath_chrstr.get_data();
	if (GodotMonoDecomp_DecompileModule(decompilerHandle, outputCSProjectPath_c) != 0) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

String GodotMonoDecompWrapper::decompile_individual_file(const String &file) {
	CharString file_chrstr = file.utf8();
	const char *file_c = file_chrstr.get_data();
	const char *result = GodotMonoDecomp_DecompileIndividualFile(decompilerHandle, file_c);
	ERR_FAIL_COND_V_MSG(result == nullptr, "", "Failed to decompile individual file");
	String result_str = String::utf8(result);
	GodotMonoDecomp_FreeString((void *)result);
	return result_str;
}

Vector<String> GodotMonoDecompWrapper::get_files_not_present_in_file_map() {
	int num = GodotMonoDecomp_GetNumberOfFilesNotPresentInFileMap(decompilerHandle);
	if (num == 0) {
		return Vector<String>();
	}
	ERR_FAIL_COND_V_MSG(num < 0, Vector<String>(), "Failed to get number of files not present in file map");
	const char **files_not_present_in_file_map_c_array = GodotMonoDecomp_GetFilesNotPresentInFileMap(decompilerHandle);
	Vector<String> files_not_present_in_file_map_strs;
	for (int i = 0; i < num; i++) {
		files_not_present_in_file_map_strs.push_back(String::utf8(files_not_present_in_file_map_c_array[i]));
	}
	GodotMonoDecomp_FreeArray((void *)files_not_present_in_file_map_c_array, num);
	return files_not_present_in_file_map_strs;
}