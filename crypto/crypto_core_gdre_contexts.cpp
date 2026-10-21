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

using AESMode = CryptoCore::AESContext::Mode;
using AESCipher = CryptoCore::AESContext::Cipher;
using CamelliaMode = CryptoCoreGdre::CamelliaContext::Mode;
using CamelliaCipher = CryptoCoreGdre::CamelliaContext::Cipher;
using AriaMode = CryptoCoreGdre::AriaContext::Mode;
using AriaCipher = CryptoCoreGdre::AriaContext::Cipher;

// AESContext with CFB support
Error AESContextGDRE::start(Mode p_mode, const PackedByteArray &p_key, const PackedByteArray &p_iv) {
	ERR_FAIL_COND_V_MSG(mode != MODE_MAX, ERR_ALREADY_IN_USE, "AESContext already started. Call 'finish' before starting a new one.");
	ERR_FAIL_COND_V_MSG(p_mode < 0 || p_mode >= MODE_MAX, ERR_INVALID_PARAMETER, "Invalid mode requested.");
	// Key check.
	int key_bits = p_key.size() << 3;
	ERR_FAIL_COND_V_MSG(key_bits != 128 && key_bits != 256, ERR_INVALID_PARAMETER, "AES key must be either 16 or 32 bytes");

	AESMode ctx_mode = AESMode::NONE;
	AESCipher ctx_cipher = AESCipher::NONE;
	switch (p_mode) {
		case MODE_ECB_ENCRYPT:
			ctx_mode = AESMode::ENCRYPT;
			ctx_cipher = AESCipher::ECB;
			break;
		case MODE_ECB_DECRYPT:
			ctx_mode = AESMode::DECRYPT;
			ctx_cipher = AESCipher::ECB;
			break;
		case MODE_CBC_ENCRYPT:
			ctx_mode = AESMode::ENCRYPT;
			ctx_cipher = AESCipher::CBC;
			break;
		case MODE_CBC_DECRYPT:
			ctx_mode = AESMode::DECRYPT;
			ctx_cipher = AESCipher::CBC;
			break;
		case MODE_CFB_ENCRYPT:
			ctx_mode = AESMode::ENCRYPT;
			ctx_cipher = AESCipher::CFB;
			break;
		case MODE_CFB_DECRYPT:
			ctx_mode = AESMode::DECRYPT;
			ctx_cipher = AESCipher::CFB;
			break;
		default:
			ERR_FAIL_V(ERR_INVALID_PARAMETER);
	}

	// Initialization vector.
	PackedByteArray iv = p_iv;
	ERR_FAIL_COND_V_MSG((ctx_cipher == AESCipher::CBC || ctx_cipher == AESCipher::CFB) && iv.size() != 16, ERR_INVALID_PARAMETER, "The initialization vector (IV) must be exactly 16 bytes.");
	if (unlikely(ctx_cipher == AESCipher::ECB && iv.size())) {
		WARN_PRINT("ECB mode does not require an initialization vector (IV). Pass an empty IV argument when using ECB.");
		iv.clear();
	}
	RETURN_IF_ERROR(ctx.setup(ctx_mode, ctx_cipher, p_key.ptr(), p_key.size(), iv.size() ? iv.ptr() : nullptr, iv.size()));
	mode = p_mode;
	return OK;
}

PackedByteArray AESContextGDRE::update(const PackedByteArray &p_src) {
	ERR_FAIL_COND_V_MSG(mode < 0 || mode >= MODE_MAX, PackedByteArray(), "AESContext not started. Call 'start' before calling 'update'.");
	int len = p_src.size();
	ERR_FAIL_COND_V_MSG(len % 16, PackedByteArray(), "The number of bytes to be encrypted must be multiple of 16. Add padding if needed");
	PackedByteArray out;
	out.resize(len);
	Error err = ctx.update(p_src.ptr(), len, out.ptrw(), len);
	ERR_FAIL_COND_V(err != OK, PackedByteArray());
	return out;
}

