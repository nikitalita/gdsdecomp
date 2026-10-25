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
	static Array get_default_settings();

	static Dictionary _get_settings_from_config();
public:
#if !GODOT_MONO_DECOMP_DISABLED
	static constexpr bool godot_mono_decomp_enabled = true;
#else
	static constexpr bool godot_mono_decomp_enabled = false;
#endif

	static constexpr bool is_godot_mono_decomp_enabled() {
		return godot_mono_decomp_enabled;
	}

	static Ref<GodotMonoDecompWrapper> create(const String &assemblyPath, const Vector<String> &originalProjectFiles);

	bool is_handle_valid() const;

	Error decompile_module(const String &outputCSProjectPath, const Vector<String> &excludeFiles);
	String decompile_individual_file(const String &file);
	Dictionary get_script_info(const String &file);
	Vector<String> get_files_not_present_in_file_map();
	Vector<String> get_files_in_file_map();
	bool is_custom_version_detected() const;
	Vector<String> get_all_strings_in_module();
	Vector<String> get_global_class_files();

	Dictionary get_settings() const;
	bool reload_settings();

	static bool is_file_assembly(const String &file);
	static void add_default_config_settings();
	~GodotMonoDecompWrapper();

private:
	Error _load(const String &assemblyPath, const Vector<String> &originalProjectFiles, const Dictionary &settings);

	String assembly_path;
	void *decompilerHandle = nullptr;
	Vector<String> originalProjectFiles;
};
