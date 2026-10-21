/**************************************************************************/
/*  crypto_core_gdre_contexts.cpp                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "crypto_core_gdre_contexts.h"

#include "core/object/class_db.h"

// AESContext with CFB support
Error AESContextGDRE::start(Mode p_mode, const PackedByteArray &p_key, const PackedByteArray &p_iv) {
	ERR_FAIL_COND_V_MSG(mode != MODE_MAX, ERR_ALREADY_IN_USE, "AESContextGDRE already started. Call 'finish' before starting a new one.");
	ERR_FAIL_COND_V_MSG(p_mode < 0 || p_mode >= MODE_MAX, ERR_INVALID_PARAMETER, "Invalid mode requested.");
	// Key check.
	int key_bits = p_key.size() << 3;
	ERR_FAIL_COND_V_MSG(key_bits != 128 && key_bits != 192 && key_bits != 256, ERR_INVALID_PARAMETER, "Camellia key must be either 16, 24, or 32 bytes");
	// Initialization vector.
	if (p_mode == MODE_CBC_ENCRYPT || p_mode == MODE_CBC_DECRYPT || p_mode == MODE_CFB_ENCRYPT || p_mode == MODE_CFB_DECRYPT) {
		ERR_FAIL_COND_V_MSG(p_iv.size() != 16, ERR_INVALID_PARAMETER, "The initialization vector (IV) must be exactly 16 bytes.");
		iv.resize(0);
		iv.append_array(p_iv);
	}
	// Encryption/decryption key.
	if (p_mode == MODE_CBC_ENCRYPT || p_mode == MODE_ECB_ENCRYPT || p_mode == MODE_CFB_ENCRYPT || p_mode == MODE_CFB_DECRYPT) {
		// ctx.set_encode_key(p_key.ptr(), key_bits);
	} else {
		// ctx.set_decode_key(p_key.ptr(), key_bits);
	}
	mode = p_mode;
	return OK;
}

PackedByteArray AESContextGDRE::update(const PackedByteArray &p_src) {
	ERR_FAIL_COND_V_MSG(mode < 0 || mode >= MODE_MAX, PackedByteArray(), "AESContextGDRE not started. Call 'start' before calling 'update'.");
	int len = p_src.size();
	ERR_FAIL_COND_V_MSG(len % 16, PackedByteArray(), "The number of bytes to be encrypted must be multiple of 16. Add padding if needed");
	PackedByteArray out;
	out.resize(len);
	const uint8_t *src_ptr = p_src.ptr();
	uint8_t *out_ptr = out.ptrw();
	// switch (mode) {
	// 	case MODE_ECB_ENCRYPT: {
	// 		for (int i = 0; i < len; i += 16) {
	// 			Error err = ctx.encrypt_ecb(src_ptr + i, out_ptr + i);
	// 			ERR_FAIL_COND_V(err != OK, PackedByteArray());
	// 		}
	// 	} break;
	// 	case MODE_ECB_DECRYPT: {
	// 		for (int i = 0; i < len; i += 16) {
	// 			Error err = ctx.decrypt_ecb(src_ptr + i, out_ptr + i);
	// 			ERR_FAIL_COND_V(err != OK, PackedByteArray());
	// 		}
	// 	} break;
	// 	case MODE_CBC_ENCRYPT: {
	// 		Error err = ctx.encrypt_cbc(len, iv.ptrw(), p_src.ptr(), out.ptrw());
	// 		ERR_FAIL_COND_V(err != OK, PackedByteArray());
	// 	} break;
	// 	case MODE_CBC_DECRYPT: {
	// 		Error err = ctx.decrypt_cbc(len, iv.ptrw(), p_src.ptr(), out.ptrw());
	// 		ERR_FAIL_COND_V(err != OK, PackedByteArray());
	// 	} break;
	// 	case MODE_CFB_ENCRYPT: {
	// 		Error err = ctx.encrypt_cfb(len, iv.ptrw(), p_src.ptr(), out.ptrw());
	// 		ERR_FAIL_COND_V(err != OK, PackedByteArray());
	// 	} break;
	// 	case MODE_CFB_DECRYPT: {
	// 		Error err = ctx.decrypt_cfb(len, iv.ptrw(), p_src.ptr(), out.ptrw());
	// 		ERR_FAIL_COND_V(err != OK, PackedByteArray());
	// 	} break;
	// 	default:
	// 		ERR_FAIL_V_MSG(PackedByteArray(), "Bug!");
	// }
	return out;
}

PackedByteArray AESContextGDRE::get_iv_state() {
	ERR_FAIL_COND_V_MSG(mode != MODE_CBC_ENCRYPT && mode != MODE_CBC_DECRYPT && mode != MODE_CFB_ENCRYPT && mode != MODE_CFB_DECRYPT, PackedByteArray(), "Calling 'get_iv_state' only makes sense when the context is started in CBC or CFB mode.");
	PackedByteArray out;
	out.append_array(iv);
	return out;
}

void AESContextGDRE::finish() {
	mode = MODE_MAX;
	iv.resize(0);
}

void AESContextGDRE::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "mode", "key", "iv"), &AESContextGDRE::start, DEFVAL(PackedByteArray()));
	ClassDB::bind_method(D_METHOD("update", "src"), &AESContextGDRE::update);
	ClassDB::bind_method(D_METHOD("get_iv_state"), &AESContextGDRE::get_iv_state);
	ClassDB::bind_method(D_METHOD("finish"), &AESContextGDRE::finish);
	BIND_ENUM_CONSTANT(MODE_ECB_ENCRYPT);
	BIND_ENUM_CONSTANT(MODE_ECB_DECRYPT);
	BIND_ENUM_CONSTANT(MODE_CBC_ENCRYPT);
	BIND_ENUM_CONSTANT(MODE_CBC_DECRYPT);
	BIND_ENUM_CONSTANT(MODE_CFB_ENCRYPT);
	BIND_ENUM_CONSTANT(MODE_CFB_DECRYPT);
	BIND_ENUM_CONSTANT(MODE_MAX);
}

Error CamelliaContext::start(Mode p_mode, const PackedByteArray &p_key, const PackedByteArray &p_iv) {
	ERR_FAIL_COND_V_MSG(mode != MODE_MAX, ERR_ALREADY_IN_USE, "CamelliaContext already started. Call 'finish' before starting a new one.");
	ERR_FAIL_COND_V_MSG(p_mode < 0 || p_mode >= MODE_MAX, ERR_INVALID_PARAMETER, "Invalid mode requested.");
	// Key check.
	int key_bits = p_key.size() << 3;
	ERR_FAIL_COND_V_MSG(key_bits != 128 && key_bits != 192 && key_bits != 256, ERR_INVALID_PARAMETER, "Camellia key must be either 16, 24, or 32 bytes");
	// Initialization vector.
	if (p_mode == MODE_CBC_ENCRYPT || p_mode == MODE_CBC_DECRYPT || p_mode == MODE_CFB_ENCRYPT || p_mode == MODE_CFB_DECRYPT) {
		ERR_FAIL_COND_V_MSG(p_iv.size() != 16, ERR_INVALID_PARAMETER, "The initialization vector (IV) must be exactly 16 bytes.");
		iv.resize(0);
		iv.append_array(p_iv);
	}
	// Encryption/decryption key.
	if (p_mode == MODE_CBC_ENCRYPT || p_mode == MODE_ECB_ENCRYPT || p_mode == MODE_CFB_ENCRYPT || p_mode == MODE_CFB_DECRYPT) {
		ctx.set_encode_key(p_key.ptr(), key_bits);
	} else {
		ctx.set_decode_key(p_key.ptr(), key_bits);
	}
	mode = p_mode;
	return OK;
}

PackedByteArray CamelliaContext::update(const PackedByteArray &p_src) {
	ERR_FAIL_COND_V_MSG(mode < 0 || mode >= MODE_MAX, PackedByteArray(), "CamelliaContext not started. Call 'start' before calling 'update'.");
	int len = p_src.size();
	ERR_FAIL_COND_V_MSG(len % 16, PackedByteArray(), "The number of bytes to be encrypted must be multiple of 16. Add padding if needed");
	PackedByteArray out;
	out.resize(len);
	const uint8_t *src_ptr = p_src.ptr();
	uint8_t *out_ptr = out.ptrw();
	switch (mode) {
		case MODE_ECB_ENCRYPT: {
			for (int i = 0; i < len; i += 16) {
				Error err = ctx.encrypt_ecb(src_ptr + i, out_ptr + i);
				ERR_FAIL_COND_V(err != OK, PackedByteArray());
			}
		} break;
		case MODE_ECB_DECRYPT: {
			for (int i = 0; i < len; i += 16) {
				Error err = ctx.decrypt_ecb(src_ptr + i, out_ptr + i);
				ERR_FAIL_COND_V(err != OK, PackedByteArray());
			}
		} break;
		case MODE_CBC_ENCRYPT: {
			Error err = ctx.encrypt_cbc(len, iv.ptrw(), p_src.ptr(), out.ptrw());
			ERR_FAIL_COND_V(err != OK, PackedByteArray());
		} break;
		case MODE_CBC_DECRYPT: {
			Error err = ctx.decrypt_cbc(len, iv.ptrw(), p_src.ptr(), out.ptrw());
			ERR_FAIL_COND_V(err != OK, PackedByteArray());
		} break;
		case MODE_CFB_ENCRYPT: {
			Error err = ctx.encrypt_cfb(len, iv.ptrw(), p_src.ptr(), out.ptrw());
			ERR_FAIL_COND_V(err != OK, PackedByteArray());
		} break;
		case MODE_CFB_DECRYPT: {
			Error err = ctx.decrypt_cfb(len, iv.ptrw(), p_src.ptr(), out.ptrw());
			ERR_FAIL_COND_V(err != OK, PackedByteArray());
		} break;
		default:
			ERR_FAIL_V_MSG(PackedByteArray(), "Bug!");
	}
	return out;
}

PackedByteArray CamelliaContext::get_iv_state() {
	ERR_FAIL_COND_V_MSG(mode != MODE_CBC_ENCRYPT && mode != MODE_CBC_DECRYPT && mode != MODE_CFB_ENCRYPT && mode != MODE_CFB_DECRYPT, PackedByteArray(), "Calling 'get_iv_state' only makes sense when the context is started in CBC or CFB mode.");
	PackedByteArray out;
	out.append_array(iv);
	return out;
}

void CamelliaContext::finish() {
	mode = MODE_MAX;
	iv.resize(0);
}

void CamelliaContext::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "mode", "key", "iv"), &CamelliaContext::start, DEFVAL(PackedByteArray()));
	ClassDB::bind_method(D_METHOD("update", "src"), &CamelliaContext::update);
	ClassDB::bind_method(D_METHOD("get_iv_state"), &CamelliaContext::get_iv_state);
	ClassDB::bind_method(D_METHOD("finish"), &CamelliaContext::finish);
	BIND_ENUM_CONSTANT(MODE_ECB_ENCRYPT);
	BIND_ENUM_CONSTANT(MODE_ECB_DECRYPT);
	BIND_ENUM_CONSTANT(MODE_CBC_ENCRYPT);
	BIND_ENUM_CONSTANT(MODE_CBC_DECRYPT);
	BIND_ENUM_CONSTANT(MODE_CFB_ENCRYPT);
	BIND_ENUM_CONSTANT(MODE_CFB_DECRYPT);
	BIND_ENUM_CONSTANT(MODE_MAX);
}

// ARIA
Error AriaContext::start(Mode p_mode, const PackedByteArray &p_key, const PackedByteArray &p_iv) {
	ERR_FAIL_COND_V_MSG(mode != MODE_MAX, ERR_ALREADY_IN_USE, "AriaContext already started. Call 'finish' before starting a new one.");
	ERR_FAIL_COND_V_MSG(p_mode < 0 || p_mode >= MODE_MAX, ERR_INVALID_PARAMETER, "Invalid mode requested.");
	// Key check.
	int key_bits = p_key.size() << 3;
	ERR_FAIL_COND_V_MSG(key_bits != 128 && key_bits != 192 && key_bits != 256, ERR_INVALID_PARAMETER, "ARIA key must be 16, 24, or 32 bytes (128, 192, or 256 bits)");
	// Initialization vector.
	if (p_mode == MODE_CBC_ENCRYPT || p_mode == MODE_CBC_DECRYPT || p_mode == MODE_CFB_ENCRYPT || p_mode == MODE_CFB_DECRYPT) {
		ERR_FAIL_COND_V_MSG(p_iv.size() != 16, ERR_INVALID_PARAMETER, "The initialization vector (IV) must be exactly 16 bytes.");
		iv.resize(0);
		iv.append_array(p_iv);
	}
	// Encryption/decryption key.
	if (p_mode == MODE_CBC_ENCRYPT || p_mode == MODE_ECB_ENCRYPT || p_mode == MODE_CFB_ENCRYPT || p_mode == MODE_CFB_DECRYPT) {
		ctx.set_encode_key(p_key.ptr(), key_bits);
	} else {
		ctx.set_decode_key(p_key.ptr(), key_bits);
	}
	mode = p_mode;
	return OK;
}

PackedByteArray AriaContext::update(const PackedByteArray &p_src) {
	ERR_FAIL_COND_V_MSG(mode < 0 || mode >= MODE_MAX, PackedByteArray(), "AriaContext not started. Call 'start' before calling 'update'.");
	int len = p_src.size();
	PackedByteArray out;
	out.resize(len);
	const uint8_t *src_ptr = p_src.ptr();
	uint8_t *out_ptr = out.ptrw();
	switch (mode) {
		case MODE_ECB_ENCRYPT: {
			ERR_FAIL_COND_V_MSG(len % 16, PackedByteArray(), "The number of bytes to be encrypted must be multiple of 16. Add padding if needed");
			for (int i = 0; i < len; i += 16) {
				Error err = ctx.encrypt_ecb(src_ptr + i, out_ptr + i);
				ERR_FAIL_COND_V(err != OK, PackedByteArray());
			}
		} break;
		case MODE_ECB_DECRYPT: {
			ERR_FAIL_COND_V_MSG(len % 16, PackedByteArray(), "The number of bytes to be decrypted must be multiple of 16. Add padding if needed");
			for (int i = 0; i < len; i += 16) {
				Error err = ctx.decrypt_ecb(src_ptr + i, out_ptr + i);
				ERR_FAIL_COND_V(err != OK, PackedByteArray());
			}
		} break;
		case MODE_CBC_ENCRYPT: {
			ERR_FAIL_COND_V_MSG(len % 16, PackedByteArray(), "The number of bytes to be encrypted must be multiple of 16. Add padding if needed");
			Error err = ctx.encrypt_cbc(len, iv.ptrw(), p_src.ptr(), out.ptrw());
			ERR_FAIL_COND_V(err != OK, PackedByteArray());
		} break;
		case MODE_CBC_DECRYPT: {
			ERR_FAIL_COND_V_MSG(len % 16, PackedByteArray(), "The number of bytes to be decrypted must be multiple of 16. Add padding if needed");
			Error err = ctx.decrypt_cbc(len, iv.ptrw(), p_src.ptr(), out.ptrw());
			ERR_FAIL_COND_V(err != OK, PackedByteArray());
		} break;
		case MODE_CFB_ENCRYPT: {
			Error err = ctx.encrypt_cfb(len, iv.ptrw(), p_src.ptr(), out.ptrw());
			ERR_FAIL_COND_V(err != OK, PackedByteArray());
		} break;
		case MODE_CFB_DECRYPT: {
			Error err = ctx.decrypt_cfb(len, iv.ptrw(), p_src.ptr(), out.ptrw());
			ERR_FAIL_COND_V(err != OK, PackedByteArray());
		} break;
		default:
			ERR_FAIL_V_MSG(PackedByteArray(), "Bug!");
	}
	return out;
}

PackedByteArray AriaContext::get_iv_state() {
	ERR_FAIL_COND_V_MSG(mode != MODE_CBC_ENCRYPT && mode != MODE_CBC_DECRYPT && mode != MODE_CFB_ENCRYPT && mode != MODE_CFB_DECRYPT, PackedByteArray(), "Calling 'get_iv_state' only makes sense when the context is started in CBC or CFB mode.");
	PackedByteArray out;
	out.append_array(iv);
	return out;
}

void AriaContext::finish() {
	mode = MODE_MAX;
	iv.resize(0);
}

void AriaContext::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start", "mode", "key", "iv"), &AriaContext::start, DEFVAL(PackedByteArray()));
	ClassDB::bind_method(D_METHOD("update", "src"), &AriaContext::update);
	ClassDB::bind_method(D_METHOD("get_iv_state"), &AriaContext::get_iv_state);
	ClassDB::bind_method(D_METHOD("finish"), &AriaContext::finish);
	BIND_ENUM_CONSTANT(MODE_ECB_ENCRYPT);
	BIND_ENUM_CONSTANT(MODE_ECB_DECRYPT);
	BIND_ENUM_CONSTANT(MODE_CBC_ENCRYPT);
	BIND_ENUM_CONSTANT(MODE_CBC_DECRYPT);
	BIND_ENUM_CONSTANT(MODE_CFB_ENCRYPT);
	BIND_ENUM_CONSTANT(MODE_CFB_DECRYPT);
	BIND_ENUM_CONSTANT(MODE_MAX);
}
