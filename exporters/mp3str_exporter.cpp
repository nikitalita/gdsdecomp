#include "mp3str_exporter.h"
#include "compat/resource_loader_compat.h"
#include "exporters/export_report.h"
#include "gdre_test_macros.h"
#include "modules/mp3/audio_stream_mp3.h"
#include "utility/common.h"

Error Mp3StrExporter::export_file(const String &p_dest_path, const String &p_src_path) {
	Error err;

	Ref<AudioStreamMP3> sample = ResourceCompatLoader::non_global_load(p_src_path, "", &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load mp3str file " + p_src_path);

	err = _export_resource(sample, p_dest_path, p_src_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to export mp3str file " + p_src_path);

	print_verbose("Converted " + p_src_path + " to " + p_dest_path);
	return OK;
}

Error Mp3StrExporter::_export_resource(Ref<AudioStreamMP3> sample, const String &p_dest_path, const String &p_src_path) {
	Error err = gdre::ensure_dir(p_dest_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + p_dest_path);

	Ref<FileAccess> f = FileAccess::open(p_dest_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_CREATE, "Could not open " + p_dest_path + " for saving");

	PackedByteArray data = sample->get_data();
	f->store_buffer(data.ptr(), data.size());

	return OK;
}

Ref<ExportReport> Mp3StrExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	// Check if the exporter can handle the given importer and resource type

	String src_path = import_infos->get_path();
	String dst_path = output_dir.path_join(import_infos->get_export_dest().replace("res://", ""));
	Ref<ExportReport> report = memnew(ExportReport(import_infos, get_name()));
	report->set_resources_used({ import_infos->get_path() });
	Error err = OK;
	Ref<AudioStreamMP3> sample = ResourceCompatLoader::non_global_load(src_path, "", &err);
	ERR_FAIL_COND_V_MSG(err != OK, report, "Could not load mp3str file " + src_path);
	err = _export_resource(sample, dst_path, src_path);
	report->set_error(err);
	if (err == OK) {
		report->set_saved_path(dst_path);
		if (import_infos->get_ver_major() >= 4) {
			import_infos->set_param("loop", sample->has_loop());
			import_infos->set_param("loop_offset", sample->get_loop_offset());
			import_infos->set_param("bpm", sample->get_bpm());
			import_infos->set_param("beat_count", sample->get_beat_count());
			import_infos->set_param("bar_beats", sample->get_bar_beats());
		}
	}

	return report;
}

void Mp3StrExporter::get_handled_types(List<String> *out) const {
	out->push_back("AudioStreamMP3");
}

void Mp3StrExporter::get_handled_importers(List<String> *out) const {
	out->push_back("mp3");
}

String Mp3StrExporter::get_name() const {
	return EXPORTER_NAME;
}

String Mp3StrExporter::get_default_export_extension(const String &res_path) const {
	return "mp3";
}

Vector<String> Mp3StrExporter::get_export_extensions(const String &res_path) const {
	return { "mp3" };
}

Error Mp3StrExporter::test_export(const Ref<ExportReport> &export_report, const String &original_project_dir) const {
	Error _ret_err = OK;
	{
		auto dests = export_report->get_resources_used();
		GDRE_REQUIRE_GE(dests.size(), 1);
		String pck_resource = dests[0];
		String exported_resource = export_report->get_saved_path();
		Ref<AudioStreamMP3> pck_audio = ResourceCompatLoader::non_global_load(pck_resource);
		GDRE_CHECK(pck_audio.is_valid());
		Ref<AudioStreamMP3> exported_audio = AudioStreamMP3::load_from_file(exported_resource);
		GDRE_CHECK(exported_audio.is_valid());
		GDRE_CHECK_EQ(pck_audio->get_length(), exported_audio->get_length());
		GDRE_CHECK_VECTOR_EQ(pck_audio->get_data(), exported_audio->get_data());

		if (!original_project_dir.is_empty()) {
			String original_import_path = original_project_dir.path_join(export_report->get_import_info()->get_source_file().trim_prefix("res://"));
			Ref<AudioStreamMP3> original_audio = AudioStreamMP3::load_from_file(original_import_path);
			GDRE_CHECK(original_audio.is_valid());
			GDRE_CHECK_EQ(original_audio->get_length(), exported_audio->get_length());
			GDRE_CHECK_VECTOR_EQ(original_audio->get_data(), exported_audio->get_data());
		}
	}
	return _ret_err;
}
