#include "tests/test_macros.h"

#include "test_common.h"
#include "test_config_helpers.inc"

#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"
#include "utility/gdre_config.h"
#include "utility/task_manager.h"

TEST_FORCE_LINK(test_gdre_config)

// Every test case in this file is tagged with [ConfigTest]; the GDRETestListener resets GDREConfig to its defaults
// (and redirects the config file to a temp path) before and after each one.

namespace TestGdreConfig {

using TestConfigHelpers::ScopedSetting;

struct CoreSettingExpectation {
	const char *key;
	Variant::Type type;
	Variant default_value;
	bool hidden = false;
	bool dirpicker = false;
	bool range = false;
	bool special_values = false;
	// The value used for the round-trip test, must be different from the default
	Variant test_value;
};

static Vector<CoreSettingExpectation> get_core_setting_expectations() {
	Vector<CoreSettingExpectation> ret;
	// {key, type, default, hidden, dirpicker, range, special_values, test_value}
	ret.push_back({ CoreSettings::LAST_GDRE_VERSION_USED, Variant::STRING, "", true, false, false, false, "1.2.3-test" });
	ret.push_back({ CoreSettings::MAX_CORES_TO_USE, Variant::INT, -1, false, false, false, true, 3 });
	ret.push_back({ CoreSettings::FORCE_SINGLE_THREADED, Variant::BOOL, false, false, false, false, false, true });
	ret.push_back({ CoreSettings::ASK_FOR_DOWNLOAD, Variant::BOOL, true, true, false, false, false, false });
	ret.push_back({ CoreSettings::LAST_SHOWED_DISCLAIMER, Variant::STRING, "<NONE>", true, false, false, false, "4.5.0" });
	ret.push_back({ CoreSettings::PREVIEW_USE_SCENE_VIEW_BY_DEFAULT, Variant::BOOL, false, false, false, false, false, true });
	ret.push_back({ CoreSettings::RECOVERY_LOAD_EMBEDDED_ZIPS, Variant::BOOL, true, false, false, false, false, false });
	ret.push_back({ CoreSettings::RECOVERY_DEFAULT_PARENT_FOLDER_FOR_RECOVERY, Variant::STRING, OS::get_singleton()->get_system_dir(OS::SYSTEM_DIR_DESKTOP), false, true, false, false, "/some/test/dir" });
	ret.push_back({ CoreSettings::RECOVERY_DELETE_AUTO_CONVERTED_FILES, Variant::BOOL, false, false, false, false, false, true });
	ret.push_back({ CoreSettings::RECOVERY_DOWNLOAD_PLUGINS, Variant::BOOL, false, false, false, false, false, true });
	ret.push_back({ CoreSettings::RECOVERY_EXECUTE_VISUAL_SHADER_NODE_SCRIPTS, Variant::BOOL, false, false, false, false, false, true });
	ret.push_back({ CoreSettings::RECOVERY_CLEAR_OUTPUT_DIR_EXCEPT_GIT_BEFORE_FULL_RECOVERY, Variant::BOOL, false, true, false, false, false, true });
	ret.push_back({ CoreSettings::RECOVERY_WRITE_JSON_REPORT, Variant::BOOL, false, true, false, false, false, true });
	ret.push_back({ CoreSettings::RECOVERY_GIT_CREATE_GIT_REPO, Variant::BOOL, false, false, false, false, false, true });
	ret.push_back({ CoreSettings::RECOVERY_GIT_ADD_IMPORTS_TO_GIT_REPO, Variant::BOOL, false, false, false, false, false, true });
	ret.push_back({ CoreSettings::SCRIPT_INDENT_TYPE, Variant::INT, 0, false, false, false, true, 1 });
	ret.push_back({ CoreSettings::SCRIPT_INDENT_SIZE, Variant::INT, 4, false, false, true, false, 8 });
	return ret;
}

static Ref<GDREConfigSetting> find_setting(const String &p_key, int *r_count = nullptr) {
	Ref<GDREConfigSetting> found;
	int count = 0;
	TypedArray<GDREConfigSetting> all = GDREConfig::get_singleton()->get_all_settings();
	for (int i = 0; i < all.size(); i++) {
		Ref<GDREConfigSetting> setting = all[i];
		if (setting.is_valid() && setting->get_full_name() == p_key) {
			found = setting;
			count++;
		}
	}
	if (r_count) {
		*r_count = count;
	}
	return found;
}

TEST_CASE("[GDSDecomp][ConfigTest] CoreSettings are registered exactly once") {
	for (const auto &expected : get_core_setting_expectations()) {
		int count = 0;
		Ref<GDREConfigSetting> setting = find_setting(expected.key, &count);
		INFO(expected.key);
		CHECK_EQ(count, 1);
		CHECK(setting.is_valid());
	}
}

TEST_CASE("[GDSDecomp][ConfigTest] CoreSettings have the expected defaults, types and flags") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	for (const auto &expected : get_core_setting_expectations()) {
		INFO(expected.key);
		Ref<GDREConfigSetting> setting = find_setting(expected.key);
		REQUIRE(setting.is_valid());

		CHECK_EQ(setting->get_type(), expected.type);
		CHECK_EQ(setting->get_default_value(), expected.default_value);
		CHECK_EQ(cfg->get_default_value(expected.key), expected.default_value);
		CHECK_EQ(setting->get_value(), expected.default_value);
		// the registered default must win over the caller-provided fallback once the config is initialized
		CHECK_EQ(cfg->get_setting(expected.key, "<<fallback that should never be returned>>"), expected.default_value);
		CHECK(cfg->has_setting(expected.key));

		CHECK_EQ(setting->is_hidden(), expected.hidden);
		CHECK_EQ(setting->is_dirpicker(), expected.dirpicker);
		CHECK_EQ(setting->is_range_setting(), expected.range);
		CHECK_EQ(setting->has_special_value(), expected.special_values);
		CHECK_FALSE(setting->is_filepicker());
		CHECK_FALSE(setting->is_ephemeral());
		CHECK_FALSE(setting->is_virtual_setting());
		CHECK_FALSE(cfg->is_ephemeral_setting(expected.key));
	}
}

