#include "godot_mono_decomp_wrapper.h"
#include "core/templates/vector.h"
#include "godot_mono_decomp.h"

GodotMonoDecompWrapper::GodotMonoDecompWrapper() {
	decompilerHandle = nullptr;
}

GodotMonoDecompWrapper::~GodotMonoDecompWrapper() {
	if (decompilerHandle != nullptr) {
		GodotMonoDecomp_FreeObjectHandle(decompilerHandle);
	}
}

Ref<GodotMonoDecompWrapper> GodotMonoDecompWrapper::create(const String &assembly_path, const Vector<String> &originalProjectFiles, const Vector<String> &assemblyReferenceDirs, const GodotMonoDecompSettings &settings) {
	CharString assembly_path_chrstr = assembly_path.utf8();
	const char *assembly_path_c = assembly_path_chrstr.get_data();
	String ref_path = assembly_path.get_base_dir();
	CharString ref_path_chrstr = ref_path.utf8();
	const char *ref_path_c = ref_path_chrstr.get_data();
	const char *ref_path_c_array[] = { ref_path_c };

	CharString godotVersionOverride_chrstr = settings.GodotVersionOverride.is_empty() ? "" : settings.GodotVersionOverride.utf8();
	const char *godotVersionOverride_c = settings.GodotVersionOverride.is_empty() ? nullptr : godotVersionOverride_chrstr.get_data();

	const char **originalProjectFiles_c_array = new const char *[originalProjectFiles.size()];
	Vector<CharString> originalProjectFiles_chrstrs;
	originalProjectFiles_chrstrs.resize(originalProjectFiles.size());
	for (int i = 0; i < originalProjectFiles.size(); i++) {
		// to keep them from being freed
		originalProjectFiles_chrstrs.write[i] = originalProjectFiles[i].utf8();
		originalProjectFiles_c_array[i] = originalProjectFiles_chrstrs[i].get_data();
	}

	auto decompilerHandle = GodotMonoDecomp_CreateGodotModuleDecompiler(assembly_path_c, originalProjectFiles_c_array, originalProjectFiles.size(), ref_path_c_array, 1, godotVersionOverride_c);
	delete[] originalProjectFiles_c_array;
	if (decompilerHandle == nullptr) {
		return Ref<GodotMonoDecompWrapper>();
	}
	auto wrapper = memnew(GodotMonoDecompWrapper);
	wrapper->decompilerHandle = decompilerHandle;
	wrapper->assembly_path = assembly_path;
	return Ref<GodotMonoDecompWrapper>(wrapper);
}
#include "gd_parallel_queue.h"
#include "task_manager.h"

struct DecompileModuleTaskData : public TaskRunnerStruct {
	String outputCSProjectPath;
	Vector<String> excludeFiles;
	Error err = OK;
	std::atomic<int> current_step = 0;
	int total_steps = 0;
	ParalellQueue<String> queue;
	String current_step_description = "Decompiling module...";
	bool cancelled = false;

	DecompileModuleTaskData(const String &p_outputCSProjectPath, const Vector<String> &p_excludeFiles, int p_total_steps) :
			outputCSProjectPath(p_outputCSProjectPath), excludeFiles(p_excludeFiles), total_steps(p_total_steps), queue(p_total_steps) {}
	bool done = false;

	int get_current_task_step_value() override {
		return current_step;
	}

	String get_current_task_step_description() override {
		// pop them all off until we get the current one
		while (queue.try_pop(current_step_description))
			;
		return current_step_description;
	}
	bool is_done() const override {
		return done;
	}

	void cancel() override {
		cancelled = true;
	}

	int progress_callback(int p_current, int p_total, const char *p_description) {
		if (cancelled) {
			return 1;
		}
		current_step++;
		// compare and exchange if p_current is greater than current_step
		// current_step = p_current;
		// total_steps = p_total;
		String description = String::utf8(p_description);
		queue.push(description);
		return 0;
	}

	static int _progress_callback(void *p_userdata, int p_current, int p_total, const char *p_description) {
		DecompileModuleTaskData *taskData = (DecompileModuleTaskData *)p_userdata;
		return taskData->progress_callback(p_current, p_total, p_description);
	}

