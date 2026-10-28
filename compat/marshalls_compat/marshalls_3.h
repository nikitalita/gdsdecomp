#pragma once

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

namespace V3Type {
enum Type {

	NIL,
	// atomic types
	BOOL,
	INT,
	REAL, // changed to FLOAT in v4
	STRING,
	// math types
	VECTOR2, // 5
	RECT2,
	VECTOR3,
	TRANSFORM2D,
	PLANE,
	QUAT, // 10 -- changed to QUATERION in v4
	AABB,
	BASIS,
	TRANSFORM, // changed to TRANSFORM3D in v4
	// misc types
	COLOR,
	NODE_PATH, // 15
	_RID, // renamed to RID in v4
	OBJECT,
	DICTIONARY,
	ARRAY,
	// arrays
	POOL_BYTE_ARRAY, // 20 -- renamed to PACKED_BYTE_ARRAY in v4
	POOL_INT_ARRAY, // renamed to PACKED_INT32_ARRAY in v4
	POOL_REAL_ARRAY, // renamed to PACKED_FLOAT32_ARRAY in v4
	POOL_STRING_ARRAY, // renamed to PACKED_STRING_ARRAY in v4
	POOL_VECTOR2_ARRAY, // renamed to PACKED_VECTOR2_ARRAY in v4
	POOL_VECTOR3_ARRAY, // 25 -- renamed to PACKED_VECTOR3_ARRAY in v4
	POOL_COLOR_ARRAY, // renamed to PACKED_COLOR_ARRAY in v4
	VARIANT_MAX // 27
};
}

String get_variant_type_name_v3(int p_type);
int get_variant_type_v3(String p_type_name);
Variant::Type variant_type_enum_v3_to_v4(int type);
int variant_type_enum_v4_to_v3(Variant::Type type);

Error decode_variant_3(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);
Error encode_variant_3(const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects = false, int p_depth = 0);
