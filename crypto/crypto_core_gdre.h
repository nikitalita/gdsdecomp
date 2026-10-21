#pragma once

#include "core/typedefs.h"

class CryptoCoreGdre {
public:
	class CamelliaContext {
	public:
		enum class Mode {
			NONE,
			ENCRYPT,
			DECRYPT,
		};
		enum class Cipher {
			NONE,
			CBC,
			CFB,
			ECB,
		};

	private:
		uint32_t key_id = 0;
		uint32_t alg = 0;
		void *ctx = nullptr;

		void reset();

	public:
		CamelliaContext();
		~CamelliaContext();

		Error setup(Mode p_mode, Cipher p_cipher, const uint8_t *p_key, size_t p_key_length, const uint8_t *p_iv, size_t p_iv_size);
		Error update(const uint8_t *p_src, size_t p_src_size, uint8_t *r_dst, size_t p_dst_size);
		Error finish(uint8_t *r_dst, size_t p_dst_size);
	};

	class AriaContext {
	public:
		enum class Mode {
			NONE,
			ENCRYPT,
			DECRYPT,
		};
		enum class Cipher {
			NONE,
			CBC,
			CFB,
			ECB,
		};

	private:
		uint32_t key_id = 0;
		uint32_t alg = 0;
		void *ctx = nullptr;

		void reset();

	public:
		AriaContext();
		~AriaContext();

		Error setup(Mode p_mode, Cipher p_cipher, const uint8_t *p_key, size_t p_key_length, const uint8_t *p_iv, size_t p_iv_size);
		Error update(const uint8_t *p_src, size_t p_src_size, uint8_t *r_dst, size_t p_dst_size);
		Error finish(uint8_t *r_dst, size_t p_dst_size);
	};
};
