#include "tests/test_macros.h"

#include "test_common.h"
#include "test_config_helpers.inc"

#include "core/io/json.h"
#include "core/os/os.h"

#include "exporters/export_report.h"
#include "utility/common.h"
#include "utility/gdre_config.h"
#include "utility/gdre_settings.h"
#include "utility/import_exporter.h"
#include "utility/pck_dumper.h"
#include <compat/resource_loader_compat.h>

TEST_FORCE_LINK(test_project_export)

class ExportReport;
class ImportInfo;
namespace TestProjectExport {

Error test_json_import_info(const Ref<ImportInfo> &import_info) {
	Error err = OK;
	Dictionary json = import_info->to_json();
	Ref<ImportInfo> import_info2 = ImportInfo::from_json(json);
	CHECK(import_info->is_equal_to(import_info2));
	return err;
}

Error test_json_export_report(const Ref<ExportReport> &export_report) {
	Error err = OK;
	Dictionary json = export_report->to_json();
	Ref<ExportReport> export_report2 = ExportReport::from_json(json);
	CHECK(export_report->is_equal_to(export_report2));
	return err;
}

String get_test_projects_path() {
	return get_gdsdecomp_path().path_join("tests/test_projects");
}

Error test_export_project(const String &version, const String &sub_project, const String &original_parent_dir, const String &exported_parent_dir) {
	String exported_pck_path = exported_parent_dir.path_join(version).path_join(sub_project);

	String subdir = version.path_join(sub_project.get_base_dir());
	String original_project_zip = original_parent_dir.path_join(subdir).path_join(sub_project.get_file().get_basename() + ".zip");
	// where we will extract the original project
	String original_extract_dir = get_tmp_path().path_join("original").path_join(subdir);
	// where we will output the exported project during recovery
	String exported_recovery_dir = get_tmp_path().path_join("exported").path_join(subdir);

	gdre::rimraf(original_extract_dir);
	Error err = gdre::unzip_file_to_dir(original_project_zip, original_extract_dir);
	CHECK_EQ(err, OK);

	gdre::rimraf(exported_recovery_dir);
	// load the project
	err = GDRESettings::get_singleton()->load_project({ exported_pck_path }, false);
	CHECK_EQ(err, OK);
	// check that we detected the correct engine version
	auto ourVersion = GodotVer::parse(version);
	REQUIRE(ourVersion.is_valid());
	CHECK(ourVersion->is_valid_semver());
	auto loadedVersion = GodotVer::parse(GDRESettings::get_singleton()->get_version_string());
	REQUIRE(loadedVersion.is_valid());
	CHECK(loadedVersion->is_valid_semver());
	// Godot 3.1 and below did not write the correct patch version to the PCK
	if (ourVersion->get_prerelease().is_empty() && ((ourVersion->get_major() == 3 && ourVersion->get_minor() <= 1) || ourVersion->get_major() < 3)) {
		CHECK_EQ(ourVersion->get_major(), loadedVersion->get_major());
		CHECK_EQ(ourVersion->get_minor(), loadedVersion->get_minor());
	} else {
		CHECK_EQ(*ourVersion.ptr(), loadedVersion);
	}

	PckDumper dumper;
	err = dumper.check_md5_all_files();
	CHECK_EQ(err, OK);
	err = dumper.pck_dump_to_dir(exported_recovery_dir, {});
	CHECK_EQ(err, OK);
	ImportExporter import_exporter;
	err = import_exporter.export_imports(exported_recovery_dir, {});
	CHECK_EQ(err, OK);
	auto import_report = import_exporter.get_report();
	REQUIRE(import_report.is_valid());
	Dictionary json_report = import_report->to_json();
	{
		Ref<ImportExporterReport> import_exporter_report2 = ImportExporterReport::from_json(json_report);
		CHECK(import_report->is_equal_to(import_exporter_report2));
	}
#ifdef DEBUG_ENABLED
	{
		String json_report_path = exported_recovery_dir.path_join("json_report.json");
		auto fa = FileAccess::open(json_report_path, FileAccess::WRITE);
		REQUIRE(fa.is_valid());
		fa->store_string(JSON::stringify(json_report, "\t", false, true));
		fa->flush();
	}
#endif
	CHECK_EQ(import_report->get_failed().size(), 0);
	auto successes = import_report->get_successes();
	for (const auto &success : successes) {
		Ref<ExportReport> export_report = success;
		REQUIRE(export_report.is_valid());
		Ref<ImportInfo> import_info = export_report->get_import_info();
		REQUIRE(import_info.is_valid());
		test_json_import_info(import_info);
		test_json_export_report(export_report);
	}
	err = import_exporter.test_exported_project(original_extract_dir);
	CHECK_EQ(err, OK);

	GDRESettings::get_singleton()->close_log_file();
	GDRESettings::get_singleton()->unload_project();
	return err;
}

// '[SceneTree]' is in the name so that the test runner instantiates the rendering server and various other things.
TEST_CASE("[GDSDecomp][ProjectRecovery] ([SceneTree]) Recover projects ") {
	// get a list of all version numbers in the test_projects/exported' directory
	String test_projects_path = get_test_projects_path();
	String original_path = test_projects_path.path_join("original");
	String exported_path = test_projects_path.path_join("exported");
	Vector<String> versions = gdre::get_dirs_at(exported_path, {}, false);
	for (const String &version : versions) {
		Vector<String> sub_projects = gdre::get_recursive_dir_list(exported_path.path_join(version), { "*.pck", "*.apk" }, false);
		for (const String &sub_project : sub_projects) {
			SUBCASE(vformat("%s: Test recover project %s", version, sub_project).utf8().get_data()) {
				test_export_project(version, sub_project, original_path, exported_path);
			}
		}
	}
}


// ---------------------------------------------------------------------------------------------------------------
// Tests for the settings that affect recovery (see CoreSettings in utility/gdre_config.h).
// The [ConfigTest] tag makes the test listener reset GDREConfig before and after each test case.
// ---------------------------------------------------------------------------------------------------------------

static constexpr const char *CONFIG_TEST_SUB_PROJECT = "2d/platformer/platformer.pck";
static constexpr const char *CONFIG_TEST_V3 = "3.5.3";
static constexpr const char *CONFIG_TEST_V4 = "4.5.1";
static constexpr const char *CONFIG_TEST_PRE_4_5 = "4.3.0";

// Loads the exported test project, dumps the pck, runs the full export, and unloads the project again.
static Error recover_to_dir(const String &p_version, const String &p_out_dir, Ref<ImportExporterReport> *r_report = nullptr) {
	String pck_path = get_test_projects_path().path_join("exported").path_join(p_version).path_join(CONFIG_TEST_SUB_PROJECT);
	ERR_FAIL_COND_V_MSG(!FileAccess::exists(pck_path), ERR_FILE_NOT_FOUND, "Test project not found: " + pck_path);
	gdre::rimraf(p_out_dir);

	Error err = GDRESettings::get_singleton()->load_project({ pck_path }, false);
	ERR_FAIL_COND_V(err != OK, err);
	{
		PckDumper dumper;
		err = dumper.pck_dump_to_dir(p_out_dir, {});
		if (err == OK) {
			ImportExporter import_exporter;
			err = import_exporter.export_imports(p_out_dir, {});
			if (r_report) {
				*r_report = import_exporter.get_report();
			}
		}
	}
	GDRESettings::get_singleton()->close_log_file();
	GDRESettings::get_singleton()->unload_project();
	return err;
}

static String get_config_test_out_dir(const String &p_name) {
	return get_tmp_path().path_join("config_recovery").path_join(p_name);
}

static int count_files_recursive(const String &p_dir) {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return 0;
	}
	da->set_include_hidden(true);
	int count = da->get_files().size();
	for (const String &dir : da->get_directories()) {
		count += count_files_recursive(p_dir.path_join(dir));
	}
	return count;
}