TEST_CASE("[GDSDecomp][ConfigTest] CoreSettings special values and ranges") {
	SUBCASE("Indent size is a 1-64 range with step 1") {
		Ref<GDREConfigSetting> setting = find_setting(CoreSettings::SCRIPT_INDENT_SIZE);
		REQUIRE(setting.is_valid());
		CHECK_EQ(setting->get_min_value(), Variant(1));
		CHECK_EQ(setting->get_max_value(), Variant(64));
		CHECK_EQ(setting->get_step_value(), Variant(1));
	}
	SUBCASE("Indent type is Tabs/Spaces") {
		Ref<GDREConfigSetting> setting = find_setting(CoreSettings::SCRIPT_INDENT_TYPE);
		REQUIRE(setting.is_valid());
		Dictionary values = setting->get_list_of_possible_values();
		CHECK_EQ(values.size(), 2);
		CHECK_EQ(values[Variant(0)], Variant("Tabs"));
		CHECK_EQ(values[Variant(1)], Variant("Spaces"));
	}
	SUBCASE("Max cores has an auto-detect entry and one entry per thread") {
		Ref<GDREConfigSetting> setting = find_setting(CoreSettings::MAX_CORES_TO_USE);
		REQUIRE(setting.is_valid());
		Dictionary values = setting->get_list_of_possible_values();
		CHECK(values.has(-1));
		CHECK_EQ(values[Variant(-1)], Variant("Auto-detect"));
		CHECK(values.has(1));
		CHECK_GE(values.size(), 2);
	}
}

