#pragma once

#include "core/string/ustring.h"
#include "core/variant/variant.h"

#include "utility/gdre_config.h"

// NOTE: This is deliberately not a .h file; every .h in the tests dir is included from tests/test_main.cpp (via
// modules_tests.gen.h), where module-relative includes don't resolve.
//
// Helpers for tests that touch the global GDREConfig singleton.
//
// Test cases tagged with `[ConfigTest]` in their name get a pristine GDREConfig (all settings reset to their
// defaults, config file redirected to a temp path) from the GDRETestListener in test_common.cpp, and get it reset
// again afterwards. ScopedSetting is for flipping a value within a single test body/subcase.
namespace TestConfigHelpers {

// Sets a setting for the duration of the scope, and restores the previous value afterwards.
class ScopedSetting {
	String key;
	bool was_ephemeral = false;
	Variant old_value;

public:
	ScopedSetting(const String &p_key, const Variant &p_value) :
			key(p_key) {
		GDREConfig *cfg = GDREConfig::get_singleton();
		was_ephemeral = cfg->is_ephemeral_setting(key);
		old_value = cfg->get_setting(key, cfg->get_default_value(key));
		cfg->set_setting(key, p_value, was_ephemeral);
	}

	~ScopedSetting() {
		GDREConfig::get_singleton()->set_setting(key, old_value, was_ephemeral);
	}

	ScopedSetting(const ScopedSetting &) = delete;
	ScopedSetting &operator=(const ScopedSetting &) = delete;
};

} //namespace TestConfigHelpers
