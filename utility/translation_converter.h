#ifndef TRANSLATION_CONVERTER_H
#define TRANSLATION_CONVERTER_H

#include "core/object/object.h"
#include "core/object/ref_counted.h"

class TranslationConverter : public RefCounted {
	GDCLASS(TranslationConverter, RefCounted);

protected:
	static void _bind_methods();

public:
	static Vector<String> convert_translation_file(const String src_path);

	TranslationConverter();
	~TranslationConverter();
};
#endif
