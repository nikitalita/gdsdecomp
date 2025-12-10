#include "test_project_export.h"

#include "core/io/json.h"

#include "exporters/export_report.h"
#include "exporters/resource_exporter.h"
#include "exporters/translation_exporter.h"
#include "test_common.h"
#include "tests/test_macros.h"
#include "utility/common.h"
#include "utility/import_exporter.h"
#include "utility/pck_dumper.h"
#include <compat/resource_loader_compat.h>

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
}
