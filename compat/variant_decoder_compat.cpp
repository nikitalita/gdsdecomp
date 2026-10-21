#include "variant_decoder_compat.h"
#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "core/version_generated.gen.h"
#include "input_event_parser_v2.h"

#include "compat/image_enum_compat.h"
#include "compat/image_parser_v2.h"
#include "core/io/image.h"
#include "core/io/marshalls.h"
#include "core/object/class_db.h"
#include "core/variant/array.h"
#include "core/variant/container_type_validate.h"

#define _S(a) ((int32_t)a)
#define ERR_FAIL_ADD_OF(a, b, err) ERR_FAIL_COND_V(_S(b) < 0 || _S(a) < 0 || _S(a) > INT_MAX - _S(b), err)
#define ERR_FAIL_MUL_OF(a, b, err) ERR_FAIL_COND_V(_S(a) < 0 || _S(b) <= 0 || _S(a) > INT_MAX / _S(b), err)

#define ENCODE_MASK 0xFF
#define ENCODE_FLAG_64 1 << 16
#define ENCODE_FLAG_OBJECT_AS_ID 1 << 16

enum ContainerTypeKind {
	CONTAINER_TYPE_KIND_NONE = 0b00,
	CONTAINER_TYPE_KIND_BUILTIN = 0b01,
	CONTAINER_TYPE_KIND_CLASS_NAME = 0b10,
	CONTAINER_TYPE_KIND_SCRIPT = 0b11,
};

// Byte 0: `Variant::Type`, byte 1: unused, bytes 2 and 3: additional data.
#define HEADER_TYPE_MASK 0xFF

// For `Variant::INT`, `Variant::FLOAT` and other math types.
#define HEADER_DATA_FLAG_64 (1 << 16)

// For `Variant::OBJECT`.
#define HEADER_DATA_FLAG_OBJECT_AS_ID (1 << 16)

// For `Variant::ARRAY`.
// Occupies bits 16 and 17.
#define HEADER_DATA_FIELD_TYPED_ARRAY_MASK (0b11 << 16)
#define HEADER_DATA_FIELD_TYPED_ARRAY_SHIFT 16

// For `Variant::DICTIONARY`.
// Occupies bits 16 and 17.
#define HEADER_DATA_FIELD_TYPED_DICTIONARY_KEY_MASK (0b11 << 16)
#define HEADER_DATA_FIELD_TYPED_DICTIONARY_KEY_SHIFT 16
// Occupies bits 18 and 19.
#define HEADER_DATA_FIELD_TYPED_DICTIONARY_VALUE_MASK (0b11 << 18)
#define HEADER_DATA_FIELD_TYPED_DICTIONARY_VALUE_SHIFT 18

