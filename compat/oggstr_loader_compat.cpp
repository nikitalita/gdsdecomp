#include "oggstr_loader_compat.h"
#include "core/error/error_macros.h"
#include "modules/vorbis/audio_stream_ogg_vorbis.h"

Ref<Resource> OggStreamConverterCompat::convert(const Ref<MissingResource> &res, ResourceInfo::LoadType p_type, int ver_major, Error *r_error) {
	Vector<uint8_t> data = res->get("data");
	bool loop = res->get("loop");
	double loop_offset = res->get("loop_offset");
	Ref<AudioStreamOggVorbis> sample = AudioStreamOggVorbis::load_from_buffer(data);
	ERR_FAIL_COND_V_MSG(sample.is_null(), res, "Failed to load Ogg Vorbis stream from buffer.");
	sample->set_loop(loop);
	sample->set_loop_offset(loop_offset);
	merge_meta_and_resource_only_props(sample, res);
	return sample;
}

bool OggStreamConverterCompat::handles_type(const String &p_type, int ver_major) const {
	return ((p_type == "AudioStreamOGGVorbis" || p_type == "AudioStreamOggVorbis") && ver_major <= 3);
}
