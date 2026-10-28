#include "variant_decoder_compat.h"
#include "compat/marshalls_compat/marshalls_2.h"
#include "compat/marshalls_compat/marshalls_3.h"
#include "compat/marshalls_compat/marshalls_4.h"
#include "core/error/error_list.h"
#include "core/version_generated.gen.h"

String VariantDecoderCompat::get_variant_type_name(int p_type, int ver_major) {
	if (ver_major <= 2) {
		return get_variant_type_name_v2(p_type);
	} else if (ver_major == 3) {
		return get_variant_type_name_v3(p_type);
	}
	return Variant::get_type_name((Variant::Type)p_type);
}

int VariantDecoderCompat::get_variant_type(String p_type_name, int ver_major) {
	if (ver_major <= 2) {
		return get_variant_type_v2(p_type_name);
	} else if (ver_major == 3) {
		return get_variant_type_v3(p_type_name);
	}
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		if (Variant::get_type_name((Variant::Type)i) == p_type_name) {
			return i;
		}
	}
	return -1;
}

Variant::Type VariantDecoderCompat::convert_variant_type_from_old(int type, int ver_major) {
	if (ver_major <= 2) {
		return variant_type_enum_v2_to_v4(type);
	} else if (ver_major == 3) {
		return variant_type_enum_v3_to_v4(type);
	} else if (ver_major == GODOT_VERSION_MAJOR) {
		return (Variant::Type)type;
	} else {
		WARN_PRINT("VariantDecoderCompat::convert_variant_type_from_old: ver_major is not 2 or 3, returning type as is.");
	}
	return (Variant::Type)type;
}

int VariantDecoderCompat::convert_variant_type_to_old(Variant::Type type, int ver_major) {
	// this just calls the other functions depending on the ver_major
	if (ver_major <= 2) {
		return variant_type_enum_v4_to_v2(type);
	} else if (ver_major == 3) {
		return variant_type_enum_v4_to_v3(type);
	} else if (ver_major == GODOT_VERSION_MAJOR) {
		return int(type);
	} else {
		WARN_PRINT("VariantDecoderCompat::convert_variant_type_to_old: ver_major is not 2-4, returning type as is.");
	}
	return int(type);
}

Error VariantDecoderCompat::decode_variant_compat(int ver_major, Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_objects) {
	if (ver_major <= 2) {
		return decode_variant_2(r_variant, p_buffer, p_len, r_len, p_allow_objects);
	} else if (ver_major == 3) {
		return decode_variant_3(r_variant, p_buffer, p_len, r_len, p_allow_objects);
	}
	return decode_variant_4(r_variant, p_buffer, p_len, r_len, p_allow_objects);
}

Error VariantDecoderCompat::encode_variant_compat(int ver_major, const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects, bool p_real_t_is_double, int p_depth) {
	if (ver_major <= 2) {
		return encode_variant_2(p_variant, r_buffer, r_len);
	} else if (ver_major == 3) {
		return encode_variant_3(p_variant, r_buffer, r_len, p_full_objects, p_depth);
	}
	return encode_variant_4(p_variant, r_buffer, r_len, p_full_objects, p_real_t_is_double, p_depth);
}
