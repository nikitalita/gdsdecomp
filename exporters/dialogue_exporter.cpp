#include "dialogue_exporter.h"
#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "utility/common.h"

int get_dialogue_version(const Ref<ImportInfo> &import_infos) {
	auto importer = import_infos->get_importer();
	if (importer == "dialogue_manager") {
		auto ver = import_infos->get_iinfo_val("remap", "importer_version");
		return ver.get_type() == Variant::INT ? ver.operator int() : 15;
	}
	auto parts = importer.split("_");
	String importer_number = parts[parts.size() - 1];
	return importer_number.to_int();
}

String DialogueExporter::get_name() const {
	return EXPORTER_NAME;
}

Error DialogueExporter::export_file(const String &out_path, const String &res_path) {
	auto res = ResourceCompatLoader::fake_load(res_path);
	ERR_FAIL_COND_V_MSG(res.is_null(), ERR_FILE_CANT_OPEN, "Failed to load resource: " + res_path);
	bool is_valid = false;
	String content = res->get("raw_text", &is_valid);
	// This is an earlier version (< version 11) which did not have the raw_text field
	// TODO: We can potentially recreate the text from the `lines` property for earlier versions?
	// we would have to recreate a bunch of syntax though as it doesn't have the literal text.
	if (!is_valid) {
		return ERR_UNAVAILABLE;
	} else if (content.is_empty()) {
		return ERR_CANT_CREATE;
	}
	gdre::ensure_dir(out_path.get_base_dir());
	Ref<FileAccess> fa = FileAccess::open(out_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(fa.is_null(), ERR_FILE_CANT_OPEN, "Failed to open file: " + out_path);
	if (!fa->store_string(content)) {
		return ERR_FILE_CANT_WRITE;
	}
	return OK;
}

Ref<ExportReport> DialogueExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	Ref<ExportReport> report = Ref<ExportReport>(memnew(ExportReport(import_infos)));
	auto dest = output_dir.path_join(import_infos->get_export_dest().trim_prefix("res://"));
	report->set_error(export_file(dest, import_infos->get_path()));
	report->set_resources_used({ import_infos->get_path() });
	if (report->get_error() == OK) {
		// Started writing this in version 8.
		if (import_infos->get_ver_major() >= 4 && get_dialogue_version(import_infos) >= 8) {
			report->get_import_info()->set_params({ { "defaults", true } });
		}
		report->set_saved_path(dest);
	} else if (report->get_error() == ERR_UNAVAILABLE) {
		report->set_unsupported_format_type("Dialogue v" + String::num_int64(get_dialogue_version(import_infos)));
		report->set_message("Dialogue file did not have the 'raw_text' field, which is required for conversion.");
	}
	return report;
}

void DialogueExporter::get_handled_types(List<String> *out) const {
}

void DialogueExporter::get_handled_importers(List<String> *out) const {
	out->push_back("dialogue_manager");
	for (int i = 0; i <= 15; i++) {
		out->push_back("dialogue_manager_compiler_" + String::num_int64(i));
	}
}

String DialogueExporter::get_default_export_extension(const String &res_path) const {
	return "dialogue";
}