static bool git_is_available() {
	int exit_code = -1;
	return OS::get_singleton()->execute("git", { "--version" }, nullptr, &exit_code, true) == OK && exit_code == 0;
}

TEST_CASE("[GDSDecomp][ProjectRecovery][ConfigTest] ([SceneTree]) Recovery/write_json_report") {
	const String out_dir = get_config_test_out_dir("json_report");
	const String json_path = out_dir.path_join("gdre_export.json");
	Ref<ImportExporterReport> report;

	SUBCASE("Disabled by default") {
		CHECK_FALSE(bool(GDREConfig::get_singleton()->get_setting(CoreSettings::RECOVERY_WRITE_JSON_REPORT)));
		CHECK_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir, &report), OK);
		CHECK_FALSE(FileAccess::exists(json_path));
	}
	SUBCASE("Enabled") {
		TestConfigHelpers::ScopedSetting scoped(CoreSettings::RECOVERY_WRITE_JSON_REPORT, true);
		CHECK_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir, &report), OK);
		REQUIRE(FileAccess::exists(json_path));
		REQUIRE(report.is_valid());
		Variant parsed = JSON::parse_string(FileAccess::get_file_as_string(json_path));
		REQUIRE_EQ(parsed.get_type(), Variant::DICTIONARY);
		Dictionary parsed_dict = parsed;
		Dictionary expected = report->to_json();
		CHECK_GT(parsed_dict.size(), 0);
		for (const Variant &key : expected.keys()) {
			INFO(String(key));
			CHECK(parsed_dict.has(key));
		}
	}
	SUBCASE("Disabled explicitly") {
		TestConfigHelpers::ScopedSetting scoped(CoreSettings::RECOVERY_WRITE_JSON_REPORT, false);
		CHECK_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir, &report), OK);
		CHECK_FALSE(FileAccess::exists(json_path));
	}
	gdre::rimraf(out_dir);
}