	void run(void *p_userdata) override {
		GodotMonoDecompWrapper *wrapper = (GodotMonoDecompWrapper *)p_userdata;
		if (wrapper->decompilerHandle == nullptr) {
			ERR_PRINT("Decompiler handle is null");
			err = ERR_CANT_CREATE;
			done = true;
			return;
		}
		CharString outputCSProjectPath_chrstr = outputCSProjectPath.utf8();
		const char *outputCSProjectPath_c = outputCSProjectPath_chrstr.get_data();
		Vector<CharString> excludeFiles_chrstrrs;
		excludeFiles_chrstrrs.resize(excludeFiles.size());
		const char **excludeFiles_c_array = new const char *[excludeFiles.size()];
		for (int i = 0; i < excludeFiles.size(); i++) {
			excludeFiles_chrstrrs.write[i] = excludeFiles[i].utf8();
			excludeFiles_c_array[i] = excludeFiles_chrstrrs[i].get_data();
		}

		int result = GodotMonoDecomp_DecompileModuleWithProgress(wrapper->decompilerHandle, outputCSProjectPath_c, excludeFiles_c_array, excludeFiles.size(), &DecompileModuleTaskData::_progress_callback, this);
		delete[] excludeFiles_c_array;
		done = true;
		if (result != 0 && !cancelled) {
			err = ERR_CANT_CREATE;
		}
	}
};

Error GodotMonoDecompWrapper::decompile_module_with_progress(const String &outputCSProjectPath, const Vector<String> &excludeFiles) {
	int total_steps = GodotMonoDecomp_GetNumberOfFilesInFileMap(decompilerHandle);
	DecompileModuleTaskData taskData(outputCSProjectPath, excludeFiles, total_steps);
	TaskManager::get_singleton()->run_task(&taskData, this, "Decompiling C# scripts...", total_steps, true, true);
	return taskData.err;
}

Error GodotMonoDecompWrapper::decompile_module(const String &outputCSProjectPath, const Vector<String> &excludeFiles) {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, ERR_CANT_CREATE, "Decompiler handle is null");
	CharString outputCSProjectPath_chrstr = outputCSProjectPath.utf8();
	const char *outputCSProjectPath_c = outputCSProjectPath_chrstr.get_data();
	Vector<CharString> excludeFiles_chrstrrs;
	excludeFiles_chrstrrs.resize(excludeFiles.size());
	const char **excludeFiles_c_array = new const char *[excludeFiles.size()];
	for (int i = 0; i < excludeFiles.size(); i++) {
		excludeFiles_chrstrrs.write[i] = excludeFiles[i].utf8();
		excludeFiles_c_array[i] = excludeFiles_chrstrrs[i].get_data();
	}
	int ret = GodotMonoDecomp_DecompileModule(decompilerHandle, outputCSProjectPath_c, excludeFiles_c_array, excludeFiles.size());
	delete[] excludeFiles_c_array;
	if (ret != 0) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

String GodotMonoDecompWrapper::decompile_individual_file(const String &file) {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, "", "Decompiler handle is null");
	CharString file_chrstr = file.utf8();
	const char *file_c = file_chrstr.get_data();
	const char *result = GodotMonoDecomp_DecompileIndividualFile(decompilerHandle, file_c);
	ERR_FAIL_COND_V_MSG(result == nullptr, "", "Failed to decompile individual file");
	String result_str = String::utf8(result);
	GodotMonoDecomp_FreeString((void *)result);
	return result_str;
}

Vector<String> GodotMonoDecompWrapper::get_files_not_present_in_file_map() {
	ERR_FAIL_COND_V_MSG(decompilerHandle == nullptr, Vector<String>(), "Decompiler handle is null");
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

void GodotMonoDecompWrapper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("decompile_module", "outputCSProjectPath"), &GodotMonoDecompWrapper::decompile_module_with_progress);
	ClassDB::bind_method(D_METHOD("decompile_individual_file", "file"), &GodotMonoDecompWrapper::decompile_individual_file);
	ClassDB::bind_method(D_METHOD("get_files_not_present_in_file_map"), &GodotMonoDecompWrapper::get_files_not_present_in_file_map);
}
