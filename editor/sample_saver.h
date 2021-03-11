
#ifndef SAMPLE_SAVER_H
#define SAMPLE_SAVER_H

#include "scene/resources/sample.h"

class SampleSaver: public Sample {
    OBJ_TYPE(SampleSaver, Resource);
	RES_BASE_EXTENSION("smp");

public:
	Error save_to_wav(const String &p_path);

};

#endif