TEST_CASE("[GDSDecomp][ConfigTest] CoreSettings keys are well-formed") {
	for (const auto &expected : get_core_setting_expectations()) {
		INFO(expected.key);
		String key = expected.key;
		CHECK(key.contains_char('/'));
		CHECK_EQ(GDREConfig::get_section_from_key(key), key.get_slice("/", 0));
		CHECK_EQ(GDREConfig::get_name_from_key(key), key.substr(key.find("/") + 1));
	}
	// keys with no section are placed in General
	CHECK_EQ(GDREConfig::get_section_from_key("force_single_threaded"), String("General"));
	CHECK_EQ(GDREConfig::get_name_from_key("force_single_threaded"), String("force_single_threaded"));
}

TEST_CASE("[GDSDecomp][ConfigTest] CoreSettings round-trip through set_setting/get_setting") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	for (const auto &expected : get_core_setting_expectations()) {
		INFO(expected.key);
		REQUIRE_NE(expected.test_value, expected.default_value);
		Ref<GDREConfigSetting> setting = find_setting(expected.key);
		REQUIRE(setting.is_valid());

		cfg->set_setting(expected.key, expected.test_value);
		CHECK_EQ(cfg->get_setting(expected.key), expected.test_value);
		CHECK_EQ(setting->get_value(), expected.test_value);
		CHECK_EQ(cfg->get_setting(expected.key).get_type(), expected.type);

		// the default is not altered by changing the value
		CHECK_EQ(cfg->get_default_value(expected.key), expected.default_value);

		setting->reset();
		CHECK_EQ(cfg->get_setting(expected.key), expected.default_value);

		setting->set_value(expected.test_value);
		CHECK_EQ(cfg->get_setting(expected.key), expected.test_value);
		cfg->reset_all_settings(true);
		CHECK_EQ(cfg->get_setting(expected.key), expected.default_value);
	}
}

TEST_CASE("[GDSDecomp][ConfigTest] set_setting converts values to the registered type") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	cfg->set_setting(CoreSettings::SCRIPT_INDENT_SIZE, "8");
	Variant size = cfg->get_setting(CoreSettings::SCRIPT_INDENT_SIZE);
	CHECK_EQ(size.get_type(), Variant::INT);
	CHECK_EQ(int(size), 8);

	cfg->set_setting(CoreSettings::FORCE_SINGLE_THREADED, 1);
	Variant single_threaded = cfg->get_setting(CoreSettings::FORCE_SINGLE_THREADED);
	CHECK_EQ(single_threaded.get_type(), Variant::BOOL);
	CHECK_EQ(bool(single_threaded), true);
}

TEST_CASE("[GDSDecomp][ConfigTest] Unqualified keys resolve to the General section") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	CHECK_FALSE(bool(cfg->get_setting(CoreSettings::FORCE_SINGLE_THREADED)));
	cfg->set_setting("force_single_threaded", true);
	CHECK(cfg->has_setting("force_single_threaded"));
	CHECK(cfg->has_setting(CoreSettings::FORCE_SINGLE_THREADED));
	CHECK(bool(cfg->get_setting(CoreSettings::FORCE_SINGLE_THREADED)));
	CHECK(bool(cfg->get_setting("General/force_single_threaded")));
	CHECK_EQ(cfg->get_default_value("max_cores_to_use"), cfg->get_default_value(CoreSettings::MAX_CORES_TO_USE));
}

