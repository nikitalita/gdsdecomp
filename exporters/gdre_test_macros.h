#pragma once
// if it fails, print the expression
#define _GDRE_CHECK(...)                                               \
	if (!(__VA_ARGS__)) {                                              \
		err = FAILED;                                                  \
		ERR_PRINT(#__VA_ARGS__ ": " + String((Variant)(__VA_ARGS__))); \
	}

#define _GDRE_REQUIRE(...)                                             \
	if (!(__VA_ARGS__)) {                                              \
		err = FAILED;                                                  \
		ERR_PRINT(#__VA_ARGS__ ": " + String((Variant)(__VA_ARGS__))); \
		return err;                                                    \
	}

#define _GDRE_CHECK_GE(a, b)                                                                         \
	if (!(a >= b)) {                                                                                 \
		err = FAILED;                                                                                \
		ERR_PRINT(#a " is less than" #b ": " + String((Variant)(a)) + " < " + String((Variant)(b))); \
		return err;                                                                                  \
	}

#define _GDRE_CHECK_GT(a, b)                                                                                      \
	if (!(a > b)) {                                                                                               \
		err = FAILED;                                                                                             \
		ERR_PRINT(#a " is less than or equal to" #b ": " + String((Variant)(a)) + " <= " + String((Variant)(b))); \
		return err;                                                                                               \
	}

// TODO: define CHECK_EQ, REQUIRE, etc.
#define _GDRE_CHECK_EQ(a, b)                                                                              \
	if (!(a == b)) {                                                                                      \
		err = FAILED;                                                                                     \
		ERR_PRINT(#a " is not equal to " #b ": " + String((Variant)(a)) + " != " + String((Variant)(b))); \
	}

#define _GDRE_REQUIRE_GE(a, b)                                                                       \
	if (!(a >= b)) {                                                                                 \
		err = FAILED;                                                                                \
		ERR_PRINT(#a " is less than" #b ": " + String((Variant)(a)) + " < " + String((Variant)(b))); \
		return err;                                                                                  \
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

#define GDRE_REQUIRE_CONTINUE_EQ(a, b) \
	if (GDRESettings::is_testing()) {  \
		CHECK_EQ(a, b);                \
	} else {                           \
		_GDRE_CHECK_EQ(a, b);          \
	}                                  \
	if (!(a == b)) {                   \
		continue;                      \
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
#define GDRE_REQUIRE_GE(a, b) _GDRE_REQUIRE_GE(a, b)

#endif
