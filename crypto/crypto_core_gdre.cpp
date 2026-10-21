#include "crypto_core_gdre.h"

#include "core/os/memory.h"
// #define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
// #include <thirdparty/mbedtls/tf-psa-crypto/drivers/builtin/include/mbedtls/private/aria.h>
// #include <thirdparty/mbedtls/tf-psa-crypto/drivers/builtin/include/mbedtls/private/camellia.h>
// #undef MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
// AES256
CryptoCoreGdre::CamelliaContext::CamelliaContext() {
	// ctx = memalloc(sizeof(mbedtls_camellia_context));
	// mbedtls_camellia_init((mbedtls_camellia_context *)ctx);
}

CryptoCoreGdre::CamelliaContext::~CamelliaContext() {
	// mbedtls_camellia_free((mbedtls_camellia_context *)ctx);
	// memfree((mbedtls_camellia_context *)ctx);
}

Error CryptoCoreGdre::CamelliaContext::set_encode_key(const uint8_t *p_key, size_t p_bits) {
	// int ret = mbedtls_camellia_setkey_enc((mbedtls_camellia_context *)ctx, p_key, p_bits);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::CamelliaContext::set_decode_key(const uint8_t *p_key, size_t p_bits) {
	// int ret = mbedtls_camellia_setkey_dec((mbedtls_camellia_context *)ctx, p_key, p_bits);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::CamelliaContext::encrypt_ecb(const uint8_t p_src[16], uint8_t r_dst[16]) {
	// int ret = mbedtls_camellia_crypt_ecb((mbedtls_camellia_context *)ctx, MBEDTLS_CAMELLIA_ENCRYPT, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::CamelliaContext::encrypt_cbc(size_t p_length, uint8_t r_iv[16], const uint8_t *p_src, uint8_t *r_dst) {
	// int ret = mbedtls_camellia_crypt_cbc((mbedtls_camellia_context *)ctx, MBEDTLS_CAMELLIA_ENCRYPT, p_length, r_iv, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::CamelliaContext::encrypt_cfb(size_t p_length, uint8_t p_iv[16], const uint8_t *p_src, uint8_t *r_dst) {
	// size_t iv_off = 0; // Ignore and assume 16-byte alignment.
	// int ret = mbedtls_camellia_crypt_cfb128((mbedtls_camellia_context *)ctx, MBEDTLS_CAMELLIA_ENCRYPT, p_length, &iv_off, p_iv, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::CamelliaContext::decrypt_ecb(const uint8_t p_src[16], uint8_t r_dst[16]) {
	// int ret = mbedtls_camellia_crypt_ecb((mbedtls_camellia_context *)ctx, MBEDTLS_CAMELLIA_DECRYPT, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::CamelliaContext::decrypt_cbc(size_t p_length, uint8_t r_iv[16], const uint8_t *p_src, uint8_t *r_dst) {
	// int ret = mbedtls_camellia_crypt_cbc((mbedtls_camellia_context *)ctx, MBEDTLS_CAMELLIA_DECRYPT, p_length, r_iv, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::CamelliaContext::decrypt_cfb(size_t p_length, uint8_t p_iv[16], const uint8_t *p_src, uint8_t *r_dst) {
	// size_t iv_off = 0; // Ignore and assume 16-byte alignment.
	// int ret = mbedtls_camellia_crypt_cfb128((mbedtls_camellia_context *)ctx, MBEDTLS_CAMELLIA_DECRYPT, p_length, &iv_off, p_iv, p_src, r_dst);
	// return ret ? FAILED : OK;
}

// ARIA
CryptoCoreGdre::AriaContext::AriaContext() {
	// ctx = memalloc(sizeof(mbedtls_aria_context));
	// mbedtls_aria_init((mbedtls_aria_context *)ctx);
}

CryptoCoreGdre::AriaContext::~AriaContext() {
	// mbedtls_aria_free((mbedtls_aria_context *)ctx);
	// memfree((mbedtls_aria_context *)ctx);
}

Error CryptoCoreGdre::AriaContext::set_encode_key(const uint8_t *p_key, size_t p_bits) {
	// int ret = mbedtls_aria_setkey_enc((mbedtls_aria_context *)ctx, p_key, p_bits);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::AriaContext::set_decode_key(const uint8_t *p_key, size_t p_bits) {
	// int ret = mbedtls_aria_setkey_dec((mbedtls_aria_context *)ctx, p_key, p_bits);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::AriaContext::encrypt_ecb(const uint8_t p_src[16], uint8_t r_dst[16]) {
	// int ret = mbedtls_aria_crypt_ecb((mbedtls_aria_context *)ctx, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::AriaContext::encrypt_cbc(size_t p_length, uint8_t r_iv[16], const uint8_t *p_src, uint8_t *r_dst) {
	// int ret = mbedtls_aria_crypt_cbc((mbedtls_aria_context *)ctx, MBEDTLS_ARIA_ENCRYPT, p_length, r_iv, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::AriaContext::encrypt_cfb(size_t p_length, uint8_t p_iv[16], const uint8_t *p_src, uint8_t *r_dst) {
	// size_t iv_off = 0; // Ignore and assume 16-byte alignment.
	// int ret = mbedtls_aria_crypt_cfb128((mbedtls_aria_context *)ctx, MBEDTLS_ARIA_ENCRYPT, p_length, &iv_off, p_iv, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::AriaContext::decrypt_ecb(const uint8_t p_src[16], uint8_t r_dst[16]) {
	// int ret = mbedtls_aria_crypt_ecb((mbedtls_aria_context *)ctx, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::AriaContext::decrypt_cbc(size_t p_length, uint8_t r_iv[16], const uint8_t *p_src, uint8_t *r_dst) {
	// int ret = mbedtls_aria_crypt_cbc((mbedtls_aria_context *)ctx, MBEDTLS_ARIA_DECRYPT, p_length, r_iv, p_src, r_dst);
	// return ret ? FAILED : OK;
}

Error CryptoCoreGdre::AriaContext::decrypt_cfb(size_t p_length, uint8_t p_iv[16], const uint8_t *p_src, uint8_t *r_dst) {
	// size_t iv_off = 0; // Ignore and assume 16-byte alignment.
	// int ret = mbedtls_aria_crypt_cfb128((mbedtls_aria_context *)ctx, MBEDTLS_ARIA_DECRYPT, p_length, &iv_off, p_iv, p_src, r_dst);
	// return ret ? FAILED : OK;
}
