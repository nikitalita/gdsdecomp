#pragma once

// Intentionally not a header

#include "core/error/error_list.h"
#include "core/io/marshalls.h"
#include "core/string/ustring.h"

// Helpers shared by the version-specific marshalls_N implementations.

#define _S(a) ((int32_t)a)
#define ERR_FAIL_ADD_OF(a, b, err) ERR_FAIL_COND_V(_S(b) < 0 || _S(a) < 0 || _S(a) > INT_MAX - _S(b), err)
#define ERR_FAIL_MUL_OF(a, b, err) ERR_FAIL_COND_V(_S(a) < 0 || _S(b) <= 0 || _S(a) > INT_MAX / _S(b), err)

static Error _decode_string(const uint8_t *&buf, int &len, int *r_len, String &r_string) {
	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	int32_t strlen = decode_uint32(buf);
	int32_t pad = 0;

	// Handle padding
	if (strlen % 4) {
		pad = 4 - strlen % 4;
	}

	buf += 4;
	len -= 4;

	// Ensure buffer is big enough
	ERR_FAIL_ADD_OF(strlen, pad, ERR_FILE_EOF);
	ERR_FAIL_COND_V(strlen < 0 || strlen + pad > len, ERR_FILE_EOF);

	String str;
	// COMPAT: certain hacked Godot games have invalid utf-8 strings that parse valid on their version of Godot, so we need to handle them.
	Error err = str.append_utf8((const char *)buf, strlen);
	if (err) {
		// ERR_INVALID_DATA means that certain characters are not valid UTF-8 and were replaced with 0xFFFD; we can continue on after this error.
		if (err == ERR_INVALID_DATA && !str.is_empty()) {
			WARN_PRINT("Ignoring utf-8 parse error for string '" + str + "'...");
		} else {
			ERR_FAIL_V(ERR_INVALID_DATA);
		}
	}
	r_string = str;

	// Add padding
	strlen += pad;

	// Update buffer pos, left data count, and return size
	buf += strlen;
	len -= strlen;
	if (r_len) {
		(*r_len) += 4 + strlen;
	}

	return OK;
}

static void _encode_string(const String &p_string, uint8_t *&buf, int &r_len) {
	CharString utf8 = p_string.utf8();

	if (buf) {
		encode_uint32(utf8.length(), buf);
		buf += 4;
		memcpy(buf, utf8.get_data(), utf8.length());
		buf += utf8.length();
	}

	r_len += 4 + utf8.length();
	while (r_len % 4) {
		r_len++; //pad
		if (buf) {
			*(buf++) = 0;
		}
	}
}
