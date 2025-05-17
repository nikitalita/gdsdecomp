#include "oggstr_loader_compat.h"
#include "core/error/error_macros.h"
#include "modules/vorbis/audio_stream_ogg_vorbis.h"

Ref<Resource> OggStreamConverterCompat::convert(const Ref<MissingResource> &res, ResourceInfo::LoadType p_type, int ver_major, Error *r_error) {
	String name = get_resource_name(res, ver_major);
	Vector<uint8_t> data = res->get("data");
	bool loop = res->get("loop");
	double loop_offset = res->get("loop_offset");
	Ref<AudioStreamOggVorbis> sample = AudioStreamOggVorbis::load_from_buffer(data);
	ERR_FAIL_COND_V_MSG(sample.is_null(), res, "Failed to load Ogg Vorbis stream from buffer.");
	if (!name.is_empty()) {
		sample->set_name(name);
	}
	sample->set_loop(loop);
	sample->set_loop_offset(loop_offset);
	auto info = ResourceInfo::get_info_from_resource(res);
	ERR_FAIL_COND_V_MSG(!info.is_valid(), res, "Missing resource has no compat metadata??????????? This should have been set by the missing resource instance function(s)!!!!!!!!");
	if (info.is_valid()) {
		info->set_on_resource(sample);
	}
	return sample;
}

bool OggStreamConverterCompat::handles_type(const String &p_type, int ver_major) const {
	return ((p_type == "AudioStreamOGGVorbis" || p_type == "AudioStreamOggVorbis") && ver_major <= 3);
}