PackedByteArray AESContextGDRE::get_iv_state() {
	ERR_FAIL_V_MSG(PackedByteArray(), "Calling 'get_iv_state' is no longer supported.");
}

void AESContextGDRE::finish() {
	ctx.finish(nullptr, 0);
	mode = MODE_MAX;
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
	ERR_FAIL_COND_V_MSG(mode != MODE_MAX, ERR_ALREADY_IN_USE, "AESContext already started. Call 'finish' before starting a new one.");
	ERR_FAIL_COND_V_MSG(p_mode < 0 || p_mode >= MODE_MAX, ERR_INVALID_PARAMETER, "Invalid mode requested.");
	// Key check.
	int key_bits = p_key.size() << 3;
	ERR_FAIL_COND_V_MSG(key_bits != 128 && key_bits != 256, ERR_INVALID_PARAMETER, "AES key must be either 16 or 32 bytes");

	CamelliaMode ctx_mode = CamelliaMode::NONE;
	CamelliaCipher ctx_cipher = CamelliaCipher::NONE;
	switch (p_mode) {
		case MODE_ECB_ENCRYPT:
			ctx_mode = CamelliaMode::ENCRYPT;
			ctx_cipher = CamelliaCipher::ECB;
			break;
		case MODE_ECB_DECRYPT:
			ctx_mode = CamelliaMode::DECRYPT;
			ctx_cipher = CamelliaCipher::ECB;
			break;
		case MODE_CBC_ENCRYPT:
			ctx_mode = CamelliaMode::ENCRYPT;
			ctx_cipher = CamelliaCipher::CBC;
			break;
		case MODE_CBC_DECRYPT:
			ctx_mode = CamelliaMode::DECRYPT;
			ctx_cipher = CamelliaCipher::CBC;
			break;
		case MODE_CFB_ENCRYPT:
			ctx_mode = CamelliaMode::ENCRYPT;
			ctx_cipher = CamelliaCipher::CFB;
			break;
		case MODE_CFB_DECRYPT:
			ctx_mode = CamelliaMode::DECRYPT;
			ctx_cipher = CamelliaCipher::CFB;
			break;
		default:
			ERR_FAIL_V(ERR_INVALID_PARAMETER);
	}

	// Initialization vector.
	PackedByteArray iv = p_iv;
	ERR_FAIL_COND_V_MSG((ctx_cipher == CamelliaCipher::CBC || ctx_cipher == CamelliaCipher::CFB) && iv.size() != 16, ERR_INVALID_PARAMETER, "The initialization vector (IV) must be exactly 16 bytes.");
	if (unlikely(ctx_cipher == CamelliaCipher::ECB && iv.size())) {
		WARN_PRINT("ECB mode does not require an initialization vector (IV). Pass an empty IV argument when using ECB.");
		iv.clear();
	}
	RETURN_IF_ERROR(ctx.setup(ctx_mode, ctx_cipher, p_key.ptr(), p_key.size(), iv.size() ? iv.ptr() : nullptr, iv.size()));
	mode = p_mode;
	return OK;
}

PackedByteArray CamelliaContext::update(const PackedByteArray &p_src) {
	ERR_FAIL_COND_V_MSG(mode < 0 || mode >= MODE_MAX, PackedByteArray(), "AESContext not started. Call 'start' before calling 'update'.");
	int len = p_src.size();
	ERR_FAIL_COND_V_MSG(len % 16, PackedByteArray(), "The number of bytes to be encrypted must be multiple of 16. Add padding if needed");
	PackedByteArray out;
	out.resize(len);
	Error err = ctx.update(p_src.ptr(), len, out.ptrw(), len);
	ERR_FAIL_COND_V(err != OK, PackedByteArray());
	return out;
}

