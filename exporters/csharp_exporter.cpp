#include "csharp_exporter.h"

#include "compat/fake_csharp_script.h"
#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "utility/common.h" // For gdre namespace
#include "utility/gdre_settings.h"
#include "utility/godot_mono_decomp_wrapper.h"

void CSharpExporter::_bind_methods() {
}

Error CSharpExporter::export_file(const String &out_path, const String &res_path) {
	Error err = OK;
	ERR_FAIL_COND_V_MSG(!GDRESettings::get_singleton()->has_loaded_dotnet_assembly(), ERR_CANT_RESOLVE, "No dotnet assembly loaded");
	Ref<GodotMonoDecompWrapper> decompiler = GDRESettings::get_singleton()->get_dotnet_decompiler();
	if (decompiler.is_null()) {
		ERR_FAIL_V_MSG(ERR_CANT_RESOLVE, "No dotnet decompiler loaded");
	}
	auto source = decompiler->decompile_individual_file(res_path);
	if (source.is_empty()) {
		ERR_FAIL_V_MSG(ERR_CANT_RESOLVE, "Failed to decompile C# script: " + res_path);
	}

	err = gdre::ensure_dir(out_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to ensure output directory exists: " + out_path.get_base_dir());

	Ref<FileAccess> f = FileAccess::open(out_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_FILE_CANT_WRITE, "Cannot write to file: " + out_path);

	f->store_string(source);
	return OK;
}

Ref<ExportReport> CSharpExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	// This should not be used by the import exporter during full project recovery
	ERR_FAIL_V_MSG(Ref<ExportReport>(), "Not implemented");
}

void CSharpExporter::get_handled_types(List<String> *out) const {
	out->push_back("CSharpScript");
}

void CSharpExporter::get_handled_importers(List<String> *out) const {
}

bool CSharpExporter::supports_multithread() const {
	return true;
}

String CSharpExporter::get_name() const {
	return EXPORTER_NAME;
}

String CSharpExporter::get_default_export_extension(const String &res_path) const {
	return "cs";
}

Vector<String> CSharpExporter::get_export_extensions(const String &res_path) const {
	Vector<String> extensions;
	extensions.push_back("cs");
	return extensions;
}
