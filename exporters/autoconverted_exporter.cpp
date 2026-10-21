#include "autoconverted_exporter.h"
#include "compat/resource_compat_text.h"
#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "exporters/export_report.h"
#include "utility/gdre_settings.h"

// This is to handle the rare case where an autoconverted resources don't match the project version.
// This usually happens when it is a leftover from a previous project export on a previous version of the engine.
static inline uint32_t get_text_conversion_flags() {
	uint32_t flags = 0;
	if (GDRESettings::get_singleton()->is_pack_loaded()) {
		auto major = GDRESettings::get_singleton()->get_ver_major();
		auto minor = GDRESettings::get_singleton()->get_ver_minor();
		int format_version = ResourceFormatSaverCompatText::get_default_format_version(major, minor);
		flags = CompatFormatLoader::set_version_info_in_flags(flags, format_version, major, minor);
	}
	return flags;
}

Error AutoConvertedExporter::export_file(const String &p_dest_path, const String &p_src_path) {
	return ResourceCompatLoader::to_text(p_src_path, p_dest_path, get_text_conversion_flags());
}

Ref<ExportReport> AutoConvertedExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	// Check if the exporter can handle the given importer and resource type
	String src_path = import_infos->get_path();
	String dst_path = output_dir.path_join(import_infos->get_export_dest().replace("res://", ""));
	Ref<ExportReport> report = memnew(ExportReport(import_infos, get_name()));
	report->set_resources_used({ import_infos->get_path() });
	Error err = ResourceCompatLoader::to_text(src_path, dst_path, get_text_conversion_flags(), import_infos->get_source_file());
	report->set_error(err);
	report->set_saved_path(dst_path);
	return report;
}

void AutoConvertedExporter::get_handled_types(List<String> *out) const {
}

void AutoConvertedExporter::get_handled_importers(List<String> *out) const {
	out->push_back("autoconverted");
}

String AutoConvertedExporter::get_name() const {
	return EXPORTER_NAME;
}

String AutoConvertedExporter::get_default_export_extension(const String &res_path) const {
	if (res_path.get_extension().to_lower() == "scn") {
		return "tscn";
	}
	return "tres";
}

Vector<String> AutoConvertedExporter::get_export_extensions(const String &res_path) const {
	if (res_path.get_extension().to_lower() == "scn") {
		return { "tscn", "tres" };
	}
	return { "tres" };
}
