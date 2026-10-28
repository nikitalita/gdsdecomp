#pragma once

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"

namespace V2Type {
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
	MATRIX32, // changed to TRANSFORM2D in v3, v4
	PLANE,
	QUAT, // 10 -- changed to QUATERION in v3, v4
	_AABB, //sorry naming convention fail :( not like it's used often -- renamed to AABB in v3, v4
	MATRIX3, // changed to BASIS in v3, v4
	TRANSFORM, // changed to TRANSFORM3D in v4

	// misc types
	COLOR,
	IMAGE, // 15 -- removed in v3, v4
	NODE_PATH,
	_RID, // renamed to RID in v4
	OBJECT,
	INPUT_EVENT, // removed in v3, v4
	DICTIONARY, // 20
	ARRAY,

	// arrays
	RAW_ARRAY, // renamed to POOL_BYTE_ARRAY in v3, PACKED_BYTE_ARRAY in v4
	INT_ARRAY, // renamed to POOL_INT_ARRAY in v3, PACKED_INT32_ARRAY in v4
	REAL_ARRAY, // renamed to POOL_REAL_ARRAY in v3, PACKED_FLOAT32_ARRAY in v4
	STRING_ARRAY, // 25 -- renamed to POOL_STRING_ARRAY in v3, PACKED_STRING_ARRAY in v4
	VECTOR2_ARRAY, // renamed to POOL_VECTOR2_ARRAY in v3, PACKED_VECTOR2_ARRAY in v4
	VECTOR3_ARRAY, // renamed to POOL_VECTOR3_ARRAY in v3, PACKED_VECTOR3_ARRAY in v4
	COLOR_ARRAY, // renamed to POOL_COLOR_ARRAY in v3, PACKED_COLOR_ARRAY in v4

	VARIANT_MAX // 29

};
}

String get_variant_type_name_v2(int p_type);
int get_variant_type_v2(String p_type_name);
Variant::Type variant_type_enum_v2_to_v4(int type);
int variant_type_enum_v4_to_v2(Variant::Type type);

Error decode_variant_2(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);
Error encode_variant_2(const Variant &p_variant, uint8_t *r_buffer, int &r_len);
