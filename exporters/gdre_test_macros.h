#pragma once
#if TESTS_ENABLED
#include "tests/test_macros.h"
#else
// if it fails, print the expression
#define CHECK(...) \
	if (!(__VA_ARGS__)) {\
		err = FAILED; \
		ERR_PRINT("CHECK failed: " + String(#__VA_ARGS__));\
	}\

#define REQUIRE(...) \
	if (!(__VA_ARGS__)) {\
		err = FAILED; \
		ERR_PRINT("REQUIRE failed: " + String(#__VA_ARGS__));\
	}\
	return err;

// TODO: define CHECK_EQ, REQUIRE, etc.
#define CHECK_EQ(a, b) \
	if (!(a == b)) {\
		err = FAILED; \
		ERR_PRINT("CHECK_EQ failed: " + String(#a) + " != " + String(#b));\
	}

#define REQUIRE_GE(a, b) \
	if (!(a >= b)) {\
		err = FAILED; \
		ERR_PRINT("REQUIRE_GE failed: " + String(#a) + " < " + String(#b));\
	}\
	return err;

#endif
