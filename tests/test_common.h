#pragma once
#include "core/string/ustring.h"
#include "core/templates/vector.h"

String get_gdsdecomp_path();

String get_tmp_path();

String get_test_resources_path();

String get_test_scripts_path();

String get_gdscript_tests_path();

String get_gdsdecomp_helpers_path();

Vector<String> get_test_versions();

String remove_comments(const String &script_text);