TEST_CASE("[GDSDecomp][ConfigTest] Ephemeral settings") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	cfg->set_setting(CoreSettings::MAX_CORES_TO_USE, 2);
	CHECK_FALSE(cfg->is_ephemeral_setting(CoreSettings::MAX_CORES_TO_USE));

	cfg->set_setting(CoreSettings::FORCE_SINGLE_THREADED, true, true);
	CHECK(cfg->is_ephemeral_setting(CoreSettings::FORCE_SINGLE_THREADED));
	CHECK(bool(cfg->get_setting(CoreSettings::FORCE_SINGLE_THREADED)));
	// once ephemeral, further changes stay ephemeral
	cfg->set_setting(CoreSettings::FORCE_SINGLE_THREADED, false);
	CHECK(cfg->is_ephemeral_setting(CoreSettings::FORCE_SINGLE_THREADED));
	CHECK_FALSE(bool(cfg->get_setting(CoreSettings::FORCE_SINGLE_THREADED)));
	cfg->set_setting(CoreSettings::FORCE_SINGLE_THREADED, true);

	cfg->reset_ephemeral_settings();
	CHECK_FALSE(cfg->is_ephemeral_setting(CoreSettings::FORCE_SINGLE_THREADED));
	// falls back to the persisted value, which is still the default
	CHECK_FALSE(bool(cfg->get_setting(CoreSettings::FORCE_SINGLE_THREADED)));
	// persisted settings are not affected
	CHECK_EQ(int(cfg->get_setting(CoreSettings::MAX_CORES_TO_USE)), 2);
}

TEST_CASE("[GDSDecomp][ConfigTest] reset_all_settings restores every setting") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	const auto expectations = get_core_setting_expectations();
	for (const auto &expected : expectations) {
		cfg->set_setting(expected.key, expected.test_value);
	}
	cfg->set_setting(CoreSettings::FORCE_SINGLE_THREADED, true, true);
	CHECK(cfg->is_ephemeral_setting(CoreSettings::FORCE_SINGLE_THREADED));

	cfg->reset_all_settings(true);

	for (const auto &expected : expectations) {
		INFO(expected.key);
		CHECK_EQ(cfg->get_setting(expected.key), expected.default_value);
		CHECK_FALSE(cfg->is_ephemeral_setting(expected.key));
	}
}

TEST_CASE("[GDSDecomp][ConfigTest] ScopedSetting restores the previous value") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	cfg->set_setting(CoreSettings::SCRIPT_INDENT_SIZE, 2);
	{
		ScopedSetting scoped(CoreSettings::SCRIPT_INDENT_SIZE, 16);
		CHECK_EQ(int(cfg->get_setting(CoreSettings::SCRIPT_INDENT_SIZE)), 16);
	}
	CHECK_EQ(int(cfg->get_setting(CoreSettings::SCRIPT_INDENT_SIZE)), 2);
}

TEST_CASE("[GDSDecomp][ConfigTest] get_section_settings returns the settings of one section") {
	Dictionary recovery = GDREConfig::get_singleton()->get_section_settings("Recovery");
	CHECK(recovery.has("load_embedded_zips"));
	CHECK(recovery.has("default_parent_folder_for_recovery"));
	CHECK(recovery.has("delete_auto_converted_files"));
	CHECK(recovery.has("download_plugins"));
	CHECK(recovery.has("execute_visual_shader_node_scripts"));
	CHECK(recovery.has("clear_output_dir_except_git_before_full_recovery"));
	CHECK(recovery.has("write_json_report"));
	CHECK(recovery.has("git/create_git_repo"));
	CHECK(recovery.has("git/add_imports_to_git_repo"));
	CHECK_FALSE(recovery.has("max_cores_to_use"));
	CHECK_FALSE(recovery.has("size"));

	Dictionary indent = GDREConfig::get_singleton()->get_section_settings("Script/Indent");
	CHECK(indent.has("type"));
	CHECK(indent.has("size"));
	CHECK_FALSE(indent.has("load_embedded_zips"));
}

