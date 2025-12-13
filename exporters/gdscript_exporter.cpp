#include "gdscript_exporter.h"

#include "bytecode/bytecode_base.h"
#include "compat/fake_gdscript.h"
#include "compat/fake_script.h"
#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "core/variant/variant.h"
#include "exporters/export_report.h"
#include "gdre_test_macros.h"
#include "utility/common.h" // For gdre namespace
#include "utility/gdre_settings.h"
void GDScriptExporter::_bind_methods() {
}

Error GDScriptExporter::export_file(const String &out_path, const String &res_path) {
	Error err = OK;
	Ref<FakeGDScript> gdscript;
	gdscript.instantiate();
	err = gdscript->load_source_code(res_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to load script: " + res_path);
	return _export_file(out_path, gdscript);
}

Error GDScriptExporter::_export_file(const String &out_path, Ref<FakeGDScript> gdscript) {
	String source = gdscript->get_source_code();
	ERR_FAIL_COND_V_MSG(source.is_empty(), ERR_FILE_CORRUPT, "Script source is empty: " + gdscript->get_script_path());

	Error err = gdre::ensure_dir(out_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to ensure output directory exists: " + out_path.get_base_dir());

	Ref<FileAccess> f = FileAccess::open(out_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_FILE_CANT_WRITE, "Cannot write to file: " + out_path);

	f->store_string(source);
	return OK;
}

Ref<ExportReport> GDScriptExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	Ref<ExportReport> report = memnew(ExportReport(import_infos, get_name()));
	report->set_resources_used({ import_infos->get_path() });

	String import_path = import_infos->get_path();
	String export_path = output_dir.path_join(import_infos->get_export_dest().replace("res://", ""));

	// Handle encrypted scripts
	bool is_encrypted = import_path.get_extension().to_lower() == "gde";
	if (is_encrypted) {
		auto key = GDRESettings::get_singleton()->get_encryption_key();
		if (key.size() == 0) {
			report->set_error(ERR_UNAUTHORIZED);
			report->set_message("No encryption key provided for encrypted script");
			return report;
		}
	}
	Ref<FakeGDScript> gdscript;
	gdscript.instantiate();
	Error err = gdscript->load_source_code(import_path);
	if (err != OK) {
		report->set_error(err);
		if (is_encrypted && err == ERR_UNAUTHORIZED) {
			report->set_message("Encryption key is incorrect for encrypted script");
		} else {
			report->set_message(gdscript->get_error_message());
		}
		return report;
	}
	if (gdscript->get_source_code().is_empty()) {
		report->set_error(ERR_FILE_CORRUPT);
		report->set_message("Script source is empty");
		return report;
	}
	// Export the script
	err = _export_file(export_path, gdscript);
	if (err != OK) {
		report->set_error(err);
		return report;
	}
	report->set_saved_path(export_path);
	return report;
}

void GDScriptExporter::get_handled_types(List<String> *out) const {
	out->push_back("GDScript");
}

void GDScriptExporter::get_handled_importers(List<String> *out) const {
	out->push_back("script_bytecode");
}

bool GDScriptExporter::supports_multithread() const {
	return true;
}

String GDScriptExporter::get_name() const {
	return EXPORTER_NAME;
}

String GDScriptExporter::get_default_export_extension(const String &res_path) const {
	return "gd";
}

Error GDScriptExporter::test_export(const Ref<ExportReport> &export_report, const String &original_project_dir) const {
	Error _ret_err = OK;
	{
		String exported_resource = export_report->get_saved_path();
		String original_compiled_resource = export_report->get_resources_used()[0];
		String exported_script_text = FileAccess::get_file_as_string(exported_resource);
		Vector<uint8_t> original_bytecode;
		if (original_compiled_resource.get_extension().to_lower() == "gde") {
			Error encrypted_err = GDScriptDecomp::get_buffer_encrypted(original_compiled_resource, 3, GDRESettings::get_singleton()->get_encryption_key(), original_bytecode);
			GDRE_CHECK_EQ(encrypted_err, OK);
		} else {
			original_bytecode = FileAccess::get_file_as_bytes(original_compiled_resource);
		}
		GDRE_CHECK(!exported_script_text.is_empty());
		GDRE_CHECK(!original_bytecode.is_empty());

		auto decomp = GDScriptDecomp::create_decomp_for_commit(GDRESettings::get_singleton()->get_bytecode_revision());
		GDRE_REQUIRE(decomp.is_valid());

		// Bytecode may not be exactly the same due to earlier Godot variant encoder failing to zero out the padding bytes,
		// so we need to use the tester function to compare the bytecode
		auto compiled_exported_bytecode = decomp->compile_code_string(exported_script_text);
		Error bytecode_err = decomp->test_bytecode_match(original_bytecode, compiled_exported_bytecode, false, true);
		GDRE_CHECK_EQ(decomp->get_error_message(), "");
		GDRE_CHECK_EQ(bytecode_err, OK);

		if (!original_project_dir.is_empty()) {
			String original_script_path = original_project_dir.path_join(export_report->get_import_info()->get_source_file().trim_prefix("res://"));
			String original_script_text = FileAccess::get_file_as_string(original_script_path);
			GDRE_CHECK(!original_script_text.is_empty());
			auto compiled_original_bytecode = decomp->compile_code_string(original_script_text);
			Error bytecode_err2 = decomp->test_bytecode_match(compiled_exported_bytecode, compiled_original_bytecode, false, true);
			GDRE_CHECK_EQ(decomp->get_error_message(), "");
			GDRE_CHECK_EQ(bytecode_err2, OK);
		}
	}
	return _ret_err;
}