TEST_CASE("[GDSDecomp][ProjectRecovery][ConfigTest] ([SceneTree]) Recovery/delete_auto_converted_files") {
	// Godot 3 projects have the most remapped (auto-converted) files
	const String out_dir = get_config_test_out_dir("auto_converted");
	const String autoconverted_dir = out_dir.path_join(".autoconverted");

	int moved_files = 0;
	{
		TestConfigHelpers::ScopedSetting scoped(CoreSettings::RECOVERY_DELETE_AUTO_CONVERTED_FILES, false);
		REQUIRE_EQ(recover_to_dir(CONFIG_TEST_V3, out_dir), OK);
		moved_files = count_files_recursive(autoconverted_dir);
	}
	// Without the setting, the auto-converted files are kept; with it, they're deleted.
	if (moved_files == 0) {
		WARN_MESSAGE(false, "No auto-converted files in the test project; the 'kept' half of this test did not check anything");
	}
	{
		TestConfigHelpers::ScopedSetting scoped(CoreSettings::RECOVERY_DELETE_AUTO_CONVERTED_FILES, true);
		REQUIRE_EQ(recover_to_dir(CONFIG_TEST_V3, out_dir), OK);
		CHECK_EQ(count_files_recursive(autoconverted_dir), 0);
	}
	gdre::rimraf(out_dir);
}

TEST_CASE("[GDSDecomp][ProjectRecovery][ConfigTest] ([SceneTree]) Recovery/git/create_git_repo") {
	if (!git_is_available()) {
		MESSAGE("git is not installed, skipping");
		return;
	}
	const String out_dir = get_config_test_out_dir("git_repo");

	SUBCASE("Disabled by default") {
		CHECK_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir), OK);
		CHECK_FALSE(DirAccess::dir_exists_absolute(out_dir.path_join(".git")));
		CHECK_FALSE(FileAccess::exists(out_dir.path_join(".gitignore")));
	}
	SUBCASE("Enabled") {
		TestConfigHelpers::ScopedSetting scoped(CoreSettings::RECOVERY_GIT_CREATE_GIT_REPO, true);
		CHECK_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir), OK);
		CHECK(DirAccess::dir_exists_absolute(out_dir.path_join(".git")));
		CHECK(FileAccess::exists(out_dir.path_join(".gitignore")));
	}
	gdre::rimraf(out_dir);
}

