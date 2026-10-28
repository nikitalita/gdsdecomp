#pragma once

#include "core/error/error_list.h"
#include "core/variant/variant.h"

Error decode_variant_4(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false, int p_depth = 0);
Error encode_variant_4(const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_allow_objects = false, bool p_real_t_is_double = false, int p_depth = 0);
