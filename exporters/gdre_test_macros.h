#pragma once
// if it fails, print the expression
#define _GDRE_CHECK(...)                                               \
	if (!(__VA_ARGS__)) {                                              \
		_ret_err = FAILED;                                             \
		ERR_PRINT(#__VA_ARGS__ ": " + String((Variant)(__VA_ARGS__))); \
	}

#define _GDRE_REQUIRE(...)                                             \
	if (!(__VA_ARGS__)) {                                              \
		_ret_err = FAILED;                                             \
		ERR_PRINT(#__VA_ARGS__ ": " + String((Variant)(__VA_ARGS__))); \
		return _ret_err;                                               \
	}

#define _GDRE_CHECK_GE(a, b)                                                                         \
	if (!(a >= b)) {                                                                                 \
		_ret_err = FAILED;                                                                           \
		ERR_PRINT(#a " is less than" #b ": " + String((Variant)(a)) + " < " + String((Variant)(b))); \
		return _ret_err;                                                                             \
	}

#define _GDRE_CHECK_GT(a, b)                                                                                      \
	if (!(a > b)) {                                                                                               \
		_ret_err = FAILED;                                                                                        \
		ERR_PRINT(#a " is less than or equal to" #b ": " + String((Variant)(a)) + " <= " + String((Variant)(b))); \
		return _ret_err;                                                                                          \
	}

// TODO: define CHECK_EQ, REQUIRE, etc.
#define _GDRE_CHECK_EQ(a, b)                                                                              \
	if (!(a == b)) {                                                                                      \
		_ret_err = FAILED;                                                                                \
		ERR_PRINT(#a " is not equal to " #b ": " + String((Variant)(a)) + " != " + String((Variant)(b))); \
	}

#define _GDRE_REQUIRE_GE(a, b)                                                                       \
	if (!(a >= b)) {                                                                                 \
		_ret_err = FAILED;                                                                           \
		ERR_PRINT(#a " is less than" #b ": " + String((Variant)(a)) + " < " + String((Variant)(b))); \
		return _ret_err;                                                                             \
	}

#if TESTS_ENABLED
#include "tests/test_macros.h"
#include "utility/gdre_settings.h"

#define GDRE_CHECK(...)               \
	if (GDRESettings::is_testing()) { \
		CHECK(__VA_ARGS__);           \
	} else {                          \
		_GDRE_CHECK(__VA_ARGS__);     \
	}

#define GDRE_REQUIRE(...)             \
	if (GDRESettings::is_testing()) { \
		REQUIRE(__VA_ARGS__);         \
	} else {                          \
		_GDRE_REQUIRE(__VA_ARGS__);   \
	}

#define GDRE_CHECK_EQ(a, b)           \
	if (GDRESettings::is_testing()) { \
		CHECK_EQ(a, b);               \
	} else {                          \
		_GDRE_CHECK_EQ(a, b);         \
	}

#define GDRE_CHECK_GE(a, b)           \
	if (GDRESettings::is_testing()) { \
		CHECK_GE(a, b);               \
	} else {                          \
		_GDRE_CHECK_GE(a, b);         \
	}

#define GDRE_CHECK_GT(a, b)           \
	if (GDRESettings::is_testing()) { \
		CHECK_GT(a, b);               \
	} else {                          \
		_GDRE_CHECK_GT(a, b);         \
	}

#define GDRE_REQUIRE_GE(a, b)         \
	if (GDRESettings::is_testing()) { \
		REQUIRE_GE(a, b);             \
	} else {                          \
		_GDRE_REQUIRE_GE(a, b);       \
	}
#else

#define GDRE_CHECK(...) _GDRE_CHECK(__VA_ARGS__)
#define GDRE_REQUIRE(...) _GDRE_REQUIRE(__VA_ARGS__)
#define GDRE_CHECK_EQ(a, b) _GDRE_CHECK_EQ(a, b)
#define GDRE_CHECK_GE(a, b) _GDRE_CHECK_GE(a, b)
#define GDRE_CHECK_GT(a, b) _GDRE_CHECK_GT(a, b)
#define GDRE_REQUIRE_GE(a, b) _GDRE_REQUIRE_GE(a, b)

#endif

#define GDRE_REQUIRE_CONTINUE_EQ(a, b) \
	CHECK_EQ(a, b);                    \
	if (!(a == b)) {                   \
		continue;                      \
	}

#include "core/string/ustring.h"
#include "core/templates/vector.h"

namespace gdre_test {
template <typename T>
String get_error_message_for_vector_mismatch(const T &original_data, const T &exported_data) {
	String error_message;
	auto get_num_mis_matches = [&](int orig_offset, int exp_offset) -> Pair<int, int> {
		int num_mismatches = 0;
		int first_mismatch = -1;
		int64_t max_index = MAX(original_data.size() - orig_offset, exported_data.size() - exp_offset);
		for (int i = 0; i < max_index; i++) {
			if (exported_data.size() <= i + exp_offset) {
				if (first_mismatch == -1) {
					first_mismatch = i;
				}
				num_mismatches++;
				continue;
			} else if (original_data.size() <= i + orig_offset) {
				if (first_mismatch == -1) {
					first_mismatch = i;
				}
				num_mismatches++;
				continue;
			}
			if (original_data[i + orig_offset] != exported_data[i + exp_offset]) {
				if (first_mismatch == -1) {
					first_mismatch = i;
				}
				num_mismatches++;
			}
		}
		return Pair<int, int>(first_mismatch, num_mismatches);
	};
	if (original_data != exported_data) {
		bool size_mismatch = original_data.size() != exported_data.size();
		auto [first_mismatch, num_mismatches] = get_num_mis_matches(0, 0);

		bool shifted = false;
		if (size_mismatch && num_mismatches == MAX(original_data.size(), exported_data.size()) - first_mismatch) {
			int orig_offset = 0;
			int exp_offset = 0;
			if (exported_data.size() > original_data.size()) {
				exp_offset = first_mismatch;
			} else {
				orig_offset = first_mismatch;
			}
			auto [first, num] = get_num_mis_matches(orig_offset, exp_offset);
			shifted = num + first_mismatch < num_mismatches;
			num_mismatches = MAX(1, shifted ? num : num_mismatches);
		}
		String prefix = "First mismatch at";
		if (shifted) {
			prefix = "Shifted at";
		}
		error_message = vformat("%s %d/%d, num: %d", shifted ? "Shifted at" : "First mismatch at", first_mismatch, original_data.size(), num_mismatches);
		if (size_mismatch) {
			error_message += vformat(" (size: %d != %d)", original_data.size(), exported_data.size());
		}
	}
	return error_message;
}
} //namespace gdre_test

#define GDRE_CHECK_VECTOR_EQ(a, b) GDRE_CHECK_EQ(gdre_test::get_error_message_for_vector_mismatch(a, b), "");
