#include "crypto_core_gdre.h"

#include "core/os/memory.h"
#include <mbedtls/base64.h>
#include <psa/crypto.h>

// Camellia
CryptoCoreGdre::CamelliaContext::CamelliaContext() {
	ctx = memalloc_zeroed(sizeof(psa_cipher_operation_t));
}

CryptoCoreGdre::CamelliaContext::~CamelliaContext() {
	psa_cipher_abort((psa_cipher_operation_t *)ctx);
	psa_destroy_key(key_id);
	memfree((psa_cipher_operation_t *)ctx);
}

void CryptoCoreGdre::CamelliaContext::reset() {
	psa_cipher_abort((psa_cipher_operation_t *)ctx);
	psa_destroy_key(key_id);
	key_id = PSA_KEY_ID_NULL;
	alg = PSA_ALG_NONE;
}

Error CryptoCoreGdre::CamelliaContext::setup(Mode p_mode, Cipher p_cipher, const uint8_t *p_key, size_t p_key_length, const uint8_t *p_iv, size_t p_iv_size) {
	reset();
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	switch (p_mode) {
		case Mode::NONE:
			return OK;
		case Mode::ENCRYPT:
		case Mode::DECRYPT:
			break;
		default:
			return ERR_INVALID_PARAMETER;
	}
	switch (p_cipher) {
		case Cipher::CBC:
			alg = PSA_ALG_CBC_NO_PADDING;
			break;
		case Cipher::ECB:
			alg = PSA_ALG_ECB_NO_PADDING;
			break;
		case Cipher::CFB:
			alg = PSA_ALG_CFB;
			break;
		default:
			return ERR_INVALID_PARAMETER;
	}
	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_algorithm(&attr, alg);
	psa_set_key_type(&attr, PSA_KEY_TYPE_CAMELLIA);
	psa_set_key_bits(&attr, p_key_length << 3);
	int ret = psa_import_key(&attr, p_key, p_key_length, &key_id);
	if (ret) {
		return FAILED;
	}
	if (p_mode == Mode::ENCRYPT) {
		ret = psa_cipher_encrypt_setup((psa_cipher_operation_t *)ctx, key_id, alg);
	} else {
		ret = psa_cipher_decrypt_setup((psa_cipher_operation_t *)ctx, key_id, alg);
	}
	if (ret == PSA_SUCCESS && p_iv_size) {
		ret = psa_cipher_set_iv((psa_cipher_operation_t *)ctx, p_iv, p_iv_size);
	}
	if (ret) {
		reset();
		return FAILED;
	}
	return OK;
}

Error CryptoCoreGdre::CamelliaContext::update(const uint8_t *p_src, size_t p_src_size, uint8_t *r_dst, size_t p_dst_size) {
	size_t size = 0;
	int ret = psa_cipher_update((psa_cipher_operation_t *)ctx, p_src, p_src_size, r_dst, p_dst_size, &size);
	if (ret) {
		return FAILED;
	}
	ERR_FAIL_COND_V(p_dst_size != size, ERR_BUG);
	return OK;
}

Error CryptoCoreGdre::CamelliaContext::finish(uint8_t *r_dst, size_t p_dst_size) {
	size_t size = 0;
	int ret = psa_cipher_finish((psa_cipher_operation_t *)ctx, r_dst, p_dst_size, &size);
	reset();
	if (ret) {
		return FAILED;
	}
	ERR_FAIL_COND_V(p_dst_size != size, ERR_BUG);
	return OK;
}

// Aria
CryptoCoreGdre::AriaContext::AriaContext() {
	ctx = memalloc_zeroed(sizeof(psa_cipher_operation_t));
}

CryptoCoreGdre::AriaContext::~AriaContext() {
	psa_cipher_abort((psa_cipher_operation_t *)ctx);
	psa_destroy_key(key_id);
	memfree((psa_cipher_operation_t *)ctx);
}

void CryptoCoreGdre::AriaContext::reset() {
	psa_cipher_abort((psa_cipher_operation_t *)ctx);
	psa_destroy_key(key_id);
	key_id = PSA_KEY_ID_NULL;
	alg = PSA_ALG_NONE;
}

Error CryptoCoreGdre::AriaContext::setup(Mode p_mode, Cipher p_cipher, const uint8_t *p_key, size_t p_key_length, const uint8_t *p_iv, size_t p_iv_size) {
	reset();
	psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
	switch (p_mode) {
		case Mode::NONE:
			return OK;
		case Mode::ENCRYPT:
		case Mode::DECRYPT:
			break;
		default:
			return ERR_INVALID_PARAMETER;
	}
	switch (p_cipher) {
		case Cipher::CBC:
			alg = PSA_ALG_CBC_NO_PADDING;
			break;
		case Cipher::ECB:
			alg = PSA_ALG_ECB_NO_PADDING;
			break;
		case Cipher::CFB:
			alg = PSA_ALG_CFB;
			break;
		default:
			return ERR_INVALID_PARAMETER;
	}
	psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_algorithm(&attr, alg);
	psa_set_key_type(&attr, PSA_KEY_TYPE_ARIA);
	psa_set_key_bits(&attr, p_key_length << 3);
	int ret = psa_import_key(&attr, p_key, p_key_length, &key_id);
	if (ret) {
		return FAILED;
	}
	if (p_mode == Mode::ENCRYPT) {
		ret = psa_cipher_encrypt_setup((psa_cipher_operation_t *)ctx, key_id, alg);
	} else {
		ret = psa_cipher_decrypt_setup((psa_cipher_operation_t *)ctx, key_id, alg);
	}
	if (ret == PSA_SUCCESS && p_iv_size) {
		ret = psa_cipher_set_iv((psa_cipher_operation_t *)ctx, p_iv, p_iv_size);
	}
	if (ret) {
		reset();
		return FAILED;
	}
	return OK;
}

Error CryptoCoreGdre::AriaContext::update(const uint8_t *p_src, size_t p_src_size, uint8_t *r_dst, size_t p_dst_size) {
	size_t size = 0;
	int ret = psa_cipher_update((psa_cipher_operation_t *)ctx, p_src, p_src_size, r_dst, p_dst_size, &size);
	if (ret) {
		return FAILED;
	}
	ERR_FAIL_COND_V(p_dst_size != size, ERR_BUG);
	return OK;
}

Error CryptoCoreGdre::AriaContext::finish(uint8_t *r_dst, size_t p_dst_size) {
	size_t size = 0;
	int ret = psa_cipher_finish((psa_cipher_operation_t *)ctx, r_dst, p_dst_size, &size);
	reset();
	if (ret) {
		return FAILED;
	}
	ERR_FAIL_COND_V(p_dst_size != size, ERR_BUG);
	return OK;
}
