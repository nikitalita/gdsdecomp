
#ifndef SAMPLE_SAVER_H
#define SAMPLE_SAVER_H

#include "scene/resources/sample.h"

static class SampleSaver{
    public:
	static Error save_to_wav(const String &p_path, const Ref<Sample> &sample);
};

#endif