#define GET_CONTAINER_TYPE_KIND(m_header, m_field) \
	((ContainerTypeKind)(((m_header) & HEADER_DATA_FIELD_##m_field##_MASK) >> HEADER_DATA_FIELD_##m_field##_SHIFT))

static Error _decode_string(const uint8_t *&buf, int &len, int *r_len, String &r_string) {
	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	int32_t strlen = decode_uint32(buf);
	int32_t pad = 0;

	// Handle padding
	if (strlen % 4) {
		pad = 4 - strlen % 4;
	}

	buf += 4;
	len -= 4;

	// Ensure buffer is big enough
	ERR_FAIL_ADD_OF(strlen, pad, ERR_FILE_EOF);
	ERR_FAIL_COND_V(strlen < 0 || strlen + pad > len, ERR_FILE_EOF);

	String str;
	// COMPAT: certain hacked Godot games have invalid utf-8 strings that parse valid on their version of Godot, so we need to handle them.
	Error err = str.append_utf8((const char *)buf, strlen);
	if (err) {
		// ERR_INVALID_DATA means that certain characters are not valid UTF-8 and were replaced with 0xFFFD; we can continue on after this error.
		if (err == ERR_INVALID_DATA && !str.is_empty()) {
			WARN_PRINT("Ignoring utf-8 parse error for string '" + str + "'...");
		} else {
			ERR_FAIL_V(ERR_INVALID_DATA);
		}
	}
	r_string = str;

	// Add padding
	strlen += pad;

	// Update buffer pos, left data count, and return size
	buf += strlen;
	len -= strlen;
	if (r_len) {
		(*r_len) += 4 + strlen;
	}

	return OK;
}

struct V3TypeToString {
	V3Type::Type type;
	const char *name;
};

// create a list of V3TypeToString structs for each type
static const V3TypeToString v3_type_to_string[]{
	{ V3Type::NIL, "Nil" },
	{ V3Type::BOOL, "bool" },
	{ V3Type::INT, "int" },
	{ V3Type::REAL, "float" },
	{ V3Type::STRING, "String" },
	{ V3Type::VECTOR2, "Vector2" },
	{ V3Type::RECT2, "Rect2" },
	{ V3Type::VECTOR3, "Vector3" },
	{ V3Type::TRANSFORM2D, "Transform2D" },
	{ V3Type::PLANE, "Plane" },
	{ V3Type::QUAT, "Quat" },
	{ V3Type::AABB, "AABB" },
	{ V3Type::BASIS, "Basis" },
	{ V3Type::TRANSFORM, "Transform" },
	{ V3Type::COLOR, "Color" },
	{ V3Type::NODE_PATH, "NodePath" },
	{ V3Type::_RID, "RID" },
	{ V3Type::OBJECT, "Object" },
	{ V3Type::DICTIONARY, "Dictionary" },
	{ V3Type::ARRAY, "Array" },
	{ V3Type::POOL_BYTE_ARRAY, "PoolByteArray" },
	{ V3Type::POOL_INT_ARRAY, "PoolIntArray" },
	{ V3Type::POOL_REAL_ARRAY, "PoolRealArray" },
	{ V3Type::POOL_STRING_ARRAY, "PoolStringArray" },
	{ V3Type::POOL_VECTOR2_ARRAY, "PoolVector2Array" },
	{ V3Type::POOL_VECTOR3_ARRAY, "PoolVector3Array" },
	{ V3Type::POOL_COLOR_ARRAY, "PoolColorArray" }
};

static constexpr int v3_type_to_string_size = sizeof(v3_type_to_string) / sizeof(V3TypeToString);
static_assert(v3_type_to_string_size == V3Type::VARIANT_MAX, "v3_type_to_string_size must be equal to V3Type::VARIANT_MAX");

String VariantDecoderCompat::get_variant_type_name_v3(int p_type) {
	// type is an index into the v3_type_to_string array
	if (p_type >= 0 && p_type < v3_type_to_string_size) {
		return v3_type_to_string[p_type].name;
	}
	return "";
}

struct V2TypeToString {
	V2Type::Type type;
	const char *name;
};
// create a list of V2TypeToString structs for each type
static const V2TypeToString v2_type_to_string[] = {
	{ V2Type::NIL, "Nil" },
	{ V2Type::BOOL, "bool" },
	{ V2Type::INT, "int" },
	{ V2Type::REAL, "float" },
	{ V2Type::STRING, "String" },
	{ V2Type::VECTOR2, "Vector2" },
	{ V2Type::RECT2, "Rect2" },
	{ V2Type::VECTOR3, "Vector3" },
	{ V2Type::MATRIX32, "Matrix32" },
	{ V2Type::PLANE, "Plane" },
	{ V2Type::QUAT, "Quat" },
	{ V2Type::_AABB, "AABB" },
	{ V2Type::MATRIX3, "Matrix3" },
	{ V2Type::TRANSFORM, "Transform" },
	{ V2Type::COLOR, "Color" },
	{ V2Type::IMAGE, "Image" },
	{ V2Type::NODE_PATH, "NodePath" },
	{ V2Type::_RID, "RID" },
	{ V2Type::OBJECT, "Object" },
	{ V2Type::INPUT_EVENT, "InputEvent" },
	{ V2Type::DICTIONARY, "Dictionary" },
	{ V2Type::ARRAY, "Array" },
	{ V2Type::RAW_ARRAY, "RawArray" },
	{ V2Type::INT_ARRAY, "IntArray" },
	{ V2Type::REAL_ARRAY, "FloatArray" }, // The actual name in v2 is "RealArray", but that's not what's written to the resources and scripts
	{ V2Type::STRING_ARRAY, "StringArray" },
	{ V2Type::VECTOR2_ARRAY, "Vector2Array" },
	{ V2Type::VECTOR3_ARRAY, "Vector3Array" },
	{ V2Type::COLOR_ARRAY, "ColorArray" }
};
static constexpr int v2_type_to_string_size = sizeof(v2_type_to_string) / sizeof(V2TypeToString);
static_assert(v2_type_to_string_size == V2Type::VARIANT_MAX, "v2_type_to_string_size must be equal to V2Type::VARIANT_MAX");

String VariantDecoderCompat::get_variant_type_name_v2(int p_type) {
	// type is an index into the v2_type_to_string array
	if (p_type >= 0 && p_type < v2_type_to_string_size) {
		return v2_type_to_string[p_type].name;
	}
	return "";
}
String VariantDecoderCompat::get_variant_type_name(int p_type, int ver_major) {
	if (ver_major <= 2) {
		return get_variant_type_name_v2(p_type);
	} else if (ver_major == 3) {
		return get_variant_type_name_v3(p_type);
	}
	return Variant::get_type_name((Variant::Type)p_type);
}

int VariantDecoderCompat::get_variant_type_v3(String p_type_name) {
	for (int i = 0; i < v3_type_to_string_size; i++) {
		if (v3_type_to_string[i].name == p_type_name) {
			return v3_type_to_string[i].type;
		}
	}
	return -1;
}

int VariantDecoderCompat::get_variant_type_v2(String p_type_name) {
	for (int i = 0; i < v2_type_to_string_size; i++) {
		if (v2_type_to_string[i].name == p_type_name) {
			return v2_type_to_string[i].type;
		}
	}
	return -1;
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

// take the above function and create an array of the types
struct V2TypeToV4Type {
	V2Type::Type type;
	Variant::Type v4_type;
};
static constexpr V2TypeToV4Type v2_to_v4_type[] = {
	{ V2Type::NIL, Variant::NIL },
	{ V2Type::BOOL, Variant::BOOL },
	{ V2Type::INT, Variant::INT },
	{ V2Type::REAL, Variant::FLOAT },
	{ V2Type::STRING, Variant::STRING },
	{ V2Type::VECTOR2, Variant::VECTOR2 },
	{ V2Type::RECT2, Variant::RECT2 },
	{ V2Type::VECTOR3, Variant::VECTOR3 },
	{ V2Type::MATRIX32, Variant::TRANSFORM2D },
	{ V2Type::PLANE, Variant::PLANE },
	{ V2Type::QUAT, Variant::QUATERNION },
	{ V2Type::_AABB, Variant::AABB },
	{ V2Type::MATRIX3, Variant::BASIS },
	{ V2Type::TRANSFORM, Variant::TRANSFORM3D },
	{ V2Type::COLOR, Variant::COLOR },
	{ V2Type::IMAGE, Variant::OBJECT }, // removed in v3; for compatibility, the variant writers will auto-convert Image objects to v2 Image variants
	{ V2Type::NODE_PATH, Variant::NODE_PATH },
	{ V2Type::_RID, Variant::RID },
	{ V2Type::OBJECT, Variant::OBJECT },
	{ V2Type::INPUT_EVENT, Variant::OBJECT }, // removed in v3; for compatibility, the variant writers will auto-convert InputEvent objects to v2 InputEvent variants
	{ V2Type::DICTIONARY, Variant::DICTIONARY },
	{ V2Type::ARRAY, Variant::ARRAY },
	{ V2Type::RAW_ARRAY, Variant::PACKED_BYTE_ARRAY },
	{ V2Type::INT_ARRAY, Variant::PACKED_INT32_ARRAY },
	{ V2Type::REAL_ARRAY, Variant::PACKED_FLOAT32_ARRAY },
	{ V2Type::STRING_ARRAY, Variant::PACKED_STRING_ARRAY },
	{ V2Type::VECTOR2_ARRAY, Variant::PACKED_VECTOR2_ARRAY },
	{ V2Type::VECTOR3_ARRAY, Variant::PACKED_VECTOR3_ARRAY },
	{ V2Type::COLOR_ARRAY, Variant::PACKED_COLOR_ARRAY }
};

static constexpr int v2_to_v4_type_size = sizeof(v2_to_v4_type) / sizeof(V2TypeToV4Type);
static_assert(v2_to_v4_type_size == V2Type::VARIANT_MAX, "v2_to_v4_type_size must be equal to V2Type::VARIANT_MAX");

Variant::Type VariantDecoderCompat::variant_type_enum_v2_to_v4(int type) {
	if (type >= 0 && type < v2_to_v4_type_size) {
		return v2_to_v4_type[type].v4_type;
	}
	return Variant::VARIANT_MAX;
}

struct V3TypeToV4Type {
	V3Type::Type type;
	Variant::Type v4_type;
};

static constexpr V3TypeToV4Type v3_type_to_v4_type[] = {
	{ V3Type::NIL, Variant::NIL },
	{ V3Type::BOOL, Variant::BOOL },
	{ V3Type::INT, Variant::INT },
	{ V3Type::REAL, Variant::FLOAT },
	{ V3Type::STRING, Variant::STRING },
	{ V3Type::VECTOR2, Variant::VECTOR2 },
	{ V3Type::RECT2, Variant::RECT2 },
	{ V3Type::VECTOR3, Variant::VECTOR3 },
	{ V3Type::TRANSFORM2D, Variant::TRANSFORM2D },
	{ V3Type::PLANE, Variant::PLANE },
	{ V3Type::QUAT, Variant::QUATERNION },
	{ V3Type::AABB, Variant::AABB },
	{ V3Type::BASIS, Variant::BASIS },
	{ V3Type::TRANSFORM, Variant::TRANSFORM3D },
	{ V3Type::COLOR, Variant::COLOR },
	{ V3Type::NODE_PATH, Variant::NODE_PATH },
	{ V3Type::_RID, Variant::RID },
	{ V3Type::OBJECT, Variant::OBJECT },
	{ V3Type::DICTIONARY, Variant::DICTIONARY },
	{ V3Type::ARRAY, Variant::ARRAY },
	{ V3Type::POOL_BYTE_ARRAY, Variant::PACKED_BYTE_ARRAY },
	{ V3Type::POOL_INT_ARRAY, Variant::PACKED_INT32_ARRAY },
	{ V3Type::POOL_REAL_ARRAY, Variant::PACKED_FLOAT32_ARRAY },
	{ V3Type::POOL_STRING_ARRAY, Variant::PACKED_STRING_ARRAY },
	{ V3Type::POOL_VECTOR2_ARRAY, Variant::PACKED_VECTOR2_ARRAY },
	{ V3Type::POOL_VECTOR3_ARRAY, Variant::PACKED_VECTOR3_ARRAY },
	{ V3Type::POOL_COLOR_ARRAY, Variant::PACKED_COLOR_ARRAY }
};
static constexpr int v3_to_v4_type_size = sizeof(v3_type_to_v4_type) / sizeof(V3TypeToV4Type);
static_assert(v3_to_v4_type_size == V3Type::VARIANT_MAX, "v3_to_v4_type_size must be equal to V3Type::VARIANT_MAX");

Variant::Type VariantDecoderCompat::variant_type_enum_v3_to_v4(int type) {
	if (type >= 0 && type < v3_to_v4_type_size) {
		return v3_type_to_v4_type[type].v4_type;
	}
	return Variant::VARIANT_MAX;
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

int VariantDecoderCompat::variant_type_enum_v4_to_v2(Variant::Type type) {
	switch (type) {
		case Variant::NIL: {
			return V2Type::NIL;
		} break;
		case Variant::BOOL: {
			return V2Type::BOOL;
		} break;
		case Variant::INT: {
			return V2Type::INT;
		} break;
		case Variant::FLOAT: {
			return V2Type::REAL;
		} break;
		case Variant::STRING: {
			return V2Type::STRING;
		} break;
		case Variant::VECTOR2: {
			return V2Type::VECTOR2;
		} break;
		case Variant::RECT2: {
			return V2Type::RECT2;
		} break;
		case Variant::VECTOR3: {
			return V2Type::VECTOR3;
		} break;
		case Variant::TRANSFORM2D: {
			return V2Type::MATRIX32;
		} break;
		case Variant::PLANE: {
			return V2Type::PLANE;
		} break;
		case Variant::QUATERNION: {
			return V2Type::QUAT;
		} break;
		case Variant::AABB: {
			return V2Type::_AABB;
		} break;
		case Variant::BASIS: {
			return V2Type::MATRIX3;
		} break;
		case Variant::TRANSFORM3D: {
			return V2Type::TRANSFORM;
		} break;
		case Variant::COLOR: {
			return V2Type::COLOR;
		} break;
		// case Variant::IMAGE: // removed in v3
		case Variant::NODE_PATH: {
			return V2Type::NODE_PATH;
		} break;
		case Variant::RID: {
			return V2Type::_RID;
		} break;
		case Variant::OBJECT: {
			return V2Type::OBJECT;
		} break;
		// case Variant::INPUT_EVENT: // removed in v3
		case Variant::DICTIONARY: {
			return V2Type::DICTIONARY;
		} break;
		case Variant::ARRAY: {
			return V2Type::ARRAY;
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			return V2Type::RAW_ARRAY;
		} break;
		case Variant::PACKED_INT64_ARRAY: // Compat: v2 doesn't have PACKED_INT64_ARRAY; handled in encoders
		case Variant::PACKED_INT32_ARRAY: {
			return V2Type::INT_ARRAY;
		} break;
		case Variant::PACKED_FLOAT64_ARRAY: // Compat: v2 doesn't have PACKED_FLOAT64_ARRAY; handled in encoders
		case Variant::PACKED_FLOAT32_ARRAY: {
			return V2Type::REAL_ARRAY;
		} break;
		case Variant::PACKED_STRING_ARRAY: {
			return V2Type::STRING_ARRAY;
		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			return V2Type::VECTOR2_ARRAY;
		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			return V2Type::VECTOR3_ARRAY;
		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			return V2Type::COLOR_ARRAY;
		} break;
		case Variant::VARIANT_MAX: {
			return V2Type::VARIANT_MAX;
		} break;
		default: {
		} break;
	}
	return -1;
}

int VariantDecoderCompat::variant_type_enum_v4_to_v3(Variant::Type type) {
	switch (type) {
		case Variant::NIL: {
			return V3Type::NIL;
		} break;
		case Variant::BOOL: {
			return V3Type::BOOL;
		} break;
		case Variant::INT: {
			return V3Type::INT;
		} break;
		case Variant::FLOAT: {
			return V3Type::REAL;
		} break;
		case Variant::STRING: {
			return V3Type::STRING;
		} break;
		case Variant::VECTOR2: {
			return V3Type::VECTOR2;
		} break;
		case Variant::RECT2: {
			return V3Type::RECT2;
		} break;
		case Variant::TRANSFORM2D: {
			return V3Type::TRANSFORM2D;
		} break;
		case Variant::VECTOR3: {
			return V3Type::VECTOR3;
		} break;
		case Variant::PLANE: {
			return V3Type::PLANE;
		} break;
		case Variant::AABB: {
			return V3Type::AABB;
		} break;
		case Variant::QUATERNION: {
			return V3Type::QUAT;
		} break;
		case Variant::BASIS: {
			return V3Type::BASIS;
		} break;
		case Variant::TRANSFORM3D: {
			return V3Type::TRANSFORM;
		} break;
		case Variant::COLOR: {
			return V3Type::COLOR;
		} break;
		case Variant::NODE_PATH: {
			return V3Type::NODE_PATH;
		} break;
		case Variant::RID: {
			return V3Type::_RID;
		} break;
		case Variant::OBJECT: {
			return V3Type::OBJECT;
		} break;
		case Variant::DICTIONARY: {
			return V3Type::DICTIONARY;
		} break;
		case Variant::ARRAY: {
			return V3Type::ARRAY;
		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			return V3Type::POOL_BYTE_ARRAY;
		} break;
		case Variant::PACKED_INT32_ARRAY: {
			return V3Type::POOL_INT_ARRAY;
		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			return V3Type::POOL_REAL_ARRAY;
		} break;
		case Variant::PACKED_STRING_ARRAY: {
			return V3Type::POOL_STRING_ARRAY;
		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			return V3Type::POOL_VECTOR2_ARRAY;
		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			return V3Type::POOL_VECTOR3_ARRAY;
		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			return V3Type::POOL_COLOR_ARRAY;
		} break;
		default: {
		} break;
	}
	return -1;
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

static Error _decode_container_type(const uint8_t *&buf, int &len, int *r_len, bool p_allow_objects, ContainerTypeKind p_type_kind, ContainerType &r_type) {
	switch (p_type_kind) {
		case CONTAINER_TYPE_KIND_NONE: {
			return OK;
		} break;
		case CONTAINER_TYPE_KIND_BUILTIN: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

			int32_t bt = decode_uint32(buf);
			buf += 4;
			len -= 4;
			if (r_len) {
				(*r_len) += 4;
			}

			ERR_FAIL_INDEX_V(bt, Variant::VARIANT_MAX, ERR_INVALID_DATA);
			r_type.variant_type = (Variant::Type)bt;
			if (!p_allow_objects && r_type.variant_type == Variant::OBJECT) {
				r_type.class_name = EncodedObjectAsID::get_class_static();
			}
			return OK;
		} break;
		case CONTAINER_TYPE_KIND_CLASS_NAME: {
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}

			r_type.variant_type = Variant::OBJECT;
			if (p_allow_objects) {
				r_type.class_name = str;
			} else {
				r_type.class_name = EncodedObjectAsID::get_class_static();
			}
			return OK;
		} break;
		case CONTAINER_TYPE_KIND_SCRIPT: {
			String path;
			Error err = _decode_string(buf, len, r_len, path);
			if (err) {
				return err;
			}

			r_type.variant_type = Variant::OBJECT;
			if (p_allow_objects) {
				ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://") || !ResourceLoader::exists(path, "Script"), ERR_INVALID_DATA, vformat("Invalid script path \"%s\".", path));
				r_type.script = ResourceLoader::load(path, "Script");
				ERR_FAIL_COND_V_MSG(r_type.script.is_null(), ERR_INVALID_DATA, vformat("Can't load script at path \"%s\".", path));
				r_type.class_name = r_type.script->get_instance_base_type();
			} else {
				r_type.class_name = EncodedObjectAsID::get_class_static();
			}
			return OK;
		} break;
	}
	ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid container type kind."); // Future proofing.
}

// This is here solely to prevent script injection via the `Variant::OBJECT` type.
// TODO: This is a maintenance burden to keep in sync with the `decode_variant` function, so we need more comprehensive tests to detect compatibility drift.
Error VariantDecoderCompat::decode_variant_4(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_objects, int p_depth) {
	ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, ERR_OUT_OF_MEMORY, "Variant is too deep. Bailing.");
	const uint8_t *buf = p_buffer;
	int len = p_len;

	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	uint32_t header = decode_uint32(buf);

	ERR_FAIL_COND_V((header & HEADER_TYPE_MASK) >= Variant::VARIANT_MAX, ERR_INVALID_DATA);

	buf += 4;
	len -= 4;
	if (r_len) {
		*r_len = 4;
	}

	// NOTE: We cannot use `sizeof(real_t)` for decoding, in case a different size is encoded.
	// Decoding math types always checks for the encoded size, while encoding always uses compilation setting.
	// This does lead to some code duplication for decoding, but compatibility is the priority.
	switch (header & HEADER_TYPE_MASK) {
		case Variant::NIL: {
			r_variant = Variant();
		} break;
		case Variant::BOOL: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			bool val = decode_uint32(buf);
			r_variant = val;
			if (r_len) {
				(*r_len) += 4;
			}
		} break;
		case Variant::INT: {
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				int64_t val = int64_t(decode_uint64(buf));
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}

			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				int32_t val = int32_t(decode_uint32(buf));
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case Variant::FLOAT: {
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double), ERR_INVALID_DATA);
				double val = decode_double(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += sizeof(double);
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float), ERR_INVALID_DATA);
				float val = decode_float(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += sizeof(float);
				}
			}

		} break;
		case Variant::STRING: {
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}
			r_variant = str;

		} break;

		// Math types.
		case Variant::VECTOR2: {
			Vector2 val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 2, ERR_INVALID_DATA);
				val.x = decode_double(&buf[0]);
				val.y = decode_double(&buf[sizeof(double)]);

				if (r_len) {
					(*r_len) += sizeof(double) * 2;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 2, ERR_INVALID_DATA);
				val.x = decode_float(&buf[0]);
				val.y = decode_float(&buf[sizeof(float)]);

				if (r_len) {
					(*r_len) += sizeof(float) * 2;
				}
			}
			r_variant = val;

		} break;
		case Variant::VECTOR2I: {
			ERR_FAIL_COND_V(len < 4 * 2, ERR_INVALID_DATA);
			Vector2i val;
			val.x = decode_uint32(&buf[0]);
			val.y = decode_uint32(&buf[4]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 2;
			}

		} break;
		case Variant::RECT2: {
			Rect2 val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 4, ERR_INVALID_DATA);
				val.position.x = decode_double(&buf[0]);
				val.position.y = decode_double(&buf[sizeof(double)]);
				val.size.x = decode_double(&buf[sizeof(double) * 2]);
				val.size.y = decode_double(&buf[sizeof(double) * 3]);

				if (r_len) {
					(*r_len) += sizeof(double) * 4;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 4, ERR_INVALID_DATA);
				val.position.x = decode_float(&buf[0]);
				val.position.y = decode_float(&buf[sizeof(float)]);
				val.size.x = decode_float(&buf[sizeof(float) * 2]);
				val.size.y = decode_float(&buf[sizeof(float) * 3]);

				if (r_len) {
					(*r_len) += sizeof(float) * 4;
				}
			}
			r_variant = val;

		} break;
		case Variant::RECT2I: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Rect2i val;
			val.position.x = decode_uint32(&buf[0]);
			val.position.y = decode_uint32(&buf[4]);
			val.size.x = decode_uint32(&buf[8]);
			val.size.y = decode_uint32(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case Variant::VECTOR3: {
			Vector3 val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 3, ERR_INVALID_DATA);
				val.x = decode_double(&buf[0]);
				val.y = decode_double(&buf[sizeof(double)]);
				val.z = decode_double(&buf[sizeof(double) * 2]);

				if (r_len) {
					(*r_len) += sizeof(double) * 3;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 3, ERR_INVALID_DATA);
				val.x = decode_float(&buf[0]);
				val.y = decode_float(&buf[sizeof(float)]);
				val.z = decode_float(&buf[sizeof(float) * 2]);

				if (r_len) {
					(*r_len) += sizeof(float) * 3;
				}
			}
			r_variant = val;

		} break;
		case Variant::VECTOR3I: {
			ERR_FAIL_COND_V(len < 4 * 3, ERR_INVALID_DATA);
			Vector3i val;
			val.x = decode_uint32(&buf[0]);
			val.y = decode_uint32(&buf[4]);
			val.z = decode_uint32(&buf[8]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 3;
			}

		} break;
		case Variant::VECTOR4: {
			Vector4 val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 4, ERR_INVALID_DATA);
				val.x = decode_double(&buf[0]);
				val.y = decode_double(&buf[sizeof(double)]);
				val.z = decode_double(&buf[sizeof(double) * 2]);
				val.w = decode_double(&buf[sizeof(double) * 3]);

				if (r_len) {
					(*r_len) += sizeof(double) * 4;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 4, ERR_INVALID_DATA);
				val.x = decode_float(&buf[0]);
				val.y = decode_float(&buf[sizeof(float)]);
				val.z = decode_float(&buf[sizeof(float) * 2]);
				val.w = decode_float(&buf[sizeof(float) * 3]);

				if (r_len) {
					(*r_len) += sizeof(float) * 4;
				}
			}
			r_variant = val;

		} break;
		case Variant::VECTOR4I: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Vector4i val;
			val.x = decode_uint32(&buf[0]);
			val.y = decode_uint32(&buf[4]);
			val.z = decode_uint32(&buf[8]);
			val.w = decode_uint32(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case Variant::TRANSFORM2D: {
			Transform2D val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 6, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 2; j++) {
						val.columns[i][j] = decode_double(&buf[(i * 2 + j) * sizeof(double)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(double) * 6;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 6, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 2; j++) {
						val.columns[i][j] = decode_float(&buf[(i * 2 + j) * sizeof(float)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(float) * 6;
				}
			}
			r_variant = val;

		} break;
		case Variant::PLANE: {
			Plane val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 4, ERR_INVALID_DATA);
				val.normal.x = decode_double(&buf[0]);
				val.normal.y = decode_double(&buf[sizeof(double)]);
				val.normal.z = decode_double(&buf[sizeof(double) * 2]);
				val.d = decode_double(&buf[sizeof(double) * 3]);

				if (r_len) {
					(*r_len) += sizeof(double) * 4;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 4, ERR_INVALID_DATA);
				val.normal.x = decode_float(&buf[0]);
				val.normal.y = decode_float(&buf[sizeof(float)]);
				val.normal.z = decode_float(&buf[sizeof(float) * 2]);
				val.d = decode_float(&buf[sizeof(float) * 3]);

				if (r_len) {
					(*r_len) += sizeof(float) * 4;
				}
			}
			r_variant = val;

		} break;
		case Variant::QUATERNION: {
			Quaternion val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 4, ERR_INVALID_DATA);
				val.x = decode_double(&buf[0]);
				val.y = decode_double(&buf[sizeof(double)]);
				val.z = decode_double(&buf[sizeof(double) * 2]);
				val.w = decode_double(&buf[sizeof(double) * 3]);

				if (r_len) {
					(*r_len) += sizeof(double) * 4;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 4, ERR_INVALID_DATA);
				val.x = decode_float(&buf[0]);
				val.y = decode_float(&buf[sizeof(float)]);
				val.z = decode_float(&buf[sizeof(float) * 2]);
				val.w = decode_float(&buf[sizeof(float) * 3]);

				if (r_len) {
					(*r_len) += sizeof(float) * 4;
				}
			}
			r_variant = val;

		} break;
		case Variant::AABB: {
			AABB val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 6, ERR_INVALID_DATA);
				val.position.x = decode_double(&buf[0]);
				val.position.y = decode_double(&buf[sizeof(double)]);
				val.position.z = decode_double(&buf[sizeof(double) * 2]);
				val.size.x = decode_double(&buf[sizeof(double) * 3]);
				val.size.y = decode_double(&buf[sizeof(double) * 4]);
				val.size.z = decode_double(&buf[sizeof(double) * 5]);

				if (r_len) {
					(*r_len) += sizeof(double) * 6;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 6, ERR_INVALID_DATA);
				val.position.x = decode_float(&buf[0]);
				val.position.y = decode_float(&buf[sizeof(float)]);
				val.position.z = decode_float(&buf[sizeof(float) * 2]);
				val.size.x = decode_float(&buf[sizeof(float) * 3]);
				val.size.y = decode_float(&buf[sizeof(float) * 4]);
				val.size.z = decode_float(&buf[sizeof(float) * 5]);

				if (r_len) {
					(*r_len) += sizeof(float) * 6;
				}
			}
			r_variant = val;

		} break;
		case Variant::BASIS: {
			Basis val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 9, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						val.rows[i][j] = decode_double(&buf[(i * 3 + j) * sizeof(double)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(double) * 9;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 9, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						val.rows[i][j] = decode_float(&buf[(i * 3 + j) * sizeof(float)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(float) * 9;
				}
			}
			r_variant = val;

		} break;
		case Variant::TRANSFORM3D: {
			Transform3D val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 12, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						val.basis.rows[i][j] = decode_double(&buf[(i * 3 + j) * sizeof(double)]);
					}
				}
				val.origin[0] = decode_double(&buf[sizeof(double) * 9]);
				val.origin[1] = decode_double(&buf[sizeof(double) * 10]);
				val.origin[2] = decode_double(&buf[sizeof(double) * 11]);

				if (r_len) {
					(*r_len) += sizeof(double) * 12;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 12, ERR_INVALID_DATA);
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						val.basis.rows[i][j] = decode_float(&buf[(i * 3 + j) * sizeof(float)]);
					}
				}
				val.origin[0] = decode_float(&buf[sizeof(float) * 9]);
				val.origin[1] = decode_float(&buf[sizeof(float) * 10]);
				val.origin[2] = decode_float(&buf[sizeof(float) * 11]);

				if (r_len) {
					(*r_len) += sizeof(float) * 12;
				}
			}
			r_variant = val;

		} break;
		case Variant::PROJECTION: {
			Projection val;
			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_COND_V((size_t)len < sizeof(double) * 16, ERR_INVALID_DATA);
				for (int i = 0; i < 4; i++) {
					for (int j = 0; j < 4; j++) {
						val.columns[i][j] = decode_double(&buf[(i * 4 + j) * sizeof(double)]);
					}
				}
				if (r_len) {
					(*r_len) += sizeof(double) * 16;
				}
			} else {
				ERR_FAIL_COND_V((size_t)len < sizeof(float) * 16, ERR_INVALID_DATA);
				for (int i = 0; i < 4; i++) {
					for (int j = 0; j < 4; j++) {
						val.columns[i][j] = decode_float(&buf[(i * 4 + j) * sizeof(float)]);
					}
				}

				if (r_len) {
					(*r_len) += sizeof(float) * 16;
				}
			}
			r_variant = val;

		} break;

		// Misc types.
		case Variant::COLOR: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Color val;
			val.r = decode_float(&buf[0]);
			val.g = decode_float(&buf[4]);
			val.b = decode_float(&buf[8]);
			val.a = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4; // Colors should always be in single-precision.
			}
		} break;
		case Variant::STRING_NAME: {
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}
			r_variant = StringName(str);

		} break;

		case Variant::NODE_PATH: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t strlen = decode_uint32(buf);

			if (strlen & 0x80000000) {
				// New format.
				ERR_FAIL_COND_V(len < 12, ERR_INVALID_DATA);
				Vector<StringName> names;
				Vector<StringName> subnames;

				uint32_t namecount = strlen &= 0x7FFFFFFF;
				uint32_t subnamecount = decode_uint32(buf + 4);
				uint32_t np_flags = decode_uint32(buf + 8);

				len -= 12;
				buf += 12;

				if (np_flags & 2) { // Obsolete format with property separate from subpath.
					subnamecount++;
				}

				uint32_t total = namecount + subnamecount;

				if (r_len) {
					(*r_len) += 12;
				}

				for (uint32_t i = 0; i < total; i++) {
					String str;
					Error err = _decode_string(buf, len, r_len, str);
					if (err) {
						return err;
					}

					if (i < namecount) {
						names.push_back(str);
					} else {
						subnames.push_back(str);
					}
				}

				r_variant = NodePath(names, subnames, np_flags & 1);

			} else {
				// Old format, just a string.
				ERR_FAIL_V(ERR_INVALID_DATA);
			}

		} break;
		case Variant::RID: {
			ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
			uint64_t id = decode_uint64(buf);
			if (r_len) {
				(*r_len) += 8;
			}

			r_variant = RID::from_uint64(id);
		} break;
		case Variant::OBJECT: {
			if (header & HEADER_DATA_FLAG_OBJECT_AS_ID) {
				// This _is_ allowed.
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				ObjectID val = ObjectID(decode_uint64(buf));
				if (r_len) {
					(*r_len) += 8;
				}

				if (val.is_null()) {
					r_variant = (Object *)nullptr;
				} else {
					Ref<EncodedObjectAsID> obj_as_id;
					obj_as_id.instantiate();
					obj_as_id->set_object_id(val);

					r_variant = obj_as_id;
				}
			} else {
				ERR_FAIL_COND_V(!p_allow_objects, ERR_UNAUTHORIZED);

				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				if (str.is_empty()) {
					r_variant = (Object *)nullptr;
				} else {
					Ref<MissingResource> missing_resource;
					Object *obj;

					// COMPAT: use Converters for type
					auto converter = ResourceCompatLoader::get_converter_for_type(str, 4);
					if (converter.is_valid()) {
						missing_resource.instantiate();
						missing_resource->set_original_class(str);
						missing_resource->set_recording_properties(true);
						obj = missing_resource.ptr();
					} else {
						ERR_FAIL_COND_V(!ClassDB::can_instantiate(str), ERR_INVALID_DATA);
						obj = ClassDB::instantiate(str);
					}
					ERR_FAIL_COND_V(!obj, ERR_UNAVAILABLE);
					ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

					// Avoid premature free `RefCounted`. This must be done before properties are initialized,
					// since script functions (setters, implicit initializer) may be called. See GH-68666.
					Variant variant;
					if (Object::cast_to<RefCounted>(obj)) {
						Ref<RefCounted> ref = Ref<RefCounted>(Object::cast_to<RefCounted>(obj));
						variant = ref;
					} else {
						variant = obj;
					}

					ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
					int32_t count = decode_uint32(buf);
					buf += 4;
					len -= 4;
					if (r_len) {
						(*r_len) += 4; // Size of count number.
					}

					for (int i = 0; i < count; i++) {
						str = String();
						err = _decode_string(buf, len, r_len, str);
						if (err) {
							return err;
						}

						Variant value;
						int used;
						err = decode_variant_4(value, buf, len, &used, p_allow_objects, p_depth + 1);
						if (err) {
							return err;
						}

						buf += used;
						len -= used;
						if (r_len) {
							(*r_len) += used;
						}

						if (str == "script" && value.get_type() != Variant::NIL) {
							ERR_FAIL_COND_V_MSG(value.get_type() != Variant::STRING, ERR_INVALID_DATA, "Invalid value for \"script\" property, expected script path as String.");
							String path = value;
							// COMPAT: use ResourceCompatLoader.
							ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://") || !ResourceCompatLoader::exists(path), ERR_INVALID_DATA, vformat("Invalid script path \"%s\".", path));
							Ref<Script> script = ResourceCompatLoader::real_load(path, "Script");
							ERR_FAIL_COND_V_MSG(script.is_null(), ERR_INVALID_DATA, vformat("Can't load script at path \"%s\".", path));
							obj->set_script(script);
						} else {
							obj->set(str, value);
						}
					}
					if (converter.is_valid()) {
						Ref<Resource> res = converter->convert(missing_resource, ResourceInfo::LoadType::REAL_LOAD, 3, &err);
						res->set_path_cache(missing_resource->get_path());
						res->set_local_to_scene(missing_resource->is_local_to_scene());
						res->set_scene_unique_id(missing_resource->get_scene_unique_id());
						r_variant = res;
					}

					r_variant = variant;
				}
			}

		} break;
		case Variant::CALLABLE: {
			r_variant = Callable();
		} break;
		case Variant::SIGNAL: {
			String name;
			Error err = _decode_string(buf, len, r_len, name);
			if (err) {
				return err;
			}

			ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
			ObjectID id = ObjectID(decode_uint64(buf));
			if (r_len) {
				(*r_len) += 8;
			}

			r_variant = Signal(id, StringName(name));
		} break;
		case Variant::DICTIONARY: {
			ContainerType key_type;

			{
				ContainerTypeKind key_type_kind = GET_CONTAINER_TYPE_KIND(header, TYPED_DICTIONARY_KEY);
				Error err = _decode_container_type(buf, len, r_len, p_allow_objects, key_type_kind, key_type);
				if (err) {
					return err;
				}
			}

			ContainerType value_type;

			{
				ContainerTypeKind value_type_kind = GET_CONTAINER_TYPE_KIND(header, TYPED_DICTIONARY_VALUE);
				Error err = _decode_container_type(buf, len, r_len, p_allow_objects, value_type_kind, value_type);
				if (err) {
					return err;
				}
			}

			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

			int32_t count = decode_uint32(buf);
			//bool shared = count & 0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4; // Size of count number.
			}

			Dictionary dict;
			if (key_type.variant_type != Variant::NIL || value_type.variant_type != Variant::NIL) {
				dict.set_typed(key_type, value_type);
			}

			for (int i = 0; i < count; i++) {
				Variant key, value;

				int used;
				Error err = decode_variant_4(key, buf, len, &used, p_allow_objects, p_depth + 1);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				err = decode_variant_4(value, buf, len, &used, p_allow_objects, p_depth + 1);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				dict[key] = value;
			}

			r_variant = dict;

		} break;
		case Variant::ARRAY: {
			ContainerType type;

			{
				ContainerTypeKind type_kind = GET_CONTAINER_TYPE_KIND(header, TYPED_ARRAY);
				Error err = _decode_container_type(buf, len, r_len, p_allow_objects, type_kind, type);
				if (err) {
					return err;
				}
			}

			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

			int32_t count = decode_uint32(buf);
			//bool shared = count & 0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4; // Size of count number.
			}

			Array array;
			if (type.variant_type != Variant::NIL) {
				array.set_typed(type);
			}

			for (int i = 0; i < count; i++) {
				int used = 0;
				Variant elem;
				Error err = decode_variant_4(elem, buf, len, &used, p_allow_objects, p_depth + 1);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");
				buf += used;
				len -= used;
				array.push_back(elem);
				if (r_len) {
					(*r_len) += used;
				}
			}

			r_variant = array;

		} break;

		// Packed arrays.
		case Variant::PACKED_BYTE_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_COND_V(count < 0 || count > len, ERR_INVALID_DATA);

			Vector<uint8_t> data;

			if (count) {
				data.resize(count);
				uint8_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = buf[i];
				}
			}

			r_variant = data;

			if (r_len) {
				if (count % 4) {
					(*r_len) += 4 - count % 4;
				}
				(*r_len) += 4 + count;
			}

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<int32_t> data;

			if (count) {
				//const int *rbuf = (const int *)buf;
				data.resize(count);
				int32_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_uint32(&buf[i * 4]);
				}
			}
			r_variant = Variant(data);
			if (r_len) {
				(*r_len) += 4 + count * sizeof(int32_t);
			}

		} break;
		case Variant::PACKED_INT64_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 8, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 8 > len, ERR_INVALID_DATA);

			Vector<int64_t> data;

			if (count) {
				//const int *rbuf = (const int *)buf;
				data.resize(count);
				int64_t *w = data.ptrw();
				for (int64_t i = 0; i < count; i++) {
					w[i] = decode_uint64(&buf[i * 8]);
				}
			}
			r_variant = Variant(data);
			if (r_len) {
				(*r_len) += 4 + count * sizeof(int64_t);
			}

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<float> data;

			if (count) {
				//const float *rbuf = (const float *)buf;
				data.resize(count);
				float *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_float(&buf[i * 4]);
				}
			}
			r_variant = data;

			if (r_len) {
				(*r_len) += 4 + count * sizeof(float);
			}

		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 8, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 8 > len, ERR_INVALID_DATA);

			Vector<double> data;

			if (count) {
				data.resize(count);
				double *w = data.ptrw();
				for (int64_t i = 0; i < count; i++) {
					w[i] = decode_double(&buf[i * 8]);
				}
			}
			r_variant = data;

			if (r_len) {
				(*r_len) += 4 + count * sizeof(double);
			}

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);

			Vector<String> strings;
			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4; // Size of count number.
			}

			for (int32_t i = 0; i < count; i++) {
				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				strings.push_back(str);
			}

			r_variant = strings;

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			Vector<Vector2> varray;

			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_MUL_OF(count, sizeof(double) * 2, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(double) * 2 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector2 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_double(buf + i * sizeof(double) * 2 + sizeof(double) * 0);
						w[i].y = decode_double(buf + i * sizeof(double) * 2 + sizeof(double) * 1);
					}

					int adv = sizeof(double) * 2 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			} else {
				ERR_FAIL_MUL_OF(count, sizeof(float) * 2, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(float) * 2 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector2 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_float(buf + i * sizeof(float) * 2 + sizeof(float) * 0);
						w[i].y = decode_float(buf + i * sizeof(float) * 2 + sizeof(float) * 1);
					}

					int adv = sizeof(float) * 2 * count;

					if (r_len) {
						(*r_len) += adv;
					}
				}
			}
			r_variant = varray;

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			Vector<Vector3> varray;

			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_MUL_OF(count, sizeof(double) * 3, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(double) * 3 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector3 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_double(buf + i * sizeof(double) * 3 + sizeof(double) * 0);
						w[i].y = decode_double(buf + i * sizeof(double) * 3 + sizeof(double) * 1);
						w[i].z = decode_double(buf + i * sizeof(double) * 3 + sizeof(double) * 2);
					}

					int adv = sizeof(double) * 3 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			} else {
				ERR_FAIL_MUL_OF(count, sizeof(float) * 3, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(float) * 3 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector3 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_float(buf + i * sizeof(float) * 3 + sizeof(float) * 0);
						w[i].y = decode_float(buf + i * sizeof(float) * 3 + sizeof(float) * 1);
						w[i].z = decode_float(buf + i * sizeof(float) * 3 + sizeof(float) * 2);
					}

					int adv = sizeof(float) * 3 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			}
			r_variant = varray;

		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 4 > len, ERR_INVALID_DATA);

			Vector<Color> carray;

			if (r_len) {
				(*r_len) += 4; // Size of count number.
			}

			if (count) {
				carray.resize(count);
				Color *w = carray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					// Colors should always be in single-precision.
					w[i].r = decode_float(buf + i * 4 * 4 + 4 * 0);
					w[i].g = decode_float(buf + i * 4 * 4 + 4 * 1);
					w[i].b = decode_float(buf + i * 4 * 4 + 4 * 2);
					w[i].a = decode_float(buf + i * 4 * 4 + 4 * 3);
				}

				int adv = 4 * 4 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = carray;

		} break;

		case Variant::PACKED_VECTOR4_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			Vector<Vector4> varray;

			if (header & HEADER_DATA_FLAG_64) {
				ERR_FAIL_MUL_OF(count, sizeof(double) * 4, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(double) * 4 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector4 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_double(buf + i * sizeof(double) * 4 + sizeof(double) * 0);
						w[i].y = decode_double(buf + i * sizeof(double) * 4 + sizeof(double) * 1);
						w[i].z = decode_double(buf + i * sizeof(double) * 4 + sizeof(double) * 2);
						w[i].w = decode_double(buf + i * sizeof(double) * 4 + sizeof(double) * 3);
					}

					int adv = sizeof(double) * 4 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			} else {
				ERR_FAIL_MUL_OF(count, sizeof(float) * 4, ERR_INVALID_DATA);
				ERR_FAIL_COND_V(count < 0 || count * sizeof(float) * 4 > (size_t)len, ERR_INVALID_DATA);

				if (r_len) {
					(*r_len) += 4; // Size of count number.
				}

				if (count) {
					varray.resize(count);
					Vector4 *w = varray.ptrw();

					for (int32_t i = 0; i < count; i++) {
						w[i].x = decode_float(buf + i * sizeof(float) * 4 + sizeof(float) * 0);
						w[i].y = decode_float(buf + i * sizeof(float) * 4 + sizeof(float) * 1);
						w[i].z = decode_float(buf + i * sizeof(float) * 4 + sizeof(float) * 2);
						w[i].w = decode_float(buf + i * sizeof(float) * 4 + sizeof(float) * 3);
					}

					int adv = sizeof(float) * 4 * count;

					if (r_len) {
						(*r_len) += adv;
					}
					len -= adv;
					buf += adv;
				}
			}
			r_variant = varray;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}