TEST_CASE("[GDSDecomp][ConfigTest] Legacy unqualified keys in the config file map to the new keys") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	const String cfg_path = get_config_test_path();

	{
		Ref<ConfigFile> file;
		file.instantiate();
		file->set_value("General", "load_embedded_zips", false);
		file->set_value("General", "default_parent_folder_for_recovery", "/legacy/dir");
		file->set_value("General", "delete_auto_converted_files", true);
		file->set_value("General", "download_plugins", true);
		file->set_value("General", "execute_visual_shader_node_scripts", true);
		file->set_value("General", "write_json_report", true);
		file->set_value("General", "force_single_threaded", true);
		REQUIRE_EQ(file->save(cfg_path), OK);
	}

	cfg->load_config();

	CHECK_FALSE(bool(cfg->get_setting(CoreSettings::RECOVERY_LOAD_EMBEDDED_ZIPS)));
	CHECK_EQ(cfg->get_setting(CoreSettings::RECOVERY_DEFAULT_PARENT_FOLDER_FOR_RECOVERY), Variant("/legacy/dir"));
	CHECK(bool(cfg->get_setting(CoreSettings::RECOVERY_DELETE_AUTO_CONVERTED_FILES)));
	CHECK(bool(cfg->get_setting(CoreSettings::RECOVERY_DOWNLOAD_PLUGINS)));
	CHECK(bool(cfg->get_setting(CoreSettings::RECOVERY_EXECUTE_VISUAL_SHADER_NODE_SCRIPTS)));
	CHECK(bool(cfg->get_setting(CoreSettings::RECOVERY_WRITE_JSON_REPORT)));
	// not an alias, this is just a normal General setting
	CHECK(bool(cfg->get_setting(CoreSettings::FORCE_SINGLE_THREADED)));
	// untouched settings stay at their defaults
	CHECK_FALSE(bool(cfg->get_setting(CoreSettings::RECOVERY_GIT_CREATE_GIT_REPO)));
}

TEST_CASE("[GDSDecomp][ConfigTest] Qualified keys in the config file are loaded") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	{
		Ref<ConfigFile> file;
		file.instantiate();
		file->set_value("Recovery", "load_embedded_zips", false);
		file->set_value("Recovery/git", "create_git_repo", true);
		file->set_value("Script/Indent", "type", 1);
		file->set_value("Script/Indent", "size", 2);
		file->set_value("General", "max_cores_to_use", 2);
		REQUIRE_EQ(file->save(get_config_test_path()), OK);
	}

	cfg->load_config();

	CHECK_FALSE(bool(cfg->get_setting(CoreSettings::RECOVERY_LOAD_EMBEDDED_ZIPS)));
	CHECK(bool(cfg->get_setting(CoreSettings::RECOVERY_GIT_CREATE_GIT_REPO)));
	CHECK_EQ(int(cfg->get_setting(CoreSettings::SCRIPT_INDENT_TYPE)), 1);
	CHECK_EQ(int(cfg->get_setting(CoreSettings::SCRIPT_INDENT_SIZE)), 2);
	CHECK_EQ(int(cfg->get_setting(CoreSettings::MAX_CORES_TO_USE)), 2);
}

TEST_CASE("[GDSDecomp][ConfigTest] save_config only writes non-default values") {
	GDREConfig *cfg = GDREConfig::get_singleton();
	const String cfg_path = get_config_test_path();

	cfg->set_setting(CoreSettings::MAX_CORES_TO_USE, 3);
	cfg->set_setting(CoreSettings::RECOVERY_WRITE_JSON_REPORT, true);
	cfg->set_setting(CoreSettings::SCRIPT_INDENT_TYPE, 1);
	cfg->save_config();
	REQUIRE(FileAccess::exists(cfg_path));

	{
		Ref<ConfigFile> file;
		file.instantiate();
		REQUIRE_EQ(file->load(cfg_path), OK);
		CHECK_EQ(file->get_value("General", "max_cores_to_use"), Variant(3));
		CHECK_EQ(file->get_value("Recovery", "write_json_report"), Variant(true));
		CHECK_EQ(file->get_value("Script", "Indent/type"), Variant(1));
		// always written
		CHECK(file->has_section_key("General", "last_gdre_version_used"));
		// defaults are not written
		CHECK_FALSE(file->has_section_key("General", "force_single_threaded"));
		CHECK_FALSE(file->has_section_key("Recovery", "load_embedded_zips"));
		CHECK_FALSE(file->has_section_key("Script", "Indent/size"));
	}

	// and they come back after a reload
	cfg->reset_all_settings(true);
	CHECK_EQ(int(cfg->get_setting(CoreSettings::MAX_CORES_TO_USE)), -1);
	cfg->load_config();
	CHECK_EQ(int(cfg->get_setting(CoreSettings::MAX_CORES_TO_USE)), 3);
	CHECK(bool(cfg->get_setting(CoreSettings::RECOVERY_WRITE_JSON_REPORT)));
	CHECK_EQ(int(cfg->get_setting(CoreSettings::SCRIPT_INDENT_TYPE)), 1);
	CHECK_EQ(int(cfg->get_setting(CoreSettings::SCRIPT_INDENT_SIZE)), 4);
}

