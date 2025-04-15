#pragma once
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/string/ustring.h"
class TextDiff : public Object {
	GDCLASS(TextDiff, Object);

protected:
	static void _bind_methods();

public:
	static String get_diff_with_header(const String &old_path, const String &new_path, const String &old_text, const String &new_text);
	static String get_diff(const String &old_text, const String &new_text);
	static void print_diff(const String &diff);
};