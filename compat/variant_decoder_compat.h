#pragma once

#include "core/string/ustring.h"
#include <core/variant/variant.h>

/* PLEASE NOTE: This class is only used for decoding variants contained in compiled GDScript files
   and binary project files. This is NOT to be used for resources.*/
class VariantDecoderCompat {
public:
	static String get_variant_type_name(int p_type, int ver_major);

	static int get_variant_type(String p_type_name, int ver_major);

	static Variant::Type convert_variant_type_from_old(int type, int ver_major);

	static int convert_variant_type_to_old(Variant::Type type, int ver_major);

	static Error decode_variant_compat(int ver_major, Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);

	static Error encode_variant_compat(int ver_major, const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects = false, bool p_real_t_is_double = false, int p_depth = 0);
};