Error VariantDecoderCompat::decode_variant_3(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_objects) {
	const uint8_t *buf = p_buffer;
	int len = p_len;

	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	uint32_t type = decode_uint32(buf);

	ERR_FAIL_COND_V((type & ENCODE_MASK) >= V3Type::VARIANT_MAX, ERR_INVALID_DATA);

	buf += 4;
	len -= 4;
	if (r_len) {
		*r_len = 4;
	}

	switch (type & ENCODE_MASK) {
		case V3Type::NIL: {
			r_variant = Variant();
		} break;
		case V3Type::BOOL: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			bool val = decode_uint32(buf);
			r_variant = val;
			if (r_len) {
				(*r_len) += 4;
			}
		} break;
		case V3Type::INT: {
			if (type & ENCODE_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				int64_t val = decode_uint64(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}

			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				int32_t val = decode_uint32(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case V3Type::REAL: {
			if (type & ENCODE_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				double val = decode_double(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}
			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				float val = decode_float(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case V3Type::STRING: {
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}
			r_variant = str;

		} break;

		// math types
		case V3Type::VECTOR2: {
			ERR_FAIL_COND_V(len < 4 * 2, ERR_INVALID_DATA);
			Vector2 val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 2;
			}

		} break;
		case V3Type::RECT2: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Rect2 val;
			val.position.x = decode_float(&buf[0]);
			val.position.y = decode_float(&buf[4]);
			val.size.x = decode_float(&buf[8]);
			val.size.y = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case V3Type::VECTOR3: {
			ERR_FAIL_COND_V(len < 4 * 3, ERR_INVALID_DATA);
			Vector3 val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			val.z = decode_float(&buf[8]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 3;
			}

		} break;
		case V3Type::TRANSFORM2D: {
			ERR_FAIL_COND_V(len < 4 * 6, ERR_INVALID_DATA);
			Transform2D val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 2; j++) {
					val.columns[i][j] = decode_float(&buf[(i * 2 + j) * 4]);
				}
			}

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 6;
			}

		} break;
		case V3Type::PLANE: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Plane val;
			val.normal.x = decode_float(&buf[0]);
			val.normal.y = decode_float(&buf[4]);
			val.normal.z = decode_float(&buf[8]);
			val.d = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case V3Type::QUAT: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Quaternion val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			val.z = decode_float(&buf[8]);
			val.w = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case V3Type::AABB: {
			ERR_FAIL_COND_V(len < 4 * 6, ERR_INVALID_DATA);
			AABB val;
			val.position.x = decode_float(&buf[0]);
			val.position.y = decode_float(&buf[4]);
			val.position.z = decode_float(&buf[8]);
			val.size.x = decode_float(&buf[12]);
			val.size.y = decode_float(&buf[16]);
			val.size.z = decode_float(&buf[20]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 6;
			}

		} break;
		case V3Type::BASIS: {
			ERR_FAIL_COND_V(len < 4 * 9, ERR_INVALID_DATA);
			Basis val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					val.rows[i][j] = decode_float(&buf[(i * 3 + j) * 4]);
				}
			}

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 9;
			}

		} break;
		case V3Type::TRANSFORM: {
			ERR_FAIL_COND_V(len < 4 * 12, ERR_INVALID_DATA);
			Transform3D val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					val.basis.rows[i][j] = decode_float(&buf[(i * 3 + j) * 4]);
				}
			}
			val.origin[0] = decode_float(&buf[36]);
			val.origin[1] = decode_float(&buf[40]);
			val.origin[2] = decode_float(&buf[44]);

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 12;
			}

		} break;

		// misc types
		case V3Type::COLOR: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Color val;
			val.r = decode_float(&buf[0]);
			val.g = decode_float(&buf[4]);
			val.b = decode_float(&buf[8]);
			val.a = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case V3Type::NODE_PATH: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t strlen = decode_uint32(buf);

			if (strlen & 0x80000000) {
				//new format
				ERR_FAIL_COND_V(len < 12, ERR_INVALID_DATA);
				Vector<StringName> names;
				Vector<StringName> subnames;

				uint32_t namecount = strlen &= 0x7FFFFFFF;
				uint32_t subnamecount = decode_uint32(buf + 4);
				uint32_t flags = decode_uint32(buf + 8);

				len -= 12;
				buf += 12;

				if (flags & 2) { // Obsolete format with property separate from subpath
					subnamecount++;
				}

				uint32_t total = namecount + subnamecount;

				if (r_len) {
					(*r_len) += 12;
				}

				for (uint32_t i = 0; i < total; i++) {
					String str;
					Error err = _decode_string(buf, len, r_len, str);
					if (err) {
						return err;
					}

					if (i < namecount) {
						names.push_back(str);
					} else {
						subnames.push_back(str);
					}
				}

				r_variant = NodePath(names, subnames, flags & 1);

			} else {
				//old format, just a string

				ERR_FAIL_V(ERR_INVALID_DATA);
			}

		} break;
		case V3Type::_RID: {
			r_variant = RID();
		} break;
		case V3Type::OBJECT: {
			if (type & ENCODE_FLAG_OBJECT_AS_ID) {
				//this _is_ allowed
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				ObjectID val = ObjectID(decode_uint64(buf));
				if (r_len) {
					(*r_len) += 8;
				}

				if (val.is_null()) {
					r_variant = (Object *)nullptr;
				} else {
					Ref<EncodedObjectAsID> obj_as_id;
					obj_as_id.instantiate();
					obj_as_id->set_object_id(val);

					r_variant = obj_as_id;
				}

			} else {
				ERR_FAIL_COND_V(!p_allow_objects, ERR_UNAUTHORIZED);

				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				if (str == String()) {
					r_variant = (Object *)nullptr;
				} else {
					Ref<MissingResource> missing_resource;
					Object *obj;

					auto converter = ResourceCompatLoader::get_converter_for_type(str, 3);
					if (converter.is_valid()) {
						missing_resource.instantiate();
						missing_resource->set_original_class(str);
						missing_resource->set_recording_properties(true);
						obj = missing_resource.ptr();
					} else {
						obj = ClassDB::instantiate(str);
					}
					ERR_FAIL_COND_V(!obj, ERR_UNAVAILABLE);
					ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

					int32_t count = decode_uint32(buf);
					buf += 4;
					len -= 4;
					if (r_len) {
						(*r_len) += 4;
					}

					for (int i = 0; i < count; i++) {
						str = String();
						err = _decode_string(buf, len, r_len, str);
						if (err) {
							return err;
						}

						Variant value;
						int used;
						err = decode_variant_3(value, buf, len, &used, p_allow_objects);
						if (err) {
							return err;
						}

						buf += used;
						len -= used;
						if (r_len) {
							(*r_len) += used;
						}
						obj->set(str, value);
					}
					if (converter.is_valid()) {
						Ref<Resource> res = converter->convert(missing_resource, ResourceInfo::LoadType::REAL_LOAD, 3, &err);
						res->set_path_cache(missing_resource->get_path());
						r_variant = res;
					} else if (Object::cast_to<RefCounted>(obj)) {
						Ref<RefCounted> ref = Ref<RefCounted>(Object::cast_to<RefCounted>(obj));
						r_variant = ref;
					} else {
						r_variant = obj;
					}
				}
			}

		} break;
		case V3Type::DICTIONARY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			//  bool shared = count&0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}

			Dictionary d;

			for (int i = 0; i < count; i++) {
				Variant key, value;

				int used;
				Error err = decode_variant_3(key, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				err = decode_variant_3(value, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				d[key] = value;
			}

			r_variant = d;

		} break;
		case V3Type::ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			//  bool shared = count&0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}

			Array varr;

			for (int i = 0; i < count; i++) {
				int used = 0;
				Variant v;
				Error err = decode_variant_3(v, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");
				buf += used;
				len -= used;
				varr.push_back(v);
				if (r_len) {
					(*r_len) += used;
				}
			}

			r_variant = varr;

		} break;

		// arrays
		case V3Type::POOL_BYTE_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_COND_V(count < 0 || count > len, ERR_INVALID_DATA);

			Vector<uint8_t> data;

			if (count) {
				data.resize(count);
				uint8_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = buf[i];
				}
			}

			r_variant = data;

			if (r_len) {
				if (count % 4) {
					(*r_len) += 4 - count % 4;
				}
				(*r_len) += 4 + count;
			}

		} break;
		case V3Type::POOL_INT_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<int32_t> data;

			if (count) {
				//const int*rbuf=(const int*)buf;
				data.resize(count);
				int32_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_uint32(&buf[i * 4]);
				}
			}
			r_variant = Variant(data);
			if (r_len) {
				(*r_len) += 4 + count * sizeof(int32_t);
			}

		} break;
		case V3Type::POOL_REAL_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<float> data;

			if (count) {
				//const float*rbuf=(const float*)buf;
				data.resize(count);
				float *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_float(&buf[i * 4]);
				}
			}
			r_variant = data;

			if (r_len) {
				(*r_len) += 4 + count * sizeof(float);
			}

		} break;
		case V3Type::POOL_STRING_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);

			Vector<String> strings;
			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}
			//printf("string count: %i\n",count);

			for (int32_t i = 0; i < count; i++) {
				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				strings.push_back(str);
			}

			r_variant = strings;

		} break;
		case V3Type::POOL_VECTOR2_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 2, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 2 > len, ERR_INVALID_DATA);
			Vector<Vector2> varray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				varray.resize(count);
				Vector2 *w = varray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].x = decode_float(buf + i * 4 * 2 + 4 * 0);
					w[i].y = decode_float(buf + i * 4 * 2 + 4 * 1);
				}

				int adv = 4 * 2 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = varray;

		} break;
		case V3Type::POOL_VECTOR3_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 3, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 3 > len, ERR_INVALID_DATA);

			Vector<Vector3> varray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				varray.resize(count);
				Vector3 *w = varray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].x = decode_float(buf + i * 4 * 3 + 4 * 0);
					w[i].y = decode_float(buf + i * 4 * 3 + 4 * 1);
					w[i].z = decode_float(buf + i * 4 * 3 + 4 * 2);
				}

				int adv = 4 * 3 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = varray;

		} break;
		case V3Type::POOL_COLOR_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 4 > len, ERR_INVALID_DATA);

			Vector<Color> carray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				carray.resize(count);
				Color *w = carray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].r = decode_float(buf + i * 4 * 4 + 4 * 0);
					w[i].g = decode_float(buf + i * 4 * 4 + 4 * 1);
					w[i].b = decode_float(buf + i * 4 * 4 + 4 * 2);
					w[i].a = decode_float(buf + i * 4 * 4 + 4 * 3);
				}

				int adv = 4 * 4 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = carray;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}

