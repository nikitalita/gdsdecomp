#pragma once

#include "tests/test_macros.h"
#include "utility/common.h"

class ExportReport;
class ImportInfo;
namespace TestProjectExport {

// change them all to declarations
String get_original_import_path(const Ref<ExportReport> &export_report, const String &original_extract_dir);
Error test_exported_texture_2d(const Ref<ExportReport> &export_report, const String &original_extract_dir, const String &version);
Error test_exported_wav(const Ref<ExportReport> &export_report, const String &original_extract_dir, const String &version);
Error test_exported_audio_stream_ogg_vorbis(const Ref<ExportReport> &export_report, const String &original_extract_dir, const String &version);
Error test_exported_mp3_audio_stream(const Ref<ExportReport> &export_report, const String &original_extract_dir, const String &version);
Error test_exported_gdscript(const Ref<ExportReport> &export_report, const String &original_extract_dir, const String &version);
Error test_json_import_info(const Ref<ImportInfo> &import_info);
Error test_json_export_report(const Ref<ExportReport> &export_report);
Error test_recovered_resource(const Ref<ExportReport> &export_report, const String &original_extract_dir, const String &version);
String get_test_projects_path();
Error test_export_project(const String &version, const String &sub_project, const String &original_parent_dir, const String &exported_parent_dir);

// TODO: might need to add '[Audio]'
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

} //namespace TestProjectExport