// Records the ids of the threads that did the work.
class ThreadRecorder {
public:
	Mutex mutex;
	HashSet<Thread::ID> thread_ids;

	void do_work(uint32_t p_index, void *p_userdata) {
		{
			MutexLock lock(mutex);
			thread_ids.insert(Thread::get_caller_id());
		}
		// give the other workers a chance to pick up some work
		OS::get_singleton()->delay_usec(2000);
	}

	String describe_work(uint32_t p_index, void *p_userdata) {
		return "ConfigTest work item " + itos(p_index);
	}

	Error run(int p_elements = 32) {
		return TaskManager::get_singleton()->run_multithreaded_group_task(
				this,
				&ThreadRecorder::do_work,
				(void *)nullptr,
				p_elements,
				&ThreadRecorder::describe_work,
				"ConfigTest::ThreadRecorder",
				"Recording threads...",
				false, -1, true, nullptr, 0, false);
	}
};

TEST_CASE("[GDSDecomp][ConfigTest] force_single_threaded runs group tasks on the calling thread") {
	REQUIRE(TaskManager::get_singleton());
	const Thread::ID main_id = Thread::get_caller_id();

	SUBCASE("Enabled") {
		ScopedSetting scoped(CoreSettings::FORCE_SINGLE_THREADED, true);
		ThreadRecorder recorder;
		CHECK_EQ(recorder.run(), OK);
		CHECK_EQ(int(recorder.thread_ids.size()), 1);
		CHECK(recorder.thread_ids.has(main_id));
	}
	SUBCASE("Disabled") {
		ScopedSetting scoped(CoreSettings::FORCE_SINGLE_THREADED, false);
		ThreadRecorder recorder;
		CHECK_EQ(recorder.run(), OK);
		CHECK_GE(int(recorder.thread_ids.size()), 1);
		if (TaskManager::get_max_thread_count() > 1) {
			// Work is done by the thread pool, not by us
			CHECK_FALSE(recorder.thread_ids.has(main_id));
		}
	}
}

TEST_CASE("[GDSDecomp][ConfigTest] max_cores_to_use limits the number of worker threads") {
	REQUIRE(TaskManager::get_singleton());
	const int max_threads = TaskManager::get_max_thread_count();

	SUBCASE("1 core") {
		ScopedSetting scoped(CoreSettings::MAX_CORES_TO_USE, 1);
		ThreadRecorder recorder;
		CHECK_EQ(recorder.run(), OK);
		CHECK_EQ(int(recorder.thread_ids.size()), 1);
	}
	SUBCASE("2 cores") {
		ScopedSetting scoped(CoreSettings::MAX_CORES_TO_USE, 2);
		ThreadRecorder recorder;
		CHECK_EQ(recorder.run(), OK);
		CHECK_GE(int(recorder.thread_ids.size()), 1);
		CHECK_LE(int(recorder.thread_ids.size()), 2);
	}
	SUBCASE("Auto-detect") {
		ScopedSetting scoped(CoreSettings::MAX_CORES_TO_USE, -1);
		ThreadRecorder recorder;
		CHECK_EQ(recorder.run(), OK);
		CHECK_GE(int(recorder.thread_ids.size()), 1);
		CHECK_LE(int(recorder.thread_ids.size()), MAX(1, max_threads));
	}
}

} //namespace TestGdreConfig