Error VariantDecoderCompat::decode_variant_2(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_objects) {
	const uint8_t *buf = p_buffer;
	int len = p_len;

	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	uint32_t type = decode_uint32(buf);

	ERR_FAIL_COND_V((type & ENCODE_MASK) >= V2Type::VARIANT_MAX, ERR_INVALID_DATA);

	buf += 4;
	len -= 4;
	if (r_len) {
		*r_len = 4;
	}

	switch (type & ENCODE_MASK) {
		case V2Type::NIL: {
			r_variant = Variant();
		} break;
		case V2Type::BOOL: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			bool val = decode_uint32(buf);
			r_variant = val;
			if (r_len) {
				(*r_len) += 4;
			}
		} break;
		case V2Type::INT: {
			if (type & ENCODE_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				int64_t val = decode_uint64(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}

			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				int32_t val = decode_uint32(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case V2Type::REAL: {
			if (type & ENCODE_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				double val = decode_double(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}
			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				float val = decode_float(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case V2Type::STRING: {
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}
			r_variant = str;

		} break;

		// math types
		case V2Type::VECTOR2: {
			ERR_FAIL_COND_V(len < 4 * 2, ERR_INVALID_DATA);
			Vector2 val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 2;
			}

		} break;
		case V2Type::RECT2: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Rect2 val;
			val.position.x = decode_float(&buf[0]);
			val.position.y = decode_float(&buf[4]);
			val.size.x = decode_float(&buf[8]);
			val.size.y = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case V2Type::VECTOR3: {
			ERR_FAIL_COND_V(len < 4 * 3, ERR_INVALID_DATA);
			Vector3 val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			val.z = decode_float(&buf[8]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 3;
			}

		} break;
		case V2Type::MATRIX32: {
			ERR_FAIL_COND_V(len < 4 * 6, ERR_INVALID_DATA);
			Transform2D val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 2; j++) {
					val.columns[i][j] = decode_float(&buf[(i * 2 + j) * 4]);
				}
			}

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 6;
			}

		} break;
		case V2Type::PLANE: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Plane val;
			val.normal.x = decode_float(&buf[0]);
			val.normal.y = decode_float(&buf[4]);
			val.normal.z = decode_float(&buf[8]);
			val.d = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case V2Type::QUAT: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Quaternion val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			val.z = decode_float(&buf[8]);
			val.w = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case V2Type::_AABB: {
			ERR_FAIL_COND_V(len < 4 * 6, ERR_INVALID_DATA);
			AABB val;
			val.position.x = decode_float(&buf[0]);
			val.position.y = decode_float(&buf[4]);
			val.position.z = decode_float(&buf[8]);
			val.size.x = decode_float(&buf[12]);
			val.size.y = decode_float(&buf[16]);
			val.size.z = decode_float(&buf[20]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 6;
			}

		} break;
		case V2Type::MATRIX3: {
			ERR_FAIL_COND_V(len < 4 * 9, ERR_INVALID_DATA);
			Basis val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					val.rows[i][j] = decode_float(&buf[(i * 3 + j) * 4]);
				}
			}

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 9;
			}

		} break;
		case V2Type::TRANSFORM: {
			ERR_FAIL_COND_V(len < 4 * 12, ERR_INVALID_DATA);
			Transform3D val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					val.basis.rows[i][j] = decode_float(&buf[(i * 3 + j) * 4]);
				}
			}
			val.origin[0] = decode_float(&buf[36]);
			val.origin[1] = decode_float(&buf[40]);
			val.origin[2] = decode_float(&buf[44]);

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 12;
			}

		} break;

		// misc types
		case V2Type::COLOR: {
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Color val;
			val.r = decode_float(&buf[0]);
			val.g = decode_float(&buf[4]);
			val.b = decode_float(&buf[8]);
			val.a = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;

		case V2Type::IMAGE: { // not in code, but is in pcfgs
			ERR_FAIL_COND_V(len < 5 * 4, ERR_INVALID_DATA);
			V2Image::Format v2_fmt = (V2Image::Format)decode_uint32(&buf[0]);
			Image::Format fmt = ImageEnumCompat::convert_image_format_enum_v2_to_v4(v2_fmt);
			ERR_FAIL_INDEX_V(v2_fmt, V2Image::IMAGE_FORMAT_V2_MAX, ERR_INVALID_DATA);

			uint32_t mipmaps = decode_uint32(&buf[4]);
			uint32_t w = decode_uint32(&buf[8]);
			uint32_t h = decode_uint32(&buf[12]);

			int32_t datalen = decode_uint32(&buf[16]);

			Ref<Image> img;
			if (datalen > 0) {
				len -= 5 * 4;
				ERR_FAIL_COND_V(len < datalen, ERR_INVALID_DATA);
				Vector<uint8_t> data;
				data.resize(datalen);
				memcpy(data.ptrw(), &buf[20], datalen);
				if (v2_fmt == 5 || v2_fmt == 6 || v2_fmt == 1) {
					Error err;
					img = ImageParserV2::convert_indexed_image(data, w, h, mipmaps, v2_fmt, &err);
					ERR_FAIL_COND_V_MSG(err || img.is_null(), ERR_PARSE_ERROR,
							"Can't convert deprecated image format " + ImageEnumCompat::get_v2_format_name(v2_fmt) + " to new image formats!");
				} else if (fmt != Image::FORMAT_MAX) {
					img = Image::create_from_data(w, h, mipmaps, fmt, data);
				}
			} else {
				if (w == 0 && h == 0) {
					img.instantiate();
				} else {
					img = Image::create_empty(w, h, mipmaps, fmt);
				}
			}
			r_variant = img;
			if (r_len) {
				if (datalen % 4) {
					(*r_len) += 4 - datalen % 4;
				}

				(*r_len) += 4 * 5 + datalen;
			}
			// wait until after we've skipped all the data to do this
			ERR_FAIL_COND_V_MSG(fmt == Image::FORMAT_MAX, ERR_UNAVAILABLE, "Can't convert deprecated image format " + ImageEnumCompat::get_v2_format_name((V2Image::Format)v2_fmt) + " to new image formats!");
		} break;

		case V2Type::NODE_PATH: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t strlen = decode_uint32(buf);

			if (strlen & 0x80000000) {
				//new format
				ERR_FAIL_COND_V(len < 12, ERR_INVALID_DATA);
				Vector<StringName> names;
				Vector<StringName> subnames;

				uint32_t namecount = strlen &= 0x7FFFFFFF;
				uint32_t subnamecount = decode_uint32(buf + 4);
				uint32_t flags = decode_uint32(buf + 8);

				len -= 12;
				buf += 12;

				if (flags & 2) { // Obsolete format with property separate from subpath
					subnamecount++;
				}

				uint32_t total = namecount + subnamecount;

				if (r_len) {
					(*r_len) += 12;
				}

				for (uint32_t i = 0; i < total; i++) {
					String str;
					Error err = _decode_string(buf, len, r_len, str);
					if (err) {
						return err;
					}

					if (i < namecount) {
						names.push_back(str);
					} else {
						subnames.push_back(str);
					}
				}

				r_variant = NodePath(names, subnames, flags & 1);

			} else {
				//old format, just a string

				buf += 4;
				len -= 4;
				ERR_FAIL_COND_V((int)strlen > len, ERR_INVALID_DATA);

				String str;
				str.append_utf8((const char *)buf, strlen);

				r_variant = NodePath(str);

				if (r_len) {
					(*r_len) += 4 + strlen;
				}

				ERR_FAIL_V(ERR_INVALID_DATA);
			}

		} break;
		case V2Type::_RID: {
			r_variant = RID();
		} break;
		case V2Type::OBJECT: {
			if (type & ENCODE_FLAG_OBJECT_AS_ID) {
				//this _is_ allowed
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				ObjectID val = ObjectID(decode_uint64(buf));
				if (r_len) {
					(*r_len) += 8;
				}

				if (val.is_null()) {
					r_variant = (Object *)nullptr;
				} else {
					Ref<EncodedObjectAsID> obj_as_id;
					obj_as_id.instantiate();
					obj_as_id->set_object_id(val);

					r_variant = obj_as_id;
				}

			} else {
				ERR_FAIL_COND_V(!p_allow_objects, ERR_UNAUTHORIZED);

				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				if (str == String()) {
					r_variant = (Object *)nullptr;
				} else {
					Ref<MissingResource> missing_resource;
					Object *obj;

					auto converter = ResourceCompatLoader::get_converter_for_type(str, 2);
					if (converter.is_valid()) {
						missing_resource.instantiate();
						missing_resource->set_original_class(str);
						missing_resource->set_recording_properties(true);
						obj = missing_resource.ptr();
					} else {
						obj = ClassDB::instantiate(str);
					}

					ERR_FAIL_COND_V(!obj, ERR_UNAVAILABLE);
					ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

					int32_t count = decode_uint32(buf);
					buf += 4;
					len -= 4;
					if (r_len) {
						(*r_len) += 4;
					}

					for (int i = 0; i < count; i++) {
						str = String();
						err = _decode_string(buf, len, r_len, str);
						if (err) {
							return err;
						}

						Variant value;
						int used;
						err = decode_variant_2(value, buf, len, &used, p_allow_objects);
						if (err) {
							return err;
						}

						buf += used;
						len -= used;
						if (r_len) {
							(*r_len) += used;
						}

						obj->set(str, value);
					}

					if (converter.is_valid()) {
						Ref<Resource> res = converter->convert(missing_resource, ResourceInfo::LoadType::REAL_LOAD, 2, &err);
						res->set_path_cache(missing_resource->get_path());
						r_variant = res;
					} else if (Object::cast_to<RefCounted>(obj)) {
						Ref<RefCounted> ref = Ref<RefCounted>(Object::cast_to<RefCounted>(obj));
						r_variant = ref;
					} else {
						r_variant = obj;
					}
				}
			}

		} break;
		case V2Type::INPUT_EVENT: {
			// Not stored in code files, but is stored in project config
			Error err = InputEventParserV2::decode_input_event(r_variant, buf, len, r_len);
			ERR_FAIL_COND_V_MSG(err, err, "Failed to decode Input Event");
		} break;
		case V2Type::DICTIONARY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			//  bool shared = count&0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}

			Dictionary d;

			for (int i = 0; i < count; i++) {
				Variant key, value;

				int used;
				Error err = decode_variant_2(key, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				err = decode_variant_2(value, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				d[key] = value;
			}

			r_variant = d;

		} break;
		case V2Type::ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			//  bool shared = count&0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}

			Array varr;

			for (int i = 0; i < count; i++) {
				int used = 0;
				Variant v;
				Error err = decode_variant_2(v, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");
				buf += used;
				len -= used;
				varr.push_back(v);
				if (r_len) {
					(*r_len) += used;
				}
			}

			r_variant = varr;

		} break;

		// arrays
		case V2Type::RAW_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_COND_V(count < 0 || count > len, ERR_INVALID_DATA);

			Vector<uint8_t> data;

			if (count) {
				data.resize(count);
				uint8_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = buf[i];
				}
			}

			r_variant = data;

			if (r_len) {
				if (count % 4) {
					(*r_len) += 4 - count % 4;
				}
				(*r_len) += 4 + count;
			}

		} break;
		case V2Type::INT_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<int32_t> data;

			if (count) {
				//const int*rbuf=(const int*)buf;
				data.resize(count);
				int32_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_uint32(&buf[i * 4]);
				}
			}
			r_variant = Variant(data);
			if (r_len) {
				(*r_len) += 4 + count * sizeof(int32_t);
			}

		} break;
		case V2Type::REAL_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<float> data;

			if (count) {
				//const float*rbuf=(const float*)buf;
				data.resize(count);
				float *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_float(&buf[i * 4]);
				}
			}
			r_variant = data;

			if (r_len) {
				(*r_len) += 4 + count * sizeof(float);
			}

		} break;
		case V2Type::STRING_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);

			Vector<String> strings;
			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}
			//printf("string count: %i\n",count);

			for (int32_t i = 0; i < count; i++) {
				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				strings.push_back(str);
			}

			r_variant = strings;

		} break;
		case V2Type::VECTOR2_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 2, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 2 > len, ERR_INVALID_DATA);
			Vector<Vector2> varray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				varray.resize(count);
				Vector2 *w = varray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].x = decode_float(buf + i * 4 * 2 + 4 * 0);
					w[i].y = decode_float(buf + i * 4 * 2 + 4 * 1);
				}

				int adv = 4 * 2 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = varray;

		} break;
		case V2Type::VECTOR3_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 3, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 3 > len, ERR_INVALID_DATA);

			Vector<Vector3> varray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				varray.resize(count);
				Vector3 *w = varray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].x = decode_float(buf + i * 4 * 3 + 4 * 0);
					w[i].y = decode_float(buf + i * 4 * 3 + 4 * 1);
					w[i].z = decode_float(buf + i * 4 * 3 + 4 * 2);
				}

				int adv = 4 * 3 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = varray;

		} break;
		case V2Type::COLOR_ARRAY: {
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 4 > len, ERR_INVALID_DATA);

			Vector<Color> carray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				carray.resize(count);
				Color *w = carray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].r = decode_float(buf + i * 4 * 4 + 4 * 0);
					w[i].g = decode_float(buf + i * 4 * 4 + 4 * 1);
					w[i].b = decode_float(buf + i * 4 * 4 + 4 * 2);
					w[i].a = decode_float(buf + i * 4 * 4 + 4 * 3);
				}

				int adv = 4 * 4 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = carray;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}