PackedByteArray CamelliaContext::get_iv_state() {
	ERR_FAIL_V_MSG(PackedByteArray(), "Calling 'get_iv_state' is no longer supported.");
}

void CamelliaContext::finish() {
	ctx.finish(nullptr, 0);
	mode = MODE_MAX;
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
	ERR_FAIL_COND_V_MSG(mode != MODE_MAX, ERR_ALREADY_IN_USE, "AESContext already started. Call 'finish' before starting a new one.");
	ERR_FAIL_COND_V_MSG(p_mode < 0 || p_mode >= MODE_MAX, ERR_INVALID_PARAMETER, "Invalid mode requested.");
	// Key check.
	int key_bits = p_key.size() << 3;
	ERR_FAIL_COND_V_MSG(key_bits != 128 && key_bits != 256, ERR_INVALID_PARAMETER, "AES key must be either 16 or 32 bytes");

	AriaMode ctx_mode = AriaMode::NONE;
	AriaCipher ctx_cipher = AriaCipher::NONE;
	switch (p_mode) {
		case MODE_ECB_ENCRYPT:
			ctx_mode = AriaMode::ENCRYPT;
			ctx_cipher = AriaCipher::ECB;
			break;
		case MODE_ECB_DECRYPT:
			ctx_mode = AriaMode::DECRYPT;
			ctx_cipher = AriaCipher::ECB;
			break;
		case MODE_CBC_ENCRYPT:
			ctx_mode = AriaMode::ENCRYPT;
			ctx_cipher = AriaCipher::CBC;
			break;
		case MODE_CBC_DECRYPT:
			ctx_mode = AriaMode::DECRYPT;
			ctx_cipher = AriaCipher::CBC;
			break;
		case MODE_CFB_ENCRYPT:
			ctx_mode = AriaMode::ENCRYPT;
			ctx_cipher = AriaCipher::CFB;
			break;
		case MODE_CFB_DECRYPT:
			ctx_mode = AriaMode::DECRYPT;
			ctx_cipher = AriaCipher::CFB;
			break;
		default:
			ERR_FAIL_V(ERR_INVALID_PARAMETER);
	}

	// Initialization vector.
	PackedByteArray iv = p_iv;
	ERR_FAIL_COND_V_MSG((ctx_cipher == AriaCipher::CBC || ctx_cipher == AriaCipher::CFB) && iv.size() != 16, ERR_INVALID_PARAMETER, "The initialization vector (IV) must be exactly 16 bytes.");
	if (unlikely(ctx_cipher == AriaCipher::ECB && iv.size())) {
		WARN_PRINT("ECB mode does not require an initialization vector (IV). Pass an empty IV argument when using ECB.");
		iv.clear();
	}
	RETURN_IF_ERROR(ctx.setup(ctx_mode, ctx_cipher, p_key.ptr(), p_key.size(), iv.size() ? iv.ptr() : nullptr, iv.size()));
	mode = p_mode;
	return OK;
}

PackedByteArray AriaContext::update(const PackedByteArray &p_src) {
	ERR_FAIL_COND_V_MSG(mode < 0 || mode >= MODE_MAX, PackedByteArray(), "AESContext not started. Call 'start' before calling 'update'.");
	int len = p_src.size();
	ERR_FAIL_COND_V_MSG(len % 16, PackedByteArray(), "The number of bytes to be encrypted must be multiple of 16. Add padding if needed");
	PackedByteArray out;
	out.resize(len);
	Error err = ctx.update(p_src.ptr(), len, out.ptrw(), len);
	ERR_FAIL_COND_V(err != OK, PackedByteArray());
	return out;
}

PackedByteArray AriaContext::get_iv_state() {
	ERR_FAIL_V_MSG(PackedByteArray(), "Calling 'get_iv_state' is no longer supported.");
}

void AriaContext::finish() {
	ctx.finish(nullptr, 0);
	mode = MODE_MAX;
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