TEST_CASE("[GDSDecomp][ProjectRecovery][ConfigTest] ([SceneTree]) Recovery/git/add_imports_to_git_repo") {
	if (!git_is_available()) {
		MESSAGE("git is not installed, skipping");
		return;
	}
	const String out_dir = get_config_test_out_dir("git_imports");
	TestConfigHelpers::ScopedSetting create_repo(CoreSettings::RECOVERY_GIT_CREATE_GIT_REPO, true);

	auto read_gitignore_lines = [&]() {
		return FileAccess::get_file_as_string(out_dir.path_join(".gitignore")).split("\n", false);
	};

	SUBCASE("Godot 3: imports are ignored unless enabled") {
		SUBCASE("Disabled (default)") {
			CHECK_FALSE(bool(GDREConfig::get_singleton()->get_setting(CoreSettings::RECOVERY_GIT_ADD_IMPORTS_TO_GIT_REPO)));
			REQUIRE_EQ(recover_to_dir(CONFIG_TEST_V3, out_dir), OK);
			CHECK(read_gitignore_lines().has(".import/"));
		}
		SUBCASE("Enabled") {
			TestConfigHelpers::ScopedSetting scoped(CoreSettings::RECOVERY_GIT_ADD_IMPORTS_TO_GIT_REPO, true);
			REQUIRE_EQ(recover_to_dir(CONFIG_TEST_V3, out_dir), OK);
			CHECK_FALSE(read_gitignore_lines().has(".import/"));
		}
	}
	SUBCASE("Godot 4: imports are un-ignored only when enabled") {
		SUBCASE("Disabled (default)") {
			REQUIRE_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir), OK);
			CHECK_FALSE(read_gitignore_lines().has("!.godot/imported/"));
		}
		SUBCASE("Enabled") {
			TestConfigHelpers::ScopedSetting scoped(CoreSettings::RECOVERY_GIT_ADD_IMPORTS_TO_GIT_REPO, true);
			REQUIRE_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir), OK);
			CHECK(read_gitignore_lines().has("!.godot/imported/"));
		}
	}
	gdre::rimraf(out_dir);
}

TEST_CASE("[GDSDecomp][ProjectRecovery][ConfigTest] ([SceneTree]) Script/Indent settings are written to the recovered project") {
	const String out_dir = get_config_test_out_dir("indent");
	const String project_path = out_dir.path_join("project.godot");

	SUBCASE("Godot 4.5+: editor_overrides use the configured indentation") {
		SUBCASE("Defaults") {
			REQUIRE_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir), OK);
			String text = FileAccess::get_file_as_string(project_path);
			CHECK(text.contains("text_editor/behavior/indent/type=0"));
			CHECK(text.contains("text_editor/behavior/indent/size=4"));
		}
		SUBCASE("Spaces, size 2") {
			TestConfigHelpers::ScopedSetting type(CoreSettings::SCRIPT_INDENT_TYPE, 1);
			TestConfigHelpers::ScopedSetting size(CoreSettings::SCRIPT_INDENT_SIZE, 2);
			REQUIRE_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir), OK);
			String text = FileAccess::get_file_as_string(project_path);
			CHECK(text.contains("text_editor/behavior/indent/type=1"));
			CHECK(text.contains("text_editor/behavior/indent/size=2"));
		}
	}
	SUBCASE("Before Godot 4.5: no editor_overrides are written") {
		TestConfigHelpers::ScopedSetting type(CoreSettings::SCRIPT_INDENT_TYPE, 1);
		TestConfigHelpers::ScopedSetting size(CoreSettings::SCRIPT_INDENT_SIZE, 2);
		REQUIRE_EQ(recover_to_dir(CONFIG_TEST_PRE_4_5, out_dir), OK);
		String text = FileAccess::get_file_as_string(project_path);
		CHECK_FALSE(text.contains("text_editor/behavior/indent/"));
	}
	gdre::rimraf(out_dir);
}

TEST_CASE("[GDSDecomp][ProjectRecovery][ConfigTest] ([SceneTree]) Recovery works with threading settings") {
	const String out_dir = get_config_test_out_dir("threading");
	Ref<ImportExporterReport> report;

	SUBCASE("General/force_single_threaded") {
		TestConfigHelpers::ScopedSetting scoped(CoreSettings::FORCE_SINGLE_THREADED, true);
		CHECK_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir, &report), OK);
		REQUIRE(report.is_valid());
		CHECK_EQ(report->get_failed().size(), 0);
		CHECK_GT(report->get_successes().size(), 0);
	}
	SUBCASE("General/max_cores_to_use") {
		TestConfigHelpers::ScopedSetting scoped(CoreSettings::MAX_CORES_TO_USE, 1);
		CHECK_EQ(recover_to_dir(CONFIG_TEST_V4, out_dir, &report), OK);
		REQUIRE(report.is_valid());
		CHECK_EQ(report->get_failed().size(), 0);
		CHECK_GT(report->get_successes().size(), 0);
	}
	gdre::rimraf(out_dir);
}

// Note: Recovery/download_plugins is covered by the config-level tests in test_gdre_config.cpp.
// Testing its effect on recovery would need a project with a missing GDExtension and network access.

} //namespace TestProjectExport