Error VariantDecoderCompat::decode_variant_compat(int ver_major, Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_objects) {
	if (ver_major <= 2) {
		return decode_variant_2(r_variant, p_buffer, p_len, r_len, p_allow_objects);
	} else if (ver_major == 3) {
		return decode_variant_3(r_variant, p_buffer, p_len, r_len, p_allow_objects);
	}
	return decode_variant_4(r_variant, p_buffer, p_len, r_len, p_allow_objects);
}

static void _encode_string(const String &p_string, uint8_t *&buf, int &r_len) {
	CharString utf8 = p_string.utf8();

	if (buf) {
		encode_uint32(utf8.length(), buf);
		buf += 4;
		memcpy(buf, utf8.get_data(), utf8.length());
		buf += utf8.length();
	}

	r_len += 4 + utf8.length();
	while (r_len % 4) {
		r_len++; //pad
		if (buf) {
			*(buf++) = 0;
		}
	}
}

Error VariantDecoderCompat::encode_variant_3(const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects, int p_depth) {
	ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, ERR_OUT_OF_MEMORY, "Potential infinite recursion detected. Bailing.");
	uint8_t *buf = r_buffer;

	r_len = 0;

	uint32_t flags = 0;

	switch (p_variant.get_type()) {
		case Variant::INT: {
			int64_t val = p_variant;
			if (val > (int64_t)INT_MAX || val < (int64_t)INT_MIN) {
				flags |= ENCODE_FLAG_64;
			}
		} break;
		case Variant::FLOAT: {
			double d = p_variant;
			float f = d;
			if (double(f) != d) {
				flags |= ENCODE_FLAG_64; //always encode real as double
			}
		} break;
		case Variant::OBJECT: {
			// Test for potential wrong values sent by the debugger when it breaks or freed objects.
			Object *obj = p_variant;
			if (!obj) {
				// Object is invalid, send a NULL instead.
				if (buf) {
					encode_uint32(Variant::NIL, buf);
				}
				r_len += 4;
				return OK;
			}
			if (!p_full_objects) {
				flags |= ENCODE_FLAG_OBJECT_AS_ID;
			}
		} break;
		default: {
		} // nothing to do at this stage
	}

	if (buf) {
		int old_type = convert_variant_type_to_old(p_variant.get_type(), 3);
		encode_uint32(old_type | flags, buf);
		buf += 4;
	}
	r_len += 4;

	switch (p_variant.get_type()) {
		case Variant::NIL: {
			//nothing to do
		} break;
		case Variant::BOOL: {
			if (buf) {
				encode_uint32(p_variant.operator bool(), buf);
			}

			r_len += 4;

		} break;
		case Variant::INT: {
			if (flags & ENCODE_FLAG_64) {
				//64 bits
				if (buf) {
					encode_uint64(p_variant.operator int64_t(), buf);
				}

				r_len += 8;
			} else {
				if (buf) {
					encode_uint32(p_variant.operator int32_t(), buf);
				}

				r_len += 4;
			}
		} break;
		case Variant::FLOAT: {
			if (flags & ENCODE_FLAG_64) {
				if (buf) {
					encode_double(p_variant.operator double(), buf);
				}

				r_len += 8;

			} else {
				if (buf) {
					encode_float(p_variant.operator float(), buf);
				}

				r_len += 4;
			}

		} break;
		case Variant::NODE_PATH: {
			NodePath np = p_variant;
			if (buf) {
				encode_uint32(uint32_t(np.get_name_count()) | 0x80000000, buf); //for compatibility with the old format
				encode_uint32(np.get_subname_count(), buf + 4);
				uint32_t np_flags = 0;
				if (np.is_absolute()) {
					np_flags |= 1;
				}

				encode_uint32(np_flags, buf + 8);

				buf += 12;
			}

			r_len += 12;

			int total = np.get_name_count() + np.get_subname_count();

			for (int i = 0; i < total; i++) {
				String str;

				if (i < np.get_name_count()) {
					str = np.get_name(i);
				} else {
					str = np.get_subname(i - np.get_name_count());
				}

				CharString utf8 = str.utf8();

				int pad = 0;

				if (utf8.length() % 4) {
					pad = 4 - utf8.length() % 4;
				}

				if (buf) {
					encode_uint32(utf8.length(), buf);
					buf += 4;
					memcpy(buf, utf8.get_data(), utf8.length());
					buf += pad + utf8.length();
				}

				r_len += 4 + utf8.length() + pad;
			}

		} break;
		case Variant::STRING: {
			_encode_string(p_variant, buf, r_len);

		} break;

		// math types
		case Variant::VECTOR2: {
			if (buf) {
				Vector2 v2 = p_variant;
				encode_float(v2.x, &buf[0]);
				encode_float(v2.y, &buf[4]);
			}

			r_len += 2 * 4;

		} break; // 5
		case Variant::RECT2: {
			if (buf) {
				Rect2 r2 = p_variant;
				encode_float(r2.position.x, &buf[0]);
				encode_float(r2.position.y, &buf[4]);
				encode_float(r2.size.x, &buf[8]);
				encode_float(r2.size.y, &buf[12]);
			}
			r_len += 4 * 4;

		} break;
		case Variant::VECTOR3: {
			if (buf) {
				Vector3 v3 = p_variant;
				encode_float(v3.x, &buf[0]);
				encode_float(v3.y, &buf[4]);
				encode_float(v3.z, &buf[8]);
			}

			r_len += 3 * 4;

		} break;
		case Variant::TRANSFORM2D: {
			if (buf) {
				Transform2D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 2; j++) {
						memcpy(&buf[(i * 2 + j) * 4], &val.columns[i][j], sizeof(float));
					}
				}
			}

			r_len += 6 * 4;

		} break;
		case Variant::PLANE: {
			if (buf) {
				Plane p = p_variant;
				encode_float(p.normal.x, &buf[0]);
				encode_float(p.normal.y, &buf[4]);
				encode_float(p.normal.z, &buf[8]);
				encode_float(p.d, &buf[12]);
			}

			r_len += 4 * 4;

		} break;
		case Variant::QUATERNION: {
			if (buf) {
				Quaternion q = p_variant;
				encode_float(q.x, &buf[0]);
				encode_float(q.y, &buf[4]);
				encode_float(q.z, &buf[8]);
				encode_float(q.w, &buf[12]);
			}

			r_len += 4 * 4;

		} break;
		case Variant::AABB: {
			if (buf) {
				AABB aabb = p_variant;
				encode_float(aabb.position.x, &buf[0]);
				encode_float(aabb.position.y, &buf[4]);
				encode_float(aabb.position.z, &buf[8]);
				encode_float(aabb.size.x, &buf[12]);
				encode_float(aabb.size.y, &buf[16]);
				encode_float(aabb.size.z, &buf[20]);
			}

			r_len += 6 * 4;

		} break;
		case Variant::BASIS: {
			if (buf) {
				Basis val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						memcpy(&buf[(i * 3 + j) * 4], &val.rows[i][j], sizeof(float));
					}
				}
			}

			r_len += 9 * 4;

		} break;
		case Variant::TRANSFORM3D: {
			if (buf) {
				Transform3D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						memcpy(&buf[(i * 3 + j) * 4], &val.basis.rows[i][j], sizeof(float));
					}
				}

				encode_float(val.origin.x, &buf[36]);
				encode_float(val.origin.y, &buf[40]);
				encode_float(val.origin.z, &buf[44]);
			}

			r_len += 12 * 4;

		} break;

		// misc types
		case Variant::COLOR: {
			if (buf) {
				Color c = p_variant;
				encode_float(c.r, &buf[0]);
				encode_float(c.g, &buf[4]);
				encode_float(c.b, &buf[8]);
				encode_float(c.a, &buf[12]);
			}

			r_len += 4 * 4;

		} break;
		case Variant::RID: {
		} break;

		case Variant::OBJECT: {
			if (p_full_objects) {
				Object *obj = p_variant;
				if (!obj) {
					if (buf) {
						encode_uint32(0, buf);
					}
					r_len += 4;

				} else {
					_encode_string(ResourceCompatLoader::object_get_class_name_for_saving(obj), buf, r_len);
					Ref<MissingResource> missing_resource;
					auto converter = ResourceCompatLoader::get_converter_for_type(obj->get_save_class(), 3);
					if (converter.is_valid()) {
						if (converter->has_convert_back()) {
							Error err;
							missing_resource = converter->convert_back(Ref<Resource>(obj), 3, &err);
							obj = missing_resource.ptr();
						} else {
							WARN_PRINT("No convert_back for " + obj->get_save_class());
							// do nothing
						}
					}

					List<PropertyInfo> props;
					obj->get_property_list(&props);

					int pc = 0;
					for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
						if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}
						pc++;
					}

					if (buf) {
						encode_uint32(pc, buf);
						buf += 4;
					}

					r_len += 4;

					for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
						if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}
						String name = E->get().name;
						_encode_string(name, buf, r_len);

						int len;
						Error err = encode_variant_3(obj->get(E->get().name), buf, len, p_full_objects, p_depth + 1);
						ERR_FAIL_COND_V(err, err);
						ERR_FAIL_COND_V(len % 4, ERR_BUG);
						r_len += len;
						if (buf) {
							buf += len;
						}
					}
				}
			} else {
				if (buf) {
					Object *obj = p_variant;
					ObjectID id;
					if (obj) {
						id = obj->get_instance_id();
					}

					encode_uint64(id, buf);
				}

				r_len += 8;
			}

		} break;
		case Variant::DICTIONARY: {
			Dictionary d = p_variant;

			if (buf) {
				encode_uint32(uint32_t(d.size()), buf);
				buf += 4;
			}
			r_len += 4;

			LocalVector<Variant> keys = d.get_key_list();

			for (const Variant &E : keys) {
				/*
				CharString utf8 = E->->utf8();

				if (buf) {
					encode_uint32(utf8.length()+1,buf);
					buf+=4;
					memcpy(buf,utf8.get_data(),utf8.length()+1);
				}

				r_len+=4+utf8.length()+1;
				while (r_len%4)
					r_len++; //pad
				*/
				Variant *v = d.getptr(E);
				int len;
				Error err = encode_variant_3(v ? E : Variant("[Deleted Object]"), buf, len, p_full_objects, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
				err = encode_variant_3(v ? *v : Variant(), buf, len, p_full_objects, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
			}

		} break;
		case Variant::ARRAY: {
			Array v = p_variant;

			if (buf) {
				encode_uint32(uint32_t(v.size()), buf);
				buf += 4;
			}

			r_len += 4;

			for (int i = 0; i < v.size(); i++) {
				int len;
				Error err = encode_variant_3(v.get(i), buf, len, p_full_objects, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
			}

		} break;
		// arrays
		case Variant::PACKED_BYTE_ARRAY: {
			Vector<uint8_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(uint8_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				memcpy(buf, data.ptr(), datalen * datasize);
				buf += datalen * datasize;
			}

			r_len += 4 + datalen * datasize;
			while (r_len % 4) {
				r_len++;
				if (buf) {
					*(buf++) = 0;
				}
			}

		} break;
		case Variant::PACKED_INT64_ARRAY: {
			Vector<int64_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int32_t); // always store as 32-bit ints

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				for (int i = 0; i < datalen; i++) {
					encode_uint32(data[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		// compat, if real_t is double
		case Variant::PACKED_FLOAT64_ARRAY: {
			Vector<double> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(double);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				for (int i = 0; i < datalen; i++) {
					encode_float(data[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			Vector<int> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int32_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				for (int i = 0; i < datalen; i++) {
					encode_uint32(data[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			Vector<float> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(float);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				for (int i = 0; i < datalen; i++) {
					encode_float(data[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			Vector<String> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			for (int i = 0; i < len; i++) {
				CharString utf8 = data.get(i).utf8();

				if (buf) {
					encode_uint32(utf8.length() + 1, buf);
					buf += 4;
					memcpy(buf, utf8.get_data(), utf8.length() + 1);
					buf += utf8.length() + 1;
				}

				r_len += 4 + utf8.length() + 1;
				while (r_len % 4) {
					r_len++; //pad
					if (buf) {
						*(buf++) = 0;
					}
				}
			}

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			Vector<Vector2> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector2 v = data.get(i);

					encode_float(v.x, &buf[0]);
					encode_float(v.y, &buf[4]);
					buf += 4 * 2;
				}
			}

			r_len += 4 * 2 * len;

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			Vector<Vector3> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector3 v = data.get(i);

					encode_float(v.x, &buf[0]);
					encode_float(v.y, &buf[4]);
					encode_float(v.z, &buf[8]);
					buf += 4 * 3;
				}
			}

			r_len += 4 * 3 * len;

		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			Vector<Color> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Color c = data.get(i);

					encode_float(c.r, &buf[0]);
					encode_float(c.g, &buf[4]);
					encode_float(c.b, &buf[8]);
					encode_float(c.a, &buf[12]);
					buf += 4 * 4;
				}
			}

			r_len += 4 * 4 * len;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}

Error VariantDecoderCompat::encode_variant_2(const Variant &p_variant, uint8_t *r_buffer, int &r_len) {
	uint8_t *buf = r_buffer;
	r_len = 0;
	if (p_variant.get_type() == Variant::OBJECT) {
		Ref<InputEvent> ie = p_variant;
		Ref<Image> image = p_variant;

		// V2 InputEvent variants
		if (ie.is_valid()) {
			if (buf) {
				encode_uint32(V2Type::INPUT_EVENT, buf);
				buf += 4;
			}
			r_len += 4;
			int llen = InputEventParserV2::encode_input_event(ie, buf);

			if (buf) {
				buf += llen;
			}
			r_len += llen;
			return OK;
		} else if (image.is_valid()) { // V2 Image variants
			if (buf) {
				encode_uint32(V2Type::IMAGE, buf);
				buf += 4;
			}
			r_len += 4;

			Vector<uint8_t> data = image->get_data();

			if (buf) {
				encode_uint32(ImageEnumCompat::convert_image_format_enum_v4_to_v2(image->get_format()), &buf[0]);
				encode_uint32(image->get_mipmap_count(), &buf[4]);
				encode_uint32(image->get_width(), &buf[8]);
				encode_uint32(image->get_height(), &buf[12]);
				int ds = data.size();
				encode_uint32(ds, &buf[16]);
				if (ds > 0) {
					memcpy(&buf[20], data.ptr(), ds);
				}
			}

			int pad = 0;
			if (data.size() % 4) {
				pad = 4 - data.size() % 4;
			}

			r_len += data.size() + 5 * 4 + pad;
			return OK;
		}
		// else fall through
	}

	if (buf) {
		encode_uint32(convert_variant_type_to_old(p_variant.get_type(), 2), buf);
		buf += 4;
	}
	r_len += 4;

	switch (p_variant.get_type()) {
		case Variant::NIL: {
			//nothing to do
		} break;
		case Variant::BOOL: {
			if (buf) {
				encode_uint32(p_variant.operator bool(), buf);
			}

			r_len += 4;

		} break;
		case Variant::INT: {
			if (buf) {
				encode_uint32(p_variant.operator int(), buf);
			}

			r_len += 4;

		} break;
		case Variant::FLOAT: {
			if (buf) {
				encode_float(p_variant.operator float(), buf);
			}

			r_len += 4;

		} break;
		case Variant::NODE_PATH: {
			NodePath np = p_variant;
			int property_idx = -1;
			uint16_t snc = np.get_subname_count(); // this is the canonical subname count for v2 variants
			// If there is a property, decrement the subname counter and store the property idx.
			if (np.get_subname_count() >= 1) {
				property_idx = np.get_subname_count() - 1;
				snc--;
			}

			if (buf) {
				encode_uint32(uint32_t(np.get_name_count()) | 0x80000000, buf); //for compatibility with the old format
				encode_uint32(snc, buf + 4);
				uint32_t flags = 0;
				if (np.is_absolute()) {
					flags |= 1;
				}
				if (property_idx != -1) {
					flags |= 2;
				}

				encode_uint32(flags, buf + 8);

				buf += 12;
			}

			r_len += 12;

			int total = np.get_name_count() + snc;
			if (property_idx != -1) {
				total++;
			}

			for (int i = 0; i < total; i++) {
				String str;

				if (i < np.get_name_count()) {
					str = np.get_name(i);
				} else if (i < np.get_name_count() + snc) {
					str = np.get_subname(i - np.get_name_count());
				} else { // property
					str = np.get_subname(property_idx);
				}

				_encode_string(str, buf, r_len);
			}

		} break;
		case Variant::STRING: {
			_encode_string(p_variant, buf, r_len);
		} break;

		// math types
		case Variant::VECTOR2: {
			if (buf) {
				Vector2 v2 = p_variant;
				encode_float(v2.x, &buf[0]);
				encode_float(v2.y, &buf[4]);
			}

			r_len += 2 * 4;

		} break; // 5
		case Variant::RECT2: {
			if (buf) {
				Rect2 r2 = p_variant;
				encode_float(r2.position.x, &buf[0]);
				encode_float(r2.position.y, &buf[4]);
				encode_float(r2.size.x, &buf[8]);
				encode_float(r2.size.y, &buf[12]);
			}
			r_len += 4 * 4;

		} break;
		case Variant::VECTOR3: {
			if (buf) {
				Vector3 v3 = p_variant;
				encode_float(v3.x, &buf[0]);
				encode_float(v3.y, &buf[4]);
				encode_float(v3.z, &buf[8]);
			}

			r_len += 3 * 4;

		} break;
		case Variant::TRANSFORM2D: {
			if (buf) {
				Transform2D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 2; j++) {
						memcpy(&buf[(i * 2 + j) * 4], &val.columns[i][j], sizeof(float));
					}
				}
			}

			r_len += 6 * 4;

		} break;
		case Variant::PLANE: {
			if (buf) {
				Plane p = p_variant;
				encode_float(p.normal.x, &buf[0]);
				encode_float(p.normal.y, &buf[4]);
				encode_float(p.normal.z, &buf[8]);
				encode_float(p.d, &buf[12]);
			}

			r_len += 4 * 4;

		} break;
		case Variant::QUATERNION: {
			if (buf) {
				Quaternion q = p_variant;
				encode_float(q.x, &buf[0]);
				encode_float(q.y, &buf[4]);
				encode_float(q.z, &buf[8]);
				encode_float(q.w, &buf[12]);
			}

			r_len += 4 * 4;

		} break;
		case Variant::AABB: {
			if (buf) {
				AABB aabb = p_variant;
				encode_float(aabb.position.x, &buf[0]);
				encode_float(aabb.position.y, &buf[4]);
				encode_float(aabb.position.z, &buf[8]);
				encode_float(aabb.size.x, &buf[12]);
				encode_float(aabb.size.y, &buf[16]);
				encode_float(aabb.size.z, &buf[20]);
			}

			r_len += 6 * 4;

		} break;
		case Variant::BASIS: {
			if (buf) {
				Basis val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						memcpy(&buf[(i * 3 + j) * 4], &val.rows[i][j], sizeof(float));
					}
				}
			}

			r_len += 9 * 4;

		} break;
		case Variant::TRANSFORM3D: {
			if (buf) {
				Transform3D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						memcpy(&buf[(i * 3 + j) * 4], &val.basis.rows[i][j], sizeof(float));
					}
				}

				encode_float(val.origin.x, &buf[36]);
				encode_float(val.origin.y, &buf[40]);
				encode_float(val.origin.z, &buf[44]);
			}

			r_len += 12 * 4;

		} break;

		// misc types
		case Variant::COLOR: {
			if (buf) {
				Color c = p_variant;
				encode_float(c.r, &buf[0]);
				encode_float(c.g, &buf[4]);
				encode_float(c.b, &buf[8]);
				encode_float(c.a, &buf[12]);
			}

			r_len += 4 * 4;

		} break;
		/*case Variant::RESOURCE: {

			ERR_EXPLAIN("Can't marshallize resources");
			ERR_FAIL_V(ERR_INVALID_DATA); //no, i'm sorry, no go
		} break;*/
		case Variant::RID:
		case Variant::OBJECT: {
			// nothing to do
		} break;
		case Variant::DICTIONARY: {
			Dictionary d = p_variant;

			if (buf) {
				// compat: No dictionaries parsed from text were ever instanced as shared in 2.x
				// encode_uint32(uint32_t(d.size()) | (d.is_shared() ? 0x80000000 : 0), buf);
				encode_uint32(uint32_t(d.size()), buf);
				buf += 4;
			}
			r_len += 4;

			LocalVector<Variant> keys = d.get_key_list();

			for (const Variant &E : keys) {
				/*
				CharString utf8 = E->->utf8();

				if (buf) {
					encode_uint32(utf8.length()+1,buf);
					buf+=4;
					memcpy(buf,utf8.get_data(),utf8.length()+1);
				}

				r_len+=4+utf8.length()+1;
				while (r_len%4)
					r_len++; //pad
				*/
				int len;
				encode_variant_2(E, buf, len);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
				encode_variant_2(d[E], buf, len);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
			}

		} break;
		case Variant::ARRAY: {
			Array v = p_variant;

			if (buf) {
				// compat: No arrays parsed from text were ever instanced as shared in 2.x
				// encode_uint32(uint32_t(v.size()) | (v.is_shared() ? 0x80000000 : 0), buf);
				encode_uint32(uint32_t(v.size()), buf);
				buf += 4;
			}

			r_len += 4;

			for (int i = 0; i < v.size(); i++) {
				int len;
				encode_variant_2(v.get(i), buf, len);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
			}

		} break;
		// arrays
		case Variant::PACKED_BYTE_ARRAY: {
			Vector<uint8_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(uint8_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				memcpy(buf, data.ptr(), datalen * datasize);
			}

			r_len += 4 + datalen * datasize;
			while (r_len % 4) {
				r_len++;
			}

		} break;
		// compat
		case Variant::PACKED_INT64_ARRAY: {
			Vector<int64_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int32_t); // always store as 32-bit ints

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				for (int i = 0; i < datalen; i++) {
					encode_uint32(data[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			Vector<int32_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int32_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				for (int i = 0; i < datalen; i++) {
					encode_uint32(data[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		// compat, if real_t is double
		// TODO: Marshalls.cpp from 2.x encodes a real_t-as-double array as a double array,
		// but always DECODES it as a float array. This is probably a bug in the original implementation.
		// Guess we'll just reproduce it....
		case Variant::PACKED_FLOAT64_ARRAY: {
			Vector<double> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(double);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				for (int i = 0; i < datalen; i++) {
					encode_float(data[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			Vector<float> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(float);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				for (int i = 0; i < datalen; i++) {
					encode_float(data[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			Vector<String> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			for (int i = 0; i < len; i++) {
				CharString utf8 = data.get(i).utf8();

				if (buf) {
					encode_uint32(utf8.length() + 1, buf);
					buf += 4;
					memcpy(buf, utf8.get_data(), utf8.length() + 1);
					buf += utf8.length() + 1;
				}

				r_len += 4 + utf8.length() + 1;
				while (r_len % 4) {
					r_len++; //pad
					if (buf) {
						*(buf++) = 0;
					}
				}
			}

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			Vector<Vector2> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector2 v = data.get(i);

					encode_float(v.x, &buf[0]);
					encode_float(v.y, &buf[4]);
					buf += 4 * 2;
				}
			}

			r_len += 4 * 2 * len;

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			Vector<Vector3> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector3 v = data.get(i);

					encode_float(v.x, &buf[0]);
					encode_float(v.y, &buf[4]);
					encode_float(v.z, &buf[8]);
					buf += 4 * 3;
				}
			}

			r_len += 4 * 3 * len;

		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			Vector<Color> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Color c = data.get(i);

					encode_float(c.r, &buf[0]);
					encode_float(c.g, &buf[4]);
					encode_float(c.b, &buf[8]);
					encode_float(c.a, &buf[12]);
					buf += 4 * 4;
				}
			}

			r_len += 4 * 4 * len;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}

static void _encode_container_type_header(const ContainerType &p_type, uint32_t &header, uint32_t p_shift, bool p_full_objects) {
	if (p_type.variant_type != Variant::NIL) {
		if (p_type.script.is_valid()) {
			header |= (p_full_objects ? CONTAINER_TYPE_KIND_SCRIPT : CONTAINER_TYPE_KIND_CLASS_NAME) << p_shift;
		} else if (p_type.class_name != StringName()) {
			header |= CONTAINER_TYPE_KIND_CLASS_NAME << p_shift;
		} else {
			// No need to check `p_full_objects` since `class_name` should be non-empty for `variant_type == Variant::OBJECT`.
			header |= CONTAINER_TYPE_KIND_BUILTIN << p_shift;
		}
	}
}

static Error _encode_container_type(const ContainerType &p_type, uint8_t *&buf, int &r_len, bool p_full_objects) {
	if (p_type.variant_type != Variant::NIL) {
		if (p_type.script.is_valid()) {
			if (p_full_objects) {
				String path = p_type.script->get_path();
				ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://"), ERR_UNAVAILABLE, "Failed to encode a path to a custom script for a container type.");
				_encode_string(path, buf, r_len);
			} else {
				_encode_string(EncodedObjectAsID::get_class_static(), buf, r_len);
			}
		} else if (p_type.class_name != StringName()) {
			_encode_string(p_full_objects ? p_type.class_name : EncodedObjectAsID::get_class_static(), buf, r_len);
		} else {
			// No need to check `p_full_objects` since `class_name` should be non-empty for `variant_type == Variant::OBJECT`.
			if (buf) {
				encode_uint32(p_type.variant_type, buf);
				buf += 4;
			}
			r_len += 4;
		}
	}
	return OK;
}

static inline unsigned int _encode_real(real_t p_real, uint8_t *p_arr, bool p_real_t_is_double) {
	if (p_real_t_is_double) {
		return encode_double(p_real, p_arr);
	}
	return encode_float(p_real, p_arr);
}

Error VariantDecoderCompat::encode_variant_4(const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects, bool p_real_t_is_double, int p_depth) {
	ERR_FAIL_COND_V_MSG(p_depth > Variant::MAX_RECURSION_DEPTH, ERR_OUT_OF_MEMORY, "Potential infinite recursion detected. Bailing.");
	uint8_t *buf = r_buffer;

	r_len = 0;

	uint32_t header = p_variant.get_type();

	const int real_t_size = p_real_t_is_double ? sizeof(double) : sizeof(float);

	switch (p_variant.get_type()) {
		case Variant::INT: {
			int64_t val = p_variant;
			if (val > (int64_t)INT_MAX || val < (int64_t)INT_MIN) {
				header |= HEADER_DATA_FLAG_64;
			}
		} break;
		case Variant::FLOAT: {
			double d = p_variant;
			float f = d;
			if (double(f) != d) {
				header |= HEADER_DATA_FLAG_64;
			}
		} break;
		case Variant::OBJECT: {
			// Test for potential wrong values sent by the debugger when it breaks.
			Object *obj = p_variant.get_validated_object();
			if (!obj) {
				// Object is invalid, send a nullptr instead.
				if (buf) {
					encode_uint32(Variant::NIL, buf);
				}
				r_len += 4;
				return OK;
			}

			if (!p_full_objects) {
				header |= HEADER_DATA_FLAG_OBJECT_AS_ID;
			}
		} break;
		case Variant::DICTIONARY: {
			const Dictionary dict = p_variant;
			_encode_container_type_header(dict.get_key_type(), header, HEADER_DATA_FIELD_TYPED_DICTIONARY_KEY_SHIFT, p_full_objects);
			_encode_container_type_header(dict.get_value_type(), header, HEADER_DATA_FIELD_TYPED_DICTIONARY_VALUE_SHIFT, p_full_objects);
		} break;
		case Variant::ARRAY: {
			const Array array = p_variant;
			_encode_container_type_header(array.get_element_type(), header, HEADER_DATA_FIELD_TYPED_ARRAY_SHIFT, p_full_objects);
		} break;
		case Variant::VECTOR2:
		case Variant::VECTOR3:
		case Variant::VECTOR4:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_VECTOR4_ARRAY:
		case Variant::TRANSFORM2D:
		case Variant::TRANSFORM3D:
		case Variant::PROJECTION:
		case Variant::QUATERNION:
		case Variant::PLANE:
		case Variant::BASIS:
		case Variant::RECT2:
		case Variant::AABB: {
			//#ifdef REAL_T_IS_DOUBLE
			if (p_real_t_is_double) {
				header |= HEADER_DATA_FLAG_64;
			}
		} break;
		default: {
			// Nothing to do at this stage.
		} break;
	}

	if (buf) {
		encode_uint32(header, buf);
		buf += 4;
	}
	r_len += 4;

	switch (p_variant.get_type()) {
		case Variant::NIL: {
			// Nothing to do.
		} break;
		case Variant::BOOL: {
			if (buf) {
				encode_uint32(p_variant.operator bool(), buf);
			}

			r_len += 4;

		} break;
		case Variant::INT: {
			if (header & HEADER_DATA_FLAG_64) {
				// 64 bits.
				if (buf) {
					encode_uint64(p_variant.operator uint64_t(), buf);
				}

				r_len += 8;
			} else {
				if (buf) {
					encode_uint32(p_variant.operator uint32_t(), buf);
				}

				r_len += 4;
			}
		} break;
		case Variant::FLOAT: {
			if (header & HEADER_DATA_FLAG_64) {
				if (buf) {
					encode_double(p_variant.operator double(), buf);
				}

				r_len += 8;

			} else {
				if (buf) {
					encode_float(p_variant.operator float(), buf);
				}

				r_len += 4;
			}

		} break;
		case Variant::NODE_PATH: {
			NodePath np = p_variant;
			if (buf) {
				encode_uint32(uint32_t(np.get_name_count()) | 0x80000000, buf); // For compatibility with the old format.
				encode_uint32(np.get_subname_count(), buf + 4);
				uint32_t np_flags = 0;
				if (np.is_absolute()) {
					np_flags |= 1;
				}

				encode_uint32(np_flags, buf + 8);

				buf += 12;
			}

			r_len += 12;

			int total = np.get_name_count() + np.get_subname_count();

			for (int i = 0; i < total; i++) {
				String str;

				if (i < np.get_name_count()) {
					str = np.get_name(i);
				} else {
					str = np.get_subname(i - np.get_name_count());
				}

				CharString utf8 = str.utf8();

				int pad = 0;

				if (utf8.length() % 4) {
					pad = 4 - utf8.length() % 4;
				}

				if (buf) {
					encode_uint32(utf8.length(), buf);
					buf += 4;
					memcpy(buf, utf8.get_data(), utf8.length());
					buf += utf8.length();
					memset(buf, 0, pad);
					buf += pad;
				}

				r_len += 4 + utf8.length() + pad;
			}

		} break;
		case Variant::STRING:
		case Variant::STRING_NAME: {
			_encode_string(p_variant, buf, r_len);

		} break;

		// Math types.
		case Variant::VECTOR2: {
			if (buf) {
				Vector2 v2 = p_variant;
				_encode_real(v2.x, &buf[0], p_real_t_is_double);
				_encode_real(v2.y, &buf[real_t_size], p_real_t_is_double);
			}

			r_len += 2 * real_t_size;

		} break;
		case Variant::VECTOR2I: {
			if (buf) {
				Vector2i v2 = p_variant;
				encode_uint32(v2.x, &buf[0]);
				encode_uint32(v2.y, &buf[4]);
			}

			r_len += 2 * 4;

		} break;
		case Variant::RECT2: {
			if (buf) {
				Rect2 r2 = p_variant;
				_encode_real(r2.position.x, &buf[0], p_real_t_is_double);
				_encode_real(r2.position.y, &buf[real_t_size], p_real_t_is_double);
				_encode_real(r2.size.x, &buf[real_t_size * 2], p_real_t_is_double);
				_encode_real(r2.size.y, &buf[real_t_size * 3], p_real_t_is_double);
			}
			r_len += 4 * real_t_size;

		} break;
		case Variant::RECT2I: {
			if (buf) {
				Rect2i r2 = p_variant;
				encode_uint32(r2.position.x, &buf[0]);
				encode_uint32(r2.position.y, &buf[4]);
				encode_uint32(r2.size.x, &buf[8]);
				encode_uint32(r2.size.y, &buf[12]);
			}
			r_len += 4 * 4;

		} break;
		case Variant::VECTOR3: {
			if (buf) {
				Vector3 v3 = p_variant;
				_encode_real(v3.x, &buf[0], p_real_t_is_double);
				_encode_real(v3.y, &buf[real_t_size], p_real_t_is_double);
				_encode_real(v3.z, &buf[real_t_size * 2], p_real_t_is_double);
			}

			r_len += 3 * real_t_size;

		} break;
		case Variant::VECTOR3I: {
			if (buf) {
				Vector3i v3 = p_variant;
				encode_uint32(v3.x, &buf[0]);
				encode_uint32(v3.y, &buf[4]);
				encode_uint32(v3.z, &buf[8]);
			}

			r_len += 3 * 4;

		} break;
		case Variant::TRANSFORM2D: {
			if (buf) {
				Transform2D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 2; j++) {
						memcpy(&buf[(i * 2 + j) * real_t_size], &val.columns[i][j], real_t_size);
					}
				}
			}

			r_len += 6 * real_t_size;

		} break;
		case Variant::VECTOR4: {
			if (buf) {
				Vector4 v4 = p_variant;
				_encode_real(v4.x, &buf[0], p_real_t_is_double);
				_encode_real(v4.y, &buf[real_t_size], p_real_t_is_double);
				_encode_real(v4.z, &buf[real_t_size * 2], p_real_t_is_double);
				_encode_real(v4.w, &buf[real_t_size * 3], p_real_t_is_double);
			}

			r_len += 4 * real_t_size;

		} break;
		case Variant::VECTOR4I: {
			if (buf) {
				Vector4i v4 = p_variant;
				encode_uint32(v4.x, &buf[0]);
				encode_uint32(v4.y, &buf[4]);
				encode_uint32(v4.z, &buf[8]);
				encode_uint32(v4.w, &buf[12]);
			}

			r_len += 4 * 4;

		} break;
		case Variant::PLANE: {
			if (buf) {
				Plane p = p_variant;
				_encode_real(p.normal.x, &buf[0], p_real_t_is_double);
				_encode_real(p.normal.y, &buf[real_t_size], p_real_t_is_double);
				_encode_real(p.normal.z, &buf[real_t_size * 2], p_real_t_is_double);
				_encode_real(p.d, &buf[real_t_size * 3], p_real_t_is_double);
			}

			r_len += 4 * real_t_size;

		} break;
		case Variant::QUATERNION: {
			if (buf) {
				Quaternion q = p_variant;
				_encode_real(q.x, &buf[0], p_real_t_is_double);
				_encode_real(q.y, &buf[real_t_size], p_real_t_is_double);
				_encode_real(q.z, &buf[real_t_size * 2], p_real_t_is_double);
				_encode_real(q.w, &buf[real_t_size * 3], p_real_t_is_double);
			}

			r_len += 4 * real_t_size;

		} break;
		case Variant::AABB: {
			if (buf) {
				AABB aabb = p_variant;
				_encode_real(aabb.position.x, &buf[0], p_real_t_is_double);
				_encode_real(aabb.position.y, &buf[real_t_size], p_real_t_is_double);
				_encode_real(aabb.position.z, &buf[real_t_size * 2], p_real_t_is_double);
				_encode_real(aabb.size.x, &buf[real_t_size * 3], p_real_t_is_double);
				_encode_real(aabb.size.y, &buf[real_t_size * 4], p_real_t_is_double);
				_encode_real(aabb.size.z, &buf[real_t_size * 5], p_real_t_is_double);
			}

			r_len += 6 * real_t_size;

		} break;
		case Variant::BASIS: {
			if (buf) {
				Basis val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						_encode_real(val.rows[i][j], &buf[(i * 3 + j) * real_t_size], p_real_t_is_double);
					}
				}
			}

			r_len += 9 * real_t_size;

		} break;
		case Variant::TRANSFORM3D: {
			if (buf) {
				Transform3D val = p_variant;
				for (int i = 0; i < 3; i++) {
					for (int j = 0; j < 3; j++) {
						_encode_real(val.basis.rows[i][j], &buf[(i * 3 + j) * real_t_size], p_real_t_is_double);
					}
				}

				_encode_real(val.origin.x, &buf[real_t_size * 9], p_real_t_is_double);
				_encode_real(val.origin.y, &buf[real_t_size * 10], p_real_t_is_double);
				_encode_real(val.origin.z, &buf[real_t_size * 11], p_real_t_is_double);
			}

			r_len += 12 * real_t_size;

		} break;
		case Variant::PROJECTION: {
			if (buf) {
				Projection val = p_variant;
				for (int i = 0; i < 4; i++) {
					for (int j = 0; j < 4; j++) {
						_encode_real(val.columns[i][j], &buf[(i * 4 + j) * real_t_size], p_real_t_is_double);
					}
				}
			}

			r_len += 16 * real_t_size;

		} break;

		// Misc types.
		case Variant::COLOR: {
			if (buf) {
				Color c = p_variant;
				encode_float(c.r, &buf[0]);
				encode_float(c.g, &buf[4]);
				encode_float(c.b, &buf[8]);
				encode_float(c.a, &buf[12]);
			}

			r_len += 4 * 4; // Colors should always be in single-precision.

		} break;
		case Variant::RID: {
			RID rid = p_variant;

			if (buf) {
				encode_uint64(rid.get_id(), buf);
			}
			r_len += 8;
		} break;
		case Variant::OBJECT: {
			if (p_full_objects) {
				Object *obj = p_variant;
				if (!obj) {
					if (buf) {
						encode_uint32(0, buf);
					}
					r_len += 4;

				} else {
					ERR_FAIL_COND_V(!ClassDB::can_instantiate(obj->get_class()), ERR_INVALID_PARAMETER);

					_encode_string(ResourceCompatLoader::object_get_class_name_for_saving(obj), buf, r_len);

					List<PropertyInfo> props;
					obj->get_property_list(&props);

					int pc = 0;
					for (const PropertyInfo &E : props) {
						if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}
						pc++;
					}

					if (buf) {
						encode_uint32(pc, buf);
						buf += 4;
					}

					r_len += 4;

					for (const PropertyInfo &E : props) {
						if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
							continue;
						}

						_encode_string(E.name, buf, r_len);

						Variant value;

						if (E.name == CoreStringName(script)) {
							Ref<Script> script = obj->get_script();
							if (script.is_valid()) {
								String path = script->get_path();
								ERR_FAIL_COND_V_MSG(path.is_empty() || !path.begins_with("res://"), ERR_UNAVAILABLE, "Failed to encode a path to a custom script.");
								value = path;
							}
						} else {
							value = obj->get(E.name);
						}

						int len;
						Error err = encode_variant_4(value, buf, len, p_full_objects, p_real_t_is_double, p_depth + 1);
						ERR_FAIL_COND_V(err, err);
						ERR_FAIL_COND_V(len % 4, ERR_BUG);
						r_len += len;
						if (buf) {
							buf += len;
						}
					}
				}
			} else {
				if (buf) {
					Object *obj = p_variant.get_validated_object();
					ObjectID id;
					if (obj) {
						id = obj->get_instance_id();
					}

					encode_uint64(id, buf);
				}

				r_len += 8;
			}

		} break;
		case Variant::CALLABLE: {
		} break;
		case Variant::SIGNAL: {
			Signal signal = p_variant;

			_encode_string(signal.get_name(), buf, r_len);

			if (buf) {
				encode_uint64(signal.get_object_id(), buf);
			}
			r_len += 8;
		} break;
		case Variant::DICTIONARY: {
			const Dictionary dict = p_variant;

			{
				Error err = _encode_container_type(dict.get_key_type(), buf, r_len, p_full_objects);
				if (err) {
					return err;
				}
			}

			{
				Error err = _encode_container_type(dict.get_value_type(), buf, r_len, p_full_objects);
				if (err) {
					return err;
				}
			}

			if (buf) {
				encode_uint32(uint32_t(dict.size()), buf);
				buf += 4;
			}
			r_len += 4;

			for (const KeyValue<Variant, Variant> &kv : dict) {
				int len;
				Error err = encode_variant_4(kv.key, buf, len, p_full_objects, p_real_t_is_double, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
				err = encode_variant_4(kv.value, buf, len, p_full_objects, p_real_t_is_double, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				r_len += len;
				if (buf) {
					buf += len;
				}
			}

		} break;
		case Variant::ARRAY: {
			const Array array = p_variant;

			{
				Error err = _encode_container_type(array.get_element_type(), buf, r_len, p_full_objects);
				if (err) {
					return err;
				}
			}

			if (buf) {
				encode_uint32(uint32_t(array.size()), buf);
				buf += 4;
			}
			r_len += 4;

			for (const Variant &elem : array) {
				int len;
				Error err = encode_variant_4(elem, buf, len, p_full_objects, p_real_t_is_double, p_depth + 1);
				ERR_FAIL_COND_V(err, err);
				ERR_FAIL_COND_V(len % 4, ERR_BUG);
				if (buf) {
					buf += len;
				}
				r_len += len;
			}

		} break;

		// Packed arrays.
		case Variant::PACKED_BYTE_ARRAY: {
			Vector<uint8_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(uint8_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const uint8_t *r = data.ptr();
				if (r) {
					memcpy(buf, &r[0], datalen * datasize);
					buf += datalen * datasize;
				}
			}

			r_len += 4 + datalen * datasize;
			while (r_len % 4) {
				r_len++;
				if (buf) {
					*(buf++) = 0;
				}
			}

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			Vector<int32_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int32_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const int32_t *r = data.ptr();
				for (int32_t i = 0; i < datalen; i++) {
					encode_uint32(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_INT64_ARRAY: {
			Vector<int64_t> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(int64_t);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const int64_t *r = data.ptr();
				for (int64_t i = 0; i < datalen; i++) {
					encode_uint64(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			Vector<float> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(float);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const float *r = data.ptr();
				for (int i = 0; i < datalen; i++) {
					encode_float(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			Vector<double> data = p_variant;
			int datalen = data.size();
			int datasize = sizeof(double);

			if (buf) {
				encode_uint32(datalen, buf);
				buf += 4;
				const double *r = data.ptr();
				for (int i = 0; i < datalen; i++) {
					encode_double(r[i], &buf[i * datasize]);
				}
			}

			r_len += 4 + datalen * datasize;

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			Vector<String> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			for (int i = 0; i < len; i++) {
				CharString utf8 = data.get(i).utf8();

				if (buf) {
					encode_uint32(utf8.length() + 1, buf);
					buf += 4;
					memcpy(buf, utf8.get_data(), utf8.length() + 1);
					buf += utf8.length() + 1;
				}

				r_len += 4 + utf8.length() + 1;
				while (r_len % 4) {
					r_len++; // Pad.
					if (buf) {
						*(buf++) = 0;
					}
				}
			}

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			Vector<Vector2> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector2 v = data.get(i);

					_encode_real(v.x, &buf[0], p_real_t_is_double);
					_encode_real(v.y, &buf[real_t_size], p_real_t_is_double);
					buf += real_t_size * 2;
				}
			}

			r_len += real_t_size * 2 * len;

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			Vector<Vector3> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector3 v = data.get(i);

					_encode_real(v.x, &buf[0], p_real_t_is_double);
					_encode_real(v.y, &buf[real_t_size], p_real_t_is_double);
					_encode_real(v.z, &buf[real_t_size * 2], p_real_t_is_double);
					buf += real_t_size * 3;
				}
			}

			r_len += real_t_size * 3 * len;

		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			Vector<Color> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Color c = data.get(i);

					encode_float(c.r, &buf[0]);
					encode_float(c.g, &buf[4]);
					encode_float(c.b, &buf[8]);
					encode_float(c.a, &buf[12]);
					buf += 4 * 4; // Colors should always be in single-precision.
				}
			}

			r_len += 4 * 4 * len;

		} break;
		case Variant::PACKED_VECTOR4_ARRAY: {
			Vector<Vector4> data = p_variant;
			int len = data.size();

			if (buf) {
				encode_uint32(len, buf);
				buf += 4;
			}

			r_len += 4;

			if (buf) {
				for (int i = 0; i < len; i++) {
					Vector4 v = data.get(i);

					_encode_real(v.x, &buf[0], p_real_t_is_double);
					_encode_real(v.y, &buf[real_t_size], p_real_t_is_double);
					_encode_real(v.z, &buf[real_t_size * 2], p_real_t_is_double);
					_encode_real(v.w, &buf[real_t_size * 3], p_real_t_is_double);
					buf += real_t_size * 4;
				}
			}

			r_len += real_t_size * 4 * len;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}
	return OK;
}

Error VariantDecoderCompat::encode_variant_compat(int ver_major, const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects, bool p_real_t_is_double, int p_depth) {
	if (ver_major <= 2) {
		return encode_variant_2(p_variant, r_buffer, r_len);
	} else if (ver_major == 3) {
		return encode_variant_3(p_variant, r_buffer, r_len, p_full_objects, p_depth);
	}
	return encode_variant_4(p_variant, r_buffer, r_len, p_full_objects, p_real_t_is_double, p_depth);
}
