/**************************************************************************/
/*  resource_format_binary.cpp                                            */
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

#include "resource_compat_binary.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access_compressed.h"
#include "core/io/image.h"
#include "core/io/marshalls.h"
#include "core/io/missing_resource.h"
#include "core/object/script_language.h"
#include "core/version.h"

#include "core/io/resource_format_binary.h"

#include "compat/image_parser_v2.h"
#include "scene/resources/packed_scene.h"
#include "utility/gdre_settings.h"

//#define print_bl(m_what) print_line(m_what)
#define print_bl(m_what) (void)(m_what)

enum {
	//numbering must be different from variant, in case new variant types are added (variant must be always contiguous for jumptable optimization)
	VARIANT_NIL = 1,
	VARIANT_BOOL = 2,
	VARIANT_INT = 3,
	VARIANT_FLOAT = 4,
	VARIANT_STRING = 5,
	VARIANT_VECTOR2 = 10,
	VARIANT_RECT2 = 11,
	VARIANT_VECTOR3 = 12,
	VARIANT_PLANE = 13,
	VARIANT_QUATERNION = 14,
	VARIANT_AABB = 15,
	VARIANT_BASIS = 16,
	VARIANT_TRANSFORM3D = 17,
	VARIANT_TRANSFORM2D = 18,
	VARIANT_COLOR = 20,
	VARIANT_IMAGE = 21,
	VARIANT_NODE_PATH = 22,
	VARIANT_RID = 23,
	VARIANT_OBJECT = 24,
	VARIANT_INPUT_EVENT = 25,
	VARIANT_DICTIONARY = 26,
	VARIANT_ARRAY = 30,
	VARIANT_PACKED_BYTE_ARRAY = 31,
	VARIANT_PACKED_INT32_ARRAY = 32,
	VARIANT_PACKED_FLOAT32_ARRAY = 33,
	VARIANT_PACKED_STRING_ARRAY = 34,
	VARIANT_PACKED_VECTOR3_ARRAY = 35,
	VARIANT_PACKED_COLOR_ARRAY = 36,
	VARIANT_PACKED_VECTOR2_ARRAY = 37,
	VARIANT_INT64 = 40,
	VARIANT_DOUBLE = 41,
	VARIANT_CALLABLE = 42,
	VARIANT_SIGNAL = 43,
	VARIANT_STRING_NAME = 44,
	VARIANT_VECTOR2I = 45,
	VARIANT_RECT2I = 46,
	VARIANT_VECTOR3I = 47,
	VARIANT_PACKED_INT64_ARRAY = 48,
	VARIANT_PACKED_FLOAT64_ARRAY = 49,
	VARIANT_VECTOR4 = 50,
	VARIANT_VECTOR4I = 51,
	VARIANT_PROJECTION = 52,
	OBJECT_EMPTY = 0,
	OBJECT_EXTERNAL_RESOURCE = 1,
	OBJECT_INTERNAL_RESOURCE = 2,
	OBJECT_EXTERNAL_RESOURCE_INDEX = 3,
	// Version 2: added 64 bits support for float and int.
	// Version 3: changed nodepath encoding.
	// Version 4: new string ID for ext/subresources, breaks forward compat.
	// Version 5: Ability to store script class in the header.
	FORMAT_VERSION = 5,
	FORMAT_VERSION_CAN_RENAME_DEPS = 1,
	FORMAT_VERSION_NO_NODEPATH_PROPERTY = 3,
};

void ResourceLoaderCompatBinary::_advance_padding(uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); //pad to 32
		}
	}
}

static Error read_reals(real_t *dst, Ref<FileAccess> &f, size_t count) {
	if (f->real_is_double) {
		if constexpr (sizeof(real_t) == 8) {
			// Ideal case with double-precision
			f->get_buffer((uint8_t *)dst, count * sizeof(double));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint64_t *dst = (uint64_t *)dst;
				for (size_t i = 0; i < count; i++) {
					dst[i] = BSWAP64(dst[i]);
				}
			}
#endif
		} else if constexpr (sizeof(real_t) == 4) {
			// May be slower, but this is for compatibility. Eventually the data should be converted.
			for (size_t i = 0; i < count; ++i) {
				dst[i] = f->get_double();
			}
		} else {
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "real_t size is neither 4 nor 8!");
		}
	} else {
		if constexpr (sizeof(real_t) == 4) {
			// Ideal case with float-precision
			f->get_buffer((uint8_t *)dst, count * sizeof(float));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *dst = (uint32_t *)dst;
				for (size_t i = 0; i < count; i++) {
					dst[i] = BSWAP32(dst[i]);
				}
			}
#endif
		} else if constexpr (sizeof(real_t) == 8) {
			for (size_t i = 0; i < count; ++i) {
				dst[i] = f->get_float();
			}
		} else {
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "real_t size is neither 4 nor 8!");
		}
	}
	return OK;
}

StringName ResourceLoaderCompatBinary::_get_string() {
	uint32_t id = f->get_32();
	if (id & 0x80000000) {
		uint32_t len = id & 0x7FFFFFFF;
		if ((int)len > str_buf.size()) {
			str_buf.resize(len);
		}
		if (len == 0) {
			return StringName();
		}
		f->get_buffer((uint8_t *)&str_buf[0], len);
		String s;
		s.parse_utf8(&str_buf[0]);
		return s;
	}

	return string_map[id];
}

Error ResourceLoaderCompatBinary::parse_variant(Variant &r_v) {
	uint32_t prop_type = f->get_32();
	print_bl("find property of type: " + itos(prop_type));

	switch (prop_type) {
		case VARIANT_NIL: {
			r_v = Variant();
		} break;
		case VARIANT_BOOL: {
			r_v = bool(f->get_32());
		} break;
		case VARIANT_INT: {
			r_v = int(f->get_32());
		} break;
		case VARIANT_INT64: {
			r_v = int64_t(f->get_64());
		} break;
		case VARIANT_FLOAT: {
			if (f->real_is_double) {
				r_v = f->get_double();
			} else {
				r_v = f->get_float();
			}
		} break;
		case VARIANT_DOUBLE: {
			r_v = f->get_double();
		} break;
		case VARIANT_STRING: {
			r_v = get_unicode_string();
		} break;
		case VARIANT_VECTOR2: {
			Vector2 v;
			v.x = f->get_real();
			v.y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_VECTOR2I: {
			Vector2i v;
			v.x = f->get_32();
			v.y = f->get_32();
			r_v = v;

		} break;
		case VARIANT_RECT2: {
			Rect2 v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_RECT2I: {
			Rect2i v;
			v.position.x = f->get_32();
			v.position.y = f->get_32();
			v.size.x = f->get_32();
			v.size.y = f->get_32();
			r_v = v;

		} break;
		case VARIANT_VECTOR3: {
			Vector3 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			r_v = v;
		} break;
		case VARIANT_VECTOR3I: {
			Vector3i v;
			v.x = f->get_32();
			v.y = f->get_32();
			v.z = f->get_32();
			r_v = v;
		} break;
		case VARIANT_VECTOR4: {
			Vector4 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			v.w = f->get_real();
			r_v = v;
		} break;
		case VARIANT_VECTOR4I: {
			Vector4i v;
			v.x = f->get_32();
			v.y = f->get_32();
			v.z = f->get_32();
			v.w = f->get_32();
			r_v = v;
		} break;
		case VARIANT_PLANE: {
			Plane v;
			v.normal.x = f->get_real();
			v.normal.y = f->get_real();
			v.normal.z = f->get_real();
			v.d = f->get_real();
			r_v = v;
		} break;
		case VARIANT_QUATERNION: {
			Quaternion v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			v.w = f->get_real();
			r_v = v;

		} break;
		case VARIANT_AABB: {
			AABB v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.position.z = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			v.size.z = f->get_real();
			r_v = v;

		} break;
		case VARIANT_TRANSFORM2D: {
			Transform2D v;
			v.columns[0].x = f->get_real();
			v.columns[0].y = f->get_real();
			v.columns[1].x = f->get_real();
			v.columns[1].y = f->get_real();
			v.columns[2].x = f->get_real();
			v.columns[2].y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_BASIS: {
			Basis v;
			v.rows[0].x = f->get_real();
			v.rows[0].y = f->get_real();
			v.rows[0].z = f->get_real();
			v.rows[1].x = f->get_real();
			v.rows[1].y = f->get_real();
			v.rows[1].z = f->get_real();
			v.rows[2].x = f->get_real();
			v.rows[2].y = f->get_real();
			v.rows[2].z = f->get_real();
			r_v = v;

		} break;
		case VARIANT_TRANSFORM3D: {
			Transform3D v;
			v.basis.rows[0].x = f->get_real();
			v.basis.rows[0].y = f->get_real();
			v.basis.rows[0].z = f->get_real();
			v.basis.rows[1].x = f->get_real();
			v.basis.rows[1].y = f->get_real();
			v.basis.rows[1].z = f->get_real();
			v.basis.rows[2].x = f->get_real();
			v.basis.rows[2].y = f->get_real();
			v.basis.rows[2].z = f->get_real();
			v.origin.x = f->get_real();
			v.origin.y = f->get_real();
			v.origin.z = f->get_real();
			r_v = v;
		} break;
		case VARIANT_PROJECTION: {
			Projection v;
			v.columns[0].x = f->get_real();
			v.columns[0].y = f->get_real();
			v.columns[0].z = f->get_real();
			v.columns[0].w = f->get_real();
			v.columns[1].x = f->get_real();
			v.columns[1].y = f->get_real();
			v.columns[1].z = f->get_real();
			v.columns[1].w = f->get_real();
			v.columns[2].x = f->get_real();
			v.columns[2].y = f->get_real();
			v.columns[2].z = f->get_real();
			v.columns[2].w = f->get_real();
			v.columns[3].x = f->get_real();
			v.columns[3].y = f->get_real();
			v.columns[3].z = f->get_real();
			v.columns[3].w = f->get_real();
			r_v = v;
		} break;
		case VARIANT_COLOR: {
			Color v; // Colors should always be in single-precision.
			v.r = f->get_float();
			v.g = f->get_float();
			v.b = f->get_float();
			v.a = f->get_float();
			r_v = v;

		} break;
		case VARIANT_STRING_NAME: {
			r_v = StringName(get_unicode_string());
		} break;

		// Old Godot 2.x Image variant, convert into an object
		case VARIANT_IMAGE: {
			//Have to decode the old Image variant here
			Error err = ImageParserV2::decode_image_v2(f, r_v, true);
			if (err != OK) {
				if (err == ERR_UNAVAILABLE) {
					return err;
				}
				ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Couldn't load resource: embedded image");
				//WARN_PRINT(String("Couldn't load resource: embedded image").utf8().get_data());
			}
		} break;
		case VARIANT_NODE_PATH: {
			Vector<StringName> names;
			Vector<StringName> subnames;
			bool absolute;

			int name_count = f->get_16();
			uint32_t subname_count = f->get_16();
			absolute = subname_count & 0x8000;
			subname_count &= 0x7FFF;
			// Version 2.x compatiblity.
			bool has_property = ver_format < FORMAT_VERSION_NO_NODEPATH_PROPERTY;
			if (has_property) {
				subname_count += 1; // has a property field, so we should count it as well
			}

			for (int i = 0; i < name_count; i++) {
				names.push_back(_get_string());
			}
			for (uint32_t i = 0; i < subname_count; i++) {
				subnames.push_back(_get_string());
			}
			// empty property field, remove it
			if (has_property && subnames[subnames.size() - 1] == "") {
				subnames.remove_at(subnames.size() - 1);
			}
			NodePath np = NodePath(names, subnames, absolute);

			r_v = np;

		} break;
		case VARIANT_RID: {
			r_v = f->get_32();
		} break;
		case VARIANT_OBJECT: {
			uint32_t objtype = f->get_32();

			switch (objtype) {
				case OBJECT_EMPTY: {
					//do none

				} break;
				case OBJECT_INTERNAL_RESOURCE: {
					uint32_t index = f->get_32();
					String path;

					if (using_named_scene_ids) { // New format.
						ERR_FAIL_INDEX_V((int)index, internal_resources.size(), ERR_PARSE_ERROR);
						path = internal_resources[index].path;
					} else {
						path += res_path + "::" + itos(index);
					}

					//always use internal cache for loading internal resources
					if (!internal_index_cache.has(path)) {
						WARN_PRINT(String("Couldn't load resource (no cache): " + path).utf8().get_data());
						r_v = Variant();
					} else {
						r_v = internal_index_cache[path];
					}
				} break;
				case OBJECT_EXTERNAL_RESOURCE: {
					//old file format, still around for compatibility

					String exttype = get_unicode_string();
					String path = get_unicode_string();

					if (!path.contains("://") && path.is_relative_path()) {
						// path is relative to file being loaded, so convert to a resource path
						WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
						path = GDRESettings::get_singleton()->localize_path(res_path.get_base_dir().path_join(path));
					}

					if (remaps.find(path)) {
						path = remaps[path];
					}

					Ref<Resource> res = load_type != ResourceCompatLoader::REAL_LOAD ? CompatFormatLoader::create_missing_external_resource(path, exttype, ResourceUID::INVALID_ID) : ResourceLoader::load(path, exttype, cache_mode);

					if (res.is_null()) {
						WARN_PRINT(String("Couldn't load resource: " + path).utf8().get_data());
					}
					r_v = res;

				} break;
				case OBJECT_EXTERNAL_RESOURCE_INDEX: {
					//new file format, just refers to an index in the external list
					int erindex = f->get_32();

					if (erindex < 0 || erindex >= external_resources.size()) {
						WARN_PRINT("Broken external resource! (index out of size)");
						r_v = Variant();
					} else {
						Ref<ResourceLoader::LoadToken> &load_token = external_resources.write[erindex].load_token;
						if (load_token.is_valid()) { // If not valid, it's OK since then we know this load accepts broken dependencies.
							Error err;
							Ref<Resource> res = finish_ext_load(load_token, &err);
							if (res.is_null()) {
								if (load_type != ResourceCompatLoader::REAL_LOAD) {
									error = ERR_FILE_MISSING_DEPENDENCIES;
									ERR_FAIL_V_MSG(error, "WE SHOULD NEVER GET HERE!!!!!!!!!!!!!!!!!!!  : Can't load dependency: " + external_resources[erindex].path + ".");
								}
								if (!ResourceLoader::is_cleaning_tasks()) {
									if (!ResourceLoader::get_abort_on_missing_resources()) {
										ResourceLoader::notify_dependency_error(local_path, external_resources[erindex].path, external_resources[erindex].type);
									} else {
										error = ERR_FILE_MISSING_DEPENDENCIES;
										ERR_FAIL_V_MSG(error, "Can't load dependency: " + external_resources[erindex].path + ".");
									}
								}
							} else {
								r_v = res;
							}
						}
					}
				} break;
				default: {
					ERR_FAIL_V(ERR_FILE_CORRUPT);
				} break;
			}
		} break;

		// Old Godot 2.x InputEvent variant
		// They were never saved into the binary resource files, we will only encounter the type number
		case VARIANT_INPUT_EVENT: {
			WARN_PRINT("Encountered a Input event variant, someone screwed up when exporting this project");
		} break;
		case VARIANT_CALLABLE: {
			r_v = Callable();
		} break;
		case VARIANT_SIGNAL: {
			r_v = Signal();
		} break;

		case VARIANT_DICTIONARY: {
			uint32_t len = f->get_32();
			Dictionary d; //last bit means shared
			len &= 0x7FFFFFFF;
			for (uint32_t i = 0; i < len; i++) {
				Variant key;
				Error err = parse_variant(key);
				if (err == ERR_UNAVAILABLE) {
					return err;
				}
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				Variant value;
				err = parse_variant(value);
				if (err == ERR_UNAVAILABLE) {
					return err;
				}
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				d[key] = value;
			}
			r_v = d;
		} break;
		case VARIANT_ARRAY: {
			uint32_t len = f->get_32();
			Array a; //last bit means shared
			len &= 0x7FFFFFFF;
			a.resize(len);
			for (uint32_t i = 0; i < len; i++) {
				Variant val;
				Error err = parse_variant(val);
				if (err == ERR_UNAVAILABLE) {
					return err;
				}
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				a[i] = val;
			}
			r_v = a;

		} break;
		case VARIANT_PACKED_BYTE_ARRAY: {
			uint32_t len = f->get_32();

			Vector<uint8_t> array;
			array.resize(len);
			uint8_t *w = array.ptrw();
			f->get_buffer(w, len);
			_advance_padding(len);

			r_v = array;

		} break;
		case VARIANT_PACKED_INT32_ARRAY: {
			uint32_t len = f->get_32();

			Vector<int32_t> array;
			array.resize(len);
			int32_t *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(int32_t));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VARIANT_PACKED_INT64_ARRAY: {
			uint32_t len = f->get_32();

			Vector<int64_t> array;
			array.resize(len);
			int64_t *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(int64_t));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint64_t *ptr = (uint64_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP64(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VARIANT_PACKED_FLOAT32_ARRAY: {
			uint32_t len = f->get_32();

			Vector<float> array;
			array.resize(len);
			float *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(float));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VARIANT_PACKED_FLOAT64_ARRAY: {
			uint32_t len = f->get_32();

			Vector<double> array;
			array.resize(len);
			double *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(double));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint64_t *ptr = (uint64_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP64(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VARIANT_PACKED_STRING_ARRAY: {
			uint32_t len = f->get_32();
			Vector<String> array;
			array.resize(len);
			String *w = array.ptrw();
			for (uint32_t i = 0; i < len; i++) {
				w[i] = get_unicode_string();
			}

			r_v = array;

		} break;
		case VARIANT_PACKED_VECTOR2_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Vector2> array;
			array.resize(len);
			Vector2 *w = array.ptrw();
			static_assert(sizeof(Vector2) == 2 * sizeof(real_t));
			const Error err = read_reals(reinterpret_cast<real_t *>(w), f, len * 2);
			ERR_FAIL_COND_V(err != OK, err);

			r_v = array;

		} break;
		case VARIANT_PACKED_VECTOR3_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Vector3> array;
			array.resize(len);
			Vector3 *w = array.ptrw();
			static_assert(sizeof(Vector3) == 3 * sizeof(real_t));
			const Error err = read_reals(reinterpret_cast<real_t *>(w), f, len * 3);
			ERR_FAIL_COND_V(err != OK, err);

			r_v = array;

		} break;
		case VARIANT_PACKED_COLOR_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Color> array;
			array.resize(len);
			Color *w = array.ptrw();
			// Colors always use `float` even with double-precision support enabled
			static_assert(sizeof(Color) == 4 * sizeof(float));
			f->get_buffer((uint8_t *)w, len * sizeof(float) * 4);
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len * 4; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		default: {
			ERR_FAIL_V(ERR_FILE_CORRUPT);
		} break;
	}

	return OK; //never reach anyway
}

Ref<Resource> ResourceLoaderCompatBinary::get_resource() {
	return resource;
}

Error ResourceLoaderCompatBinary::load() {
	if (error != OK) {
		return error;
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String path = external_resources[i].path;

		if (remaps.has(path)) {
			path = remaps[path];
		}

		if (!path.contains("://") && path.is_relative_path()) {
			// path is relative to file being loaded, so convert to a resource path
			WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
			path = GDRESettings::get_singleton()->localize_path(path.get_base_dir().path_join(external_resources[i].path));
		}

		external_resources.write[i].path = path; //remap happens here, not on load because on load it can actually be used for filesystem dock resource remap
		if (load_type != ResourceCompatLoader::REAL_LOAD) {
			Ref<ResourceLoader::LoadToken> token{ memnew(ResourceLoader::LoadToken) };
			token->local_path = path;
			token->user_path = path;
			token->res_if_unregistered = CompatFormatLoader::create_missing_external_resource(path, external_resources[i].type, external_resources[i].uid, itos(i));
			external_resources.write[i].load_token = token;
			continue;
		} // Else, if real load....
		external_resources.write[i].load_token = ResourceLoader::_load_start(path, external_resources[i].type, use_sub_threads ? ResourceLoader::LOAD_THREAD_DISTRIBUTE : ResourceLoader::LOAD_THREAD_FROM_CURRENT, ResourceFormatLoader::CACHE_MODE_REUSE);
		if (!external_resources[i].load_token.is_valid()) {
			if (!ResourceLoader::get_abort_on_missing_resources()) {
				ResourceLoader::notify_dependency_error(local_path, path, external_resources[i].type);
			} else {
				error = ERR_FILE_MISSING_DEPENDENCIES;
				ERR_FAIL_V_MSG(error, "Can't load dependency: " + path + ".");
			}
		}
	}

	for (int i = 0; i < internal_resources.size(); i++) {
		bool main = i == (internal_resources.size() - 1);

		//maybe it is loaded already
		String path;
		String id;

		if (!main) {
			path = internal_resources[i].path;

			if (path.begins_with("local://")) {
				path = path.replace_first("local://", "");
				id = path;
				path = res_path + "::" + path;

				internal_resources.write[i].path = path; // Update path.
			}

			// Shouldn't happen on a fake or non-global load.
			if (cache_mode == ResourceFormatLoader::CACHE_MODE_REUSE && ResourceCache::has(path)) {
				Ref<Resource> cached = ResourceCache::get_ref(path);
				if (cached.is_valid()) {
					//already loaded, don't do anything
					error = OK;
					internal_index_cache[path] = cached;
					continue;
				}
			}
		} else {
			// Shouldn't happen on a fake or non-global load.
			if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE && !ResourceCache::has(res_path)) {
				path = res_path;
			}
		}

		uint64_t offset = internal_resources[i].offset;

		f->seek(offset);

		String t = get_unicode_string();

		Ref<Resource> res;

		// Shouldn't happen on a fake or non-global load.
		if (cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE && ResourceCache::has(path)) {
			//use the existing one
			Ref<Resource> cached = ResourceCache::get_ref(path);
			if (cached->get_class() == t) {
				cached->reset_state();
				res = cached;
			}
		}

		MissingResource *missing_resource = nullptr;

		bool is_scene = false;
		// if it's a PackedScene, we need to note the version number
		if (t == "PackedScene") {
			is_scene = true;
		}
		if (load_type == ResourceCompatLoader::FAKE_LOAD) {
			missing_resource = main ? CompatFormatLoader::create_missing_main_resource(path, t, uid) : CompatFormatLoader::create_missing_internal_resource(path, t, id);
			res = Ref<Resource>(missing_resource);
		} else {
			// TODO!!!!!!!!!!!!!!!
			// TODO!!!!!!!!!!!!!!!
			// TODO: non-global loads and real loads should check if there is a loader for this resource
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Non-global loads and real loads are not supported yet.");
		}

		if (res.is_null()) {
			//did not replace

			Object *obj = ClassDB::instantiate(t);
			if (!obj) {
				if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
					//create a missing resource
					missing_resource = memnew(MissingResource);
					missing_resource->set_original_class(t);
					missing_resource->set_recording_properties(true);
					obj = missing_resource;
				} else {
					error = ERR_FILE_CORRUPT;
					ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, local_path + ":Resource of unrecognized type in file: " + t + ".");
				}
			}

			Resource *r = Object::cast_to<Resource>(obj);
			if (!r) {
				String obj_class = obj->get_class();
				error = ERR_FILE_CORRUPT;
				memdelete(obj); //bye
				ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, local_path + ":Resource type in resource field not a resource, type is: " + obj_class + ".");
			}

			res = Ref<Resource>(r);
			if (!path.is_empty() && cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
				r->set_path(path, cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE); //if got here because the resource with same path has different type, replace it
			} else if (!path.is_resource_file()) {
				r->set_path_cache(path);
			}
			r->set_scene_unique_id(id);
		}

		if (!main) {
			internal_index_cache[path] = res;
		}

		int pc = f->get_32();

		//set properties

		Dictionary missing_resource_properties;

		for (int j = 0; j < pc; j++) {
			StringName name = _get_string();

			if (name == StringName()) {
				error = ERR_FILE_CORRUPT;
				ERR_FAIL_V(ERR_FILE_CORRUPT);
			}

			Variant value;

			error = parse_variant(value);
			if (error) {
				return error;
			}

			bool set_valid = true;

			// The below won't happen on a fake load
			if (value.get_type() == Variant::OBJECT && missing_resource == nullptr && ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
				// If the property being set is a missing resource (and the parent is not),
				// then setting it will most likely not work.
				// Instead, save it as metadata.

				Ref<MissingResource> mr = value;
				if (mr.is_valid()) {
					missing_resource_properties[name] = mr;
					set_valid = false;
				}
			}

			if (value.get_type() == Variant::ARRAY) {
				Array set_array = value;
				bool is_get_valid = false;
				Variant get_value = res->get(name, &is_get_valid);
				if (is_get_valid && get_value.get_type() == Variant::ARRAY) {
					Array get_array = get_value;
					if (!set_array.is_same_typed(get_array)) {
						value = Array(set_array, get_array.get_typed_builtin(), get_array.get_typed_class_name(), get_array.get_typed_script());
					}
				}
			}

			if (is_scene && name == "_bundled") {
				Dictionary _bundled = res->get("_bundled");
				if (!main) {
					// ??????
					WARN_PRINT("PackedScene found in non-main resource?!!??!?!?!");
					// set it anyway; get the compat metadata from the res and set the packed_scene_version
					if (_bundled.has("version")) {
						Dictionary compat = res->get_meta(META_COMPAT, Dictionary());
						compat["packed_scene_version"] = (int)_bundled.get("version", -1);
						res->set_meta(META_COMPAT, compat);
					}
				} else if (_bundled.has("version")) {
					packed_scene_version = _bundled.get("version", -1);
				}
			}

			if (set_valid) {
				res->set(name, value);
			}
		}

		if (missing_resource) {
			missing_resource->set_recording_properties(false);
		}

		if (!missing_resource_properties.is_empty()) {
			res->set_meta(META_MISSING_RESOURCES, missing_resource_properties);
		}

#ifdef TOOLS_ENABLED
		res->set_edited(false);
#endif

		if (progress) {
			*progress = (i + 1) / float(internal_resources.size());
		}

		resource_cache.push_back(res);

		if (main) {
			if (ver_major == 2) {
				Error limperr = load_import_metadata();
				// if this was an error other than the metadata being unavailable...
				if (limperr != OK && limperr != ERR_UNAVAILABLE) {
					error = limperr;
					ERR_FAIL_V_MSG(error, "Failed to load");
				}
			}
			f.unref();
			resource = res;
			// skip translation remapping for fake and non-global loads
			if (!is_real_load()) {
				// One final step; we have to check to see if this is a PackedScene, and if so, set a metadata field for the packedscene version number in the `_bundled` property
				if (res->get_save_class() == "PackedScene") {
					Dictionary _bundled = res->get("_bundled");
					packed_scene_version = _bundled.get("version", -1);
				}
				set_compat_meta(res);
				error = OK;
				return OK;
			}
			set_compat_meta(res);
			resource->set_as_translation_remapped(translation_remapped);
			error = OK;
			return OK;
		}
	}

	return ERR_FILE_EOF;
}

void ResourceLoaderCompatBinary::set_translation_remapped(bool p_remapped) {
	translation_remapped = p_remapped;
}

static void save_ustring(Ref<FileAccess> f, const String &p_string) {
	CharString utf8 = p_string.utf8();
	f->store_32(utf8.length() + 1);
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

static String get_ustring(Ref<FileAccess> f) {
	int len = f->get_32();
	Vector<char> str_buf;
	str_buf.resize(len);
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0]);
	return s;
}

String ResourceLoaderCompatBinary::get_unicode_string() {
	int len = f->get_32();
	if (len > str_buf.size()) {
		str_buf.resize(len);
	}
	if (len == 0) {
		return String();
	}
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0]);
	return s;
}

void ResourceLoaderCompatBinary::get_classes_used(Ref<FileAccess> p_f, HashSet<StringName> *p_classes) {
	open(p_f, false, true);
	if (error) {
		return;
	}

	for (int i = 0; i < internal_resources.size(); i++) {
		p_f->seek(internal_resources[i].offset);
		String t = get_unicode_string();
		ERR_FAIL_COND(p_f->get_error() != OK);
		if (t != String()) {
			p_classes->insert(t);
		}
	}
}

void ResourceLoaderCompatBinary::get_dependencies(Ref<FileAccess> p_f, List<String> *p_dependencies, bool p_add_types) {
	open(p_f, false, true);
	if (error) {
		return;
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String dep;
		String fallback_path;

		if (external_resources[i].uid != ResourceUID::INVALID_ID) {
			dep = ResourceUID::get_singleton()->id_to_text(external_resources[i].uid);
			fallback_path = external_resources[i].path; // Used by Dependency Editor, in case uid path fails.
		} else {
			dep = external_resources[i].path;
		}

		if (p_add_types && !external_resources[i].type.is_empty()) {
			dep += "::" + external_resources[i].type;
		}
		if (!fallback_path.is_empty()) {
			if (!p_add_types) {
				dep += "::"; // Ensure that path comes third, even if there is no type.
			}
			dep += "::" + fallback_path;
		}

		p_dependencies->push_back(dep);
	}
}

void ResourceLoaderCompatBinary::open(Ref<FileAccess> p_f, bool p_no_resources, bool p_keep_uuid_paths) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		error = fac->open_after_magic(f);
		if (error != OK) {
			f.unref();
			ERR_FAIL_MSG("Failed to open binary resource file: " + local_path + ".");
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f.unref();
		ERR_FAIL_MSG("Unrecognized binary resource file: " + local_path + ".");
	}

	bool big_endian = f->get_32();
	bool use_real64 = f->get_32();

	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian
	stored_big_endian = big_endian;
	ver_major = f->get_32();
	ver_minor = f->get_32();
	ver_format = f->get_32();
	stored_use_real64 = use_real64;
	// Version 1.x? unlikely
	if (ver_major < 2) {
		switch (ver_format) {
			case 0:
				// Version 1.x, format 0
				// Ok, this might actually be Godot 1.x
				break;
			case 1:
				suspect_version = true;
				ver_major = 2;
				ver_minor = 0;
				break;
			case 2:
			case 3:
				suspect_version = true;
				// Godot didn't support format version 2-3 until Godot 3.x.
				ver_major = 3;
				// this is likely SCU, so we'll just put 1 for the minor version here.
				ver_minor = 1;
				break;
			case 4:
			case 5:
				suspect_version = true;
				ver_major = 4;
				break;
		}
	}

	print_bl("big endian: " + itos(big_endian));
#ifdef BIG_ENDIAN_ENABLED
	print_bl("endian swap: " + itos(!big_endian));
#else
	print_bl("endian swap: " + itos(big_endian));
#endif
	print_bl("real64: " + itos(use_real64));
	print_bl("major: " + itos(ver_major));
	print_bl("minor: " + itos(ver_minor));
	print_bl("format: " + itos(ver_format));

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		f.unref();
		ERR_FAIL_MSG(vformat("File '%s' can't be loaded, as it uses a format version (%d) or engine version (%d.%d) which are not supported by your engine version (%s).",
				local_path, ver_format, ver_major, ver_minor, VERSION_BRANCH));
	}

	type = get_unicode_string();

	print_bl("type: " + type);

	importmd_ofs = f->get_64();
	uint32_t flags = f->get_32();
	if (flags & ResourceFormatSaverCompatBinaryInstance::FORMAT_FLAG_NAMED_SCENE_IDS) {
		using_named_scene_ids = true;
	}
	if (flags & ResourceFormatSaverCompatBinaryInstance::FORMAT_FLAG_UIDS) {
		using_uids = true;
	}
	f->real_is_double = (flags & ResourceFormatSaverCompatBinaryInstance::FORMAT_FLAG_REAL_T_IS_DOUBLE) != 0;
	using_real_t_double = f->real_is_double;

	if (using_uids) {
		uid = f->get_64();
	} else {
		f->get_64(); // skip over uid field
		uid = ResourceUID::INVALID_ID;
	}

	if (flags & ResourceFormatSaverCompatBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS) {
		script_class = get_unicode_string();
		using_script_class = true;
	}

	for (int i = 0; i < ResourceFormatSaverCompatBinaryInstance::RESERVED_FIELDS; i++) {
		f->get_32(); //skip a few reserved fields
	}

	if (p_no_resources) {
		return;
	}

	uint32_t string_table_size = f->get_32();
	string_map.resize(string_table_size);
	for (uint32_t i = 0; i < string_table_size; i++) {
		StringName s = get_unicode_string();
		string_map.write[i] = s;
	}

	print_bl("strings: " + itos(string_table_size));

	uint32_t ext_resources_size = f->get_32();
	for (uint32_t i = 0; i < ext_resources_size; i++) {
		ExtResource er;
		er.type = get_unicode_string();
		er.path = get_unicode_string();
		if (using_uids) {
			er.uid = f->get_64();
#if 0
			if (!p_keep_uuid_paths && er.uid != ResourceUID::INVALID_ID) {
				if (ResourceUID::get_singleton()->has_id(er.uid)) {
					// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
					er.path = ResourceUID::get_singleton()->get_id_path(er.uid);
				} else {
#ifdef TOOLS_ENABLED
					// Silence a warning that can happen during the initial filesystem scan due to cache being regenerated.
					if (ResourceLoader::get_resource_uid(res_path) != er.uid) {
						WARN_PRINT(String(res_path + ": In external resource #" + itos(i) + ", invalid UID: " + ResourceUID::get_singleton()->id_to_text(er.uid) + " - using text path instead: " + er.path).utf8().get_data());
					}
#else
					WARN_PRINT(String(res_path + ": In external resource #" + itos(i) + ", invalid UID: " + ResourceUID::get_singleton()->id_to_text(er.uid) + " - using text path instead: " + er.path).utf8().get_data());
#endif
				}
			}
#endif
		}

		external_resources.push_back(er);
	}

	print_bl("ext resources: " + itos(ext_resources_size));
	uint32_t int_resources_size = f->get_32();

	for (uint32_t i = 0; i < int_resources_size; i++) {
		IntResource ir;
		ir.path = get_unicode_string();
		ir.offset = f->get_64();
		internal_resources.push_back(ir);
	}

	print_bl("int resources: " + itos(int_resources_size));

	if (f->eof_reached()) {
		error = ERR_FILE_CORRUPT;
		f.unref();
		ERR_FAIL_MSG("Premature end of file (EOF): " + local_path + ".");
	}
}

String ResourceLoaderCompatBinary::recognize(Ref<FileAccess> p_f) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		error = fac->open_after_magic(f);
		if (error != OK) {
			f.unref();
			return "";
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f.unref();
		return "";
	}

	bool big_endian = f->get_32();
	f->get_32(); // use_real64

	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian

	uint32_t ver_major = f->get_32();
	f->get_32(); // ver_minor
	uint32_t ver_fmt = f->get_32();

	if (ver_fmt > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		f.unref();
		return "";
	}

	return get_unicode_string();
}

String ResourceLoaderCompatBinary::recognize_script_class(Ref<FileAccess> p_f) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		error = fac->open_after_magic(f);
		if (error != OK) {
			f.unref();
			return "";
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f.unref();
		return "";
	}

	bool big_endian = f->get_32();
	f->get_32(); // use_real64

	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian

	uint32_t ver_major = f->get_32();
	f->get_32(); // ver_minor
	uint32_t ver_fmt = f->get_32();

	if (ver_fmt > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		f.unref();
		return "";
	}

	get_unicode_string(); // type

	f->get_64(); // Metadata offset
	uint32_t flags = f->get_32();
	f->get_64(); // UID

	if (flags & ResourceFormatSaverCompatBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS) {
		return get_unicode_string();
	} else {
		return String();
	}
}

Ref<Resource> ResourceFormatLoaderCompatBinary::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), "Cannot open file '" + p_path + "'.");

	ResourceLoaderCompatBinary loader;
	loader.cache_mode = p_cache_mode;
	loader.use_sub_threads = p_use_sub_threads;
	loader.progress = r_progress;
	String path = !p_original_path.is_empty() ? p_original_path : p_path;
	loader.local_path = p_path; // No need for local path, it only gets used in error messages.
	loader.res_path = loader.local_path;
	loader.open(f);

	err = loader.load();

	if (r_error) {
		*r_error = err;
	}

	if (err) {
		return Ref<Resource>();
	}
	return loader.resource;
}

void ResourceFormatLoaderCompatBinary::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (p_type.is_empty()) {
		get_recognized_extensions(p_extensions);
		return;
	}

	List<String> extensions;
	ClassDB::get_extensions_for_type(p_type, &extensions);

	extensions.sort();

	for (const String &E : extensions) {
		String ext = E.to_lower();
		p_extensions->push_back(ext);
	}
}

void ResourceFormatLoaderCompatBinary::get_recognized_extensions(List<String> *p_extensions) const {
	List<String> extensions;
	ClassDB::get_resource_base_extensions(&extensions);
	extensions.sort();

	for (const String &E : extensions) {
		String ext = E.to_lower();
		p_extensions->push_back(ext);
	}
}

bool ResourceFormatLoaderCompatBinary::handles_type(const String &p_type) const {
	return true; //handles all
}

void ResourceFormatLoaderCompatBinary::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_MSG(f.is_null(), "Cannot open file '" + p_path + "'.");

	ResourceLoaderCompatBinary loader;
	loader.local_path = p_path; // No need for local path, it only gets used in error messages.
	loader.res_path = loader.local_path;
	loader.get_dependencies(f, p_dependencies, p_add_types);
}

Error ResourceFormatLoaderCompatBinary::rename_dependencies(const String &p_path, const HashMap<String, String> &p_map) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, "Cannot open file '" + p_path + "'.");

	Ref<FileAccess> fw;

	String local_path = p_path.get_base_dir();

	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		Error err = fac->open_after_magic(f);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot open file '" + p_path + "'.");
		f = fac;

		Ref<FileAccessCompressed> facw;
		facw.instantiate();
		facw->configure("RSCC");
		err = facw->open_internal(p_path + ".depren", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Cannot create file '" + p_path + ".depren'.");

		fw = facw;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unrecognized binary resource file '" + local_path + "'.");
	} else {
		fw = FileAccess::open(p_path + ".depren", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(fw.is_null(), ERR_CANT_CREATE, "Cannot create file '" + p_path + ".depren'.");

		uint8_t magic[4] = { 'R', 'S', 'R', 'C' };
		fw->store_buffer(magic, 4);
	}

	bool big_endian = f->get_32();
	bool use_real64 = f->get_32();

	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian
#ifdef BIG_ENDIAN_ENABLED
	fw->store_32(!big_endian);
#else
	fw->store_32(big_endian);
#endif
	fw->set_big_endian(big_endian != 0);
	fw->store_32(use_real64); //use real64

	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_format = f->get_32();

	if (ver_format < FORMAT_VERSION_CAN_RENAME_DEPS) {
		fw.unref();

		{
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			da->remove(p_path + ".depren");
		}

		// Use the old approach.

		WARN_PRINT("This file is old, so it can't refactor dependencies, failing.");
		return ERR_UNAVAILABLE;
	}

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED,
				vformat("File '%s' can't be loaded, as it uses a format version (%d) or engine version (%d.%d) which are not supported by your engine version (%s).",
						local_path, ver_format, ver_major, ver_minor, VERSION_BRANCH));
	}

	// Since we're not actually converting the file contents, leave the version
	// numbers in the file untouched.
	fw->store_32(ver_major);
	fw->store_32(ver_minor);
	fw->store_32(ver_format);

	save_ustring(fw, get_ustring(f)); //type

	uint64_t md_ofs = f->get_position();
	uint64_t importmd_ofs = f->get_64();
	fw->store_64(0); //metadata offset

	uint32_t flags = f->get_32();
	bool using_uids = (flags & ResourceFormatSaverCompatBinaryInstance::FORMAT_FLAG_UIDS);
	uint64_t uid_data = f->get_64();

	fw->store_32(flags);
	fw->store_64(uid_data);
	if (flags & ResourceFormatSaverCompatBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS) {
		save_ustring(fw, get_ustring(f));
	}

	for (int i = 0; i < ResourceFormatSaverCompatBinaryInstance::RESERVED_FIELDS; i++) {
		fw->store_32(0); // reserved
		f->get_32();
	}

	//string table
	uint32_t string_table_size = f->get_32();

	fw->store_32(string_table_size);

	for (uint32_t i = 0; i < string_table_size; i++) {
		String s = get_ustring(f);
		save_ustring(fw, s);
	}

	//external resources
	uint32_t ext_resources_size = f->get_32();
	fw->store_32(ext_resources_size);
	for (uint32_t i = 0; i < ext_resources_size; i++) {
		String type = get_ustring(f);
		String path = get_ustring(f);

		ResourceUID::ID uid = ResourceUID::INVALID_ID;
		if (using_uids) {
			uid = f->get_64();
#if 0
			if (uid != ResourceUID::INVALID_ID) {
				if (ResourceUID::get_singleton()->has_id(uid)) {
					// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
					path = ResourceUID::get_singleton()->get_id_path(uid);
				}
			}
#endif
		}

		bool relative = false;
		if (!path.begins_with("res://")) {
			path = local_path.path_join(path).simplify_path();
			relative = true;
		}

		if (p_map.has(path)) {
			String np = p_map[path];
			path = np;
		}

		String full_path = path;

		if (relative) {
			//restore relative
			path = local_path.path_to_file(path);
		}

		save_ustring(fw, type);
		save_ustring(fw, path);

		if (using_uids) {
			// ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(full_path);
			fw->store_64(uid);
		}
	}

	int64_t size_diff = (int64_t)fw->get_position() - (int64_t)f->get_position();

	//internal resources
	uint32_t int_resources_size = f->get_32();
	fw->store_32(int_resources_size);

	for (uint32_t i = 0; i < int_resources_size; i++) {
		String path = get_ustring(f);
		uint64_t offset = f->get_64();
		save_ustring(fw, path);
		fw->store_64(offset + size_diff);
	}

	//rest of file
	uint8_t b = f->get_8();
	while (!f->eof_reached()) {
		fw->store_8(b);
		b = f->get_8();
	}
	f.unref();

	bool all_ok = fw->get_error() == OK;

	fw->seek(md_ofs);
	fw->store_64(importmd_ofs + size_diff);

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	fw.unref();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (da->exists(p_path + ".depren")) {
		da->remove(p_path);
		da->rename(p_path + ".depren", p_path);
	}
	return OK;
}

void ResourceFormatLoaderCompatBinary::get_classes_used(const String &p_path, HashSet<StringName> *r_classes) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_MSG(f.is_null(), "Cannot open file '" + p_path + "'.");

	ResourceLoaderCompatBinary loader;
	loader.local_path = p_path; // No need for local path, it only gets used in error messages.
	loader.res_path = loader.local_path;
	loader.get_classes_used(f, r_classes);
}

String ResourceFormatLoaderCompatBinary::get_resource_type(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ""; //could not read
	}

	ResourceLoaderCompatBinary loader;
	loader.local_path = p_path; // No need for local path, it only gets used in error messages.
	loader.res_path = loader.local_path;
	String r = loader.recognize(f);
	return ClassDB::get_compatibility_remapped_class(r);
}

String ResourceFormatLoaderCompatBinary::get_resource_script_class(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ""; //could not read
	}

	ResourceLoaderCompatBinary loader;
	loader.local_path = p_path; // No need for local path, it only gets used in error messages.
	loader.res_path = loader.local_path;
	return loader.recognize_script_class(f);
}

ResourceUID::ID ResourceFormatLoaderCompatBinary::get_resource_uid(const String &p_path) const {
	String ext = p_path.get_extension().to_lower();
	if (!ClassDB::is_resource_extension(ext)) {
		return ResourceUID::INVALID_ID;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ResourceUID::INVALID_ID; //could not read
	}

	ResourceLoaderCompatBinary loader;
	loader.local_path = p_path; // No need for local path, it only gets used in error meesges.
	loader.res_path = loader.local_path;
	loader.open(f, true);
	if (loader.error != OK) {
		return ResourceUID::INVALID_ID; //could not read
	}
	return loader.uid;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

void ResourceFormatSaverCompatBinaryInstance::_pad_buffer(Ref<FileAccess> f, int p_bytes) {
	int extra = 4 - (p_bytes % 4);
	if (extra < 4) {
		for (int i = 0; i < extra; i++) {
			f->store_8(0); //pad to 32
		}
	}
}

void ResourceFormatSaverCompatBinaryInstance::write_variant(Ref<FileAccess> f, const Variant &p_property, HashMap<Ref<Resource>, int> &resource_map, HashMap<Ref<Resource>, int> &external_resources, HashMap<StringName, int> &string_map, const PropertyInfo &p_hint) {
	switch (p_property.get_type()) {
		case Variant::NIL: {
			f->store_32(VARIANT_NIL);
			// don't store anything
		} break;
		case Variant::BOOL: {
			f->store_32(VARIANT_BOOL);
			bool val = p_property;
			f->store_32(val);
		} break;
		case Variant::INT: {
			int64_t val = p_property;
			if (val > 0x7FFFFFFF || val < -(int64_t)0x80000000) {
				f->store_32(VARIANT_INT64);
				f->store_64(val);

			} else {
				f->store_32(VARIANT_INT);
				f->store_32(int32_t(p_property));
			}

		} break;
		case Variant::FLOAT: {
			double d = p_property;
			float fl = d;
			if (double(fl) != d) {
				f->store_32(VARIANT_DOUBLE);
				f->store_double(d);
			} else {
				f->store_32(VARIANT_FLOAT);
				if (f->real_is_double) {
					f->store_double(d);
				} else {
					f->store_float(fl);
				}
			}

		} break;
		case Variant::STRING: {
			f->store_32(VARIANT_STRING);
			String val = p_property;
			save_unicode_string(f, val);

		} break;
		case Variant::VECTOR2: {
			f->store_32(VARIANT_VECTOR2);
			Vector2 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);

		} break;
		case Variant::VECTOR2I: {
			f->store_32(VARIANT_VECTOR2I);
			Vector2i val = p_property;
			f->store_32(val.x);
			f->store_32(val.y);

		} break;
		case Variant::RECT2: {
			f->store_32(VARIANT_RECT2);
			Rect2 val = p_property;
			f->store_real(val.position.x);
			f->store_real(val.position.y);
			f->store_real(val.size.x);
			f->store_real(val.size.y);

		} break;
		case Variant::RECT2I: {
			f->store_32(VARIANT_RECT2I);
			Rect2i val = p_property;
			f->store_32(val.position.x);
			f->store_32(val.position.y);
			f->store_32(val.size.x);
			f->store_32(val.size.y);

		} break;
		case Variant::VECTOR3: {
			f->store_32(VARIANT_VECTOR3);
			Vector3 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);

		} break;
		case Variant::VECTOR3I: {
			f->store_32(VARIANT_VECTOR3I);
			Vector3i val = p_property;
			f->store_32(val.x);
			f->store_32(val.y);
			f->store_32(val.z);

		} break;
		case Variant::VECTOR4: {
			f->store_32(VARIANT_VECTOR4);
			Vector4 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);
			f->store_real(val.w);

		} break;
		case Variant::VECTOR4I: {
			f->store_32(VARIANT_VECTOR4I);
			Vector4i val = p_property;
			f->store_32(val.x);
			f->store_32(val.y);
			f->store_32(val.z);
			f->store_32(val.w);

		} break;
		case Variant::PLANE: {
			f->store_32(VARIANT_PLANE);
			Plane val = p_property;
			f->store_real(val.normal.x);
			f->store_real(val.normal.y);
			f->store_real(val.normal.z);
			f->store_real(val.d);

		} break;
		case Variant::QUATERNION: {
			f->store_32(VARIANT_QUATERNION);
			Quaternion val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);
			f->store_real(val.w);

		} break;
		case Variant::AABB: {
			f->store_32(VARIANT_AABB);
			AABB val = p_property;
			f->store_real(val.position.x);
			f->store_real(val.position.y);
			f->store_real(val.position.z);
			f->store_real(val.size.x);
			f->store_real(val.size.y);
			f->store_real(val.size.z);

		} break;
		case Variant::TRANSFORM2D: {
			f->store_32(VARIANT_TRANSFORM2D);
			Transform2D val = p_property;
			f->store_real(val.columns[0].x);
			f->store_real(val.columns[0].y);
			f->store_real(val.columns[1].x);
			f->store_real(val.columns[1].y);
			f->store_real(val.columns[2].x);
			f->store_real(val.columns[2].y);

		} break;
		case Variant::BASIS: {
			f->store_32(VARIANT_BASIS);
			Basis val = p_property;
			f->store_real(val.rows[0].x);
			f->store_real(val.rows[0].y);
			f->store_real(val.rows[0].z);
			f->store_real(val.rows[1].x);
			f->store_real(val.rows[1].y);
			f->store_real(val.rows[1].z);
			f->store_real(val.rows[2].x);
			f->store_real(val.rows[2].y);
			f->store_real(val.rows[2].z);

		} break;
		case Variant::TRANSFORM3D: {
			f->store_32(VARIANT_TRANSFORM3D);
			Transform3D val = p_property;
			f->store_real(val.basis.rows[0].x);
			f->store_real(val.basis.rows[0].y);
			f->store_real(val.basis.rows[0].z);
			f->store_real(val.basis.rows[1].x);
			f->store_real(val.basis.rows[1].y);
			f->store_real(val.basis.rows[1].z);
			f->store_real(val.basis.rows[2].x);
			f->store_real(val.basis.rows[2].y);
			f->store_real(val.basis.rows[2].z);
			f->store_real(val.origin.x);
			f->store_real(val.origin.y);
			f->store_real(val.origin.z);

		} break;
		case Variant::PROJECTION: {
			f->store_32(VARIANT_PROJECTION);
			Projection val = p_property;
			f->store_real(val.columns[0].x);
			f->store_real(val.columns[0].y);
			f->store_real(val.columns[0].z);
			f->store_real(val.columns[0].w);
			f->store_real(val.columns[1].x);
			f->store_real(val.columns[1].y);
			f->store_real(val.columns[1].z);
			f->store_real(val.columns[1].w);
			f->store_real(val.columns[2].x);
			f->store_real(val.columns[2].y);
			f->store_real(val.columns[2].z);
			f->store_real(val.columns[2].w);
			f->store_real(val.columns[3].x);
			f->store_real(val.columns[3].y);
			f->store_real(val.columns[3].z);
			f->store_real(val.columns[3].w);

		} break;
		case Variant::COLOR: {
			f->store_32(VARIANT_COLOR);
			Color val = p_property;
			// Color are always floats
			f->store_float(val.r);
			f->store_float(val.g);
			f->store_float(val.b);
			f->store_float(val.a);

		} break;
		case Variant::STRING_NAME: {
			f->store_32(VARIANT_STRING_NAME);
			String val = p_property;
			save_unicode_string(f, val);

		} break;

		case Variant::NODE_PATH: {
			f->store_32(VARIANT_NODE_PATH);
			NodePath np = p_property;
			f->store_16(np.get_name_count());
			uint16_t snc = np.get_subname_count();
			int property_idx = -1;
			if (ver_format < FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
				// If there is a property, decrement the subname counter and store the property idx.
				if (np.get_subname_count() > 1 &&
						String(np.get_concatenated_subnames()).split(":").size() >= 2) {
					property_idx = np.get_subname_count() - 1;
					snc--;
				}
			}
			if (np.is_absolute()) {
				f->store_16(snc | 0x8000);
			} else {
				f->store_16(snc);
			}
			for (int i = 0; i < np.get_name_count(); i++) {
				if (string_map.has(np.get_name(i))) {
					f->store_32(string_map[np.get_name(i)]);
				} else {
					save_unicode_string(f, np.get_name(i), true);
				}
			}
			for (int i = 0; i < snc; i++) {
				if (string_map.has(np.get_subname(i))) {
					f->store_32(string_map[np.get_subname(i)]);
				} else {
					save_unicode_string(f, np.get_subname(i), true);
				}
			}
			if (ver_format < FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
				if (property_idx > -1) {
					f->store_32(string_map[np.get_subname(property_idx)]);
					// otherwise, store zero-length string
				} else {
					// 0x80000000 will resolve to a zero length string in the binary parser for any version
					uint32_t zlen = 0x80000000;
					f->store_32(zlen);
				}
			}

		} break;
		case Variant::RID: {
			f->store_32(VARIANT_RID);
			WARN_PRINT("Can't save RIDs.");
			RID val = p_property;
			f->store_32(val.get_id());
		} break;
		case Variant::OBJECT: {
			// This will only be triggered on godot 2.x, where the image variant is loaded as an image object vs. a resource
			if (ver_format <= 1) {
				Ref<Resource> resp = p_property;
				if (resp->is_class("Image")) {
					f->store_32(VARIANT_IMAGE);
					// storing lossless compressed by default
					ImageParserV2::write_image_v2_to_bin(f, p_property, true);
					break;
				} else if (resp->is_class("InputEvent")) {
					// these should never be saved to binary; just store the type
					f->store_32(VARIANT_INPUT_EVENT);
					break;
				}
			}
			f->store_32(VARIANT_OBJECT);
			Ref<Resource> res = p_property;
			if (res.is_null() || res->get_meta(SNAME("_skip_save_"), false)) {
				f->store_32(OBJECT_EMPTY);
				return; // Don't save it.
			}

			if (!res->is_built_in()) {
				f->store_32(OBJECT_EXTERNAL_RESOURCE_INDEX);
				f->store_32(external_resources[res]);
			} else {
				if (!resource_map.has(res)) {
					f->store_32(OBJECT_EMPTY);
					ERR_FAIL_MSG("Resource was not pre cached for the resource section, most likely due to circular reference.");
				}

				f->store_32(OBJECT_INTERNAL_RESOURCE);
				f->store_32(resource_map[res]);
				//internal resource
			}

		} break;
		case Variant::CALLABLE: {
			f->store_32(VARIANT_CALLABLE);
			WARN_PRINT("Can't save Callables.");
		} break;
		case Variant::SIGNAL: {
			f->store_32(VARIANT_SIGNAL);
			WARN_PRINT("Can't save Signals.");
		} break;

		case Variant::DICTIONARY: {
			f->store_32(VARIANT_DICTIONARY);
			Dictionary d = p_property;
			f->store_32(uint32_t(d.size()));

			List<Variant> keys;
			d.get_key_list(&keys);

			for (const Variant &E : keys) {
				write_variant(f, E, resource_map, external_resources, string_map);
				write_variant(f, d[E], resource_map, external_resources, string_map);
			}

		} break;
		case Variant::ARRAY: {
			f->store_32(VARIANT_ARRAY);
			Array a = p_property;
			f->store_32(uint32_t(a.size()));
			for (int i = 0; i < a.size(); i++) {
				write_variant(f, a[i], resource_map, external_resources, string_map);
			}

		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			f->store_32(VARIANT_PACKED_BYTE_ARRAY);
			Vector<uint8_t> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			const uint8_t *r = arr.ptr();
			f->store_buffer(r, len);
			_pad_buffer(f, len);

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			f->store_32(VARIANT_PACKED_INT32_ARRAY);
			Vector<int32_t> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			const int32_t *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_32(r[i]);
			}

		} break;
		case Variant::PACKED_INT64_ARRAY: {
			f->store_32(VARIANT_PACKED_INT64_ARRAY);
			Vector<int64_t> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			const int64_t *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_64(r[i]);
			}

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			f->store_32(VARIANT_PACKED_FLOAT32_ARRAY);
			Vector<float> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			const float *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_float(r[i]);
			}

		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			f->store_32(VARIANT_PACKED_FLOAT64_ARRAY);
			Vector<double> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			const double *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_double(r[i]);
			}

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			f->store_32(VARIANT_PACKED_STRING_ARRAY);
			Vector<String> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			const String *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				save_unicode_string(f, r[i]);
			}

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			f->store_32(VARIANT_PACKED_VECTOR3_ARRAY);
			Vector<Vector3> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			const Vector3 *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].x);
				f->store_real(r[i].y);
				f->store_real(r[i].z);
			}

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			f->store_32(VARIANT_PACKED_VECTOR2_ARRAY);
			Vector<Vector2> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			const Vector2 *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].x);
				f->store_real(r[i].y);
			}

		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			f->store_32(VARIANT_PACKED_COLOR_ARRAY);
			Vector<Color> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			const Color *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_float(r[i].r);
				f->store_float(r[i].g);
				f->store_float(r[i].b);
				f->store_float(r[i].a);
			}

		} break;
		default: {
			ERR_FAIL_MSG("Invalid variant.");
		}
	}
}

void ResourceFormatSaverCompatBinaryInstance::_find_resources(const Variant &p_variant, bool p_main) {
	switch (p_variant.get_type()) {
		case Variant::OBJECT: {
			Ref<Resource> res = p_variant;

			if (res.is_null() || external_resources.has(res) || res->get_meta(SNAME("_skip_save_"), false)) {
				return;
			}

			if (!p_main && (!bundle_resources) && !res->is_built_in()) {
				if (res->get_path() == path) {
					ERR_PRINT("Circular reference to resource being saved found: '" + local_path + "' will be null next time it's loaded.");
					return;
				}
				int idx = external_resources.size();
				external_resources[res] = idx;
				return;
			}

			if (resource_set.has(res)) {
				return;
			}

			resource_set.insert(res);

			List<PropertyInfo> property_list;

			res->get_property_list(&property_list);

			for (const PropertyInfo &E : property_list) {
				if (E.usage & PROPERTY_USAGE_STORAGE) {
					Variant value = res->get(E.name);
					if (E.usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
						NonPersistentKey npk;
						npk.base = res;
						npk.property = E.name;
						non_persistent_map[npk] = value;

						Ref<Resource> sres = value;
						if (sres.is_valid()) {
							resource_set.insert(sres);
							saved_resources.push_back(sres);
						} else {
							_find_resources(value);
						}
					} else {
						_find_resources(value);
					}
				}
			}

			saved_resources.push_back(res);

		} break;

		case Variant::ARRAY: {
			Array varray = p_variant;
			int len = varray.size();
			for (int i = 0; i < len; i++) {
				const Variant &v = varray.get(i);
				_find_resources(v);
			}

		} break;

		case Variant::DICTIONARY: {
			Dictionary d = p_variant;
			List<Variant> keys;
			d.get_key_list(&keys);
			for (const Variant &E : keys) {
				_find_resources(E);
				Variant v = d[E];
				_find_resources(v);
			}
		} break;
		case Variant::NODE_PATH: {
			//take the chance and save node path strings
			NodePath np = p_variant;
			for (int i = 0; i < np.get_name_count(); i++) {
				get_string_index(np.get_name(i));
			}
			for (int i = 0; i < np.get_subname_count(); i++) {
				get_string_index(np.get_subname(i));
			}

		} break;
		default: {
		}
	}
}

void ResourceFormatSaverCompatBinaryInstance::save_unicode_string(Ref<FileAccess> p_f, const String &p_string, bool p_bit_on_len) {
	CharString utf8 = p_string.utf8();
	if (p_bit_on_len) {
		p_f->store_32((utf8.length() + 1) | 0x80000000);
	} else {
		p_f->store_32(utf8.length() + 1);
	}
	p_f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

int ResourceFormatSaverCompatBinaryInstance::get_string_index(const String &p_string) {
	StringName s = p_string;
	if (string_map.has(s)) {
		return string_map[s];
	}

	string_map[s] = strings.size();
	strings.push_back(s);
	return strings.size() - 1;
}

struct ConnectionData {
	int from = 0;
	int to = 0;
	int signal = 0;
	int method = 0;
	int flags = 0;
	int unbinds = 0;
	Vector<int> binds;
};

Dictionary ResourceFormatSaverCompatBinaryInstance::fix_scene_bundle(const Ref<PackedScene> &p_scene, int original_version) {
	Dictionary bundled = p_scene->get("_bundled");
	int ver = bundled.get("version", -1);
	if (ver > 3) {
		ERR_FAIL_V_MSG(bundled, "THEY INCREASED THE PACKED SCENE VERSION AGAIN!!!!!! REPORT THIS!!!!!!!!!!!!!!!!!!!!!!!!!!!");
	}

	uint64_t conn_count = p_scene->get_state()->get_connection_count();
	bool requires_version_3 = false;
	for (int i = 0; i < conn_count; i++) {
		if (p_scene->get_state()->get_connection_unbinds(i) > 0) {
			requires_version_3 = true;
			break;
		}
	}
	if (requires_version_3) {
		return bundled;
	}
	Dictionary ret = bundled.duplicate(true);
	PackedInt32Array conns = ret["conns"];
	PackedInt32Array newconns;
	newconns.resize(conns.size() - conn_count); // minus unbind property * count
	int newidx = 0;
	int idx = 0;
	for (int i = 0; i < conn_count; i++) {
		newconns.write[newidx++] = conns[idx++]; //from
		newconns.write[newidx++] = conns[idx++]; //to
		newconns.write[newidx++] = conns[idx++]; //signal
		newconns.write[newidx++] = conns[idx++]; //method
		newconns.write[newidx++] = conns[idx++]; //flags
		int bindcnt = conns[idx++]; //binds
		newconns.write[newidx++] = bindcnt;
		for (int j = 0; j < bindcnt; j++) {
			newconns.write[newidx++] = conns[idx++]; //binds
		}
		// skip the unbind
		idx++;
	}
	ret["conns"] = newconns;
	ret["version"] = original_version > 0 ? original_version : 2; // default to 2

	return ret;
}

static String _resource_get_class(Ref<Resource> p_resource) {
	Ref<MissingResource> missing_resource = p_resource;
	if (missing_resource.is_valid()) {
		return missing_resource->get_original_class();
	} else {
		return p_resource->get_class();
	}
}

/* this is really only appropriate for saving fake-loaded resources right now; don't use it to save anything else*/
Error ResourceFormatSaverCompatBinaryInstance::save(const String &p_path, const Ref<Resource> &p_resource, uint32_t p_flags) {
	Error err;
	Ref<FileAccess> f;
	if (p_flags & ResourceSaver::FLAG_COMPRESS) {
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		fac->configure("RSCC");
		f = fac;
		err = fac->open_internal(p_path, FileAccess::WRITE);
	} else {
		f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot create file '" + p_path + "'.");

	// get metadata from the resource
	Dictionary compat = p_resource->get_meta(META_COMPAT, Dictionary());
	if (compat.is_empty()) {
		WARN_PRINT("Resource does not have compat metadata set?!?!?!?!");
		ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Resource does not have compat metadata set?!?!?!?!");
	}
	ver_format = compat.get("ver_format", 0);
	ver_major = compat.get("ver_major", 0);
	ver_minor = compat.get("ver_minor", 0);
	String format = compat.get("resource_format", "binary");
	using_script_class = compat.get("using_script_class", false);
	script_class = compat.get("script_class", String());
	big_endian = compat.get("stored_big_endian", false);
	using_uids = compat.get("using_uids", false);
	using_named_scene_ids = compat.get("using_named_scene_ids", false);
	using_real_t_double = compat.get("using_real_t_double", false);
	stored_use_real64 = compat.get("stored_use_real64", false);
	Ref<ResourceImportMetadatav2> imd = compat.get("import_metadata", Ref<ResourceImportMetadatav2>());
	ResourceUID::ID uid = compat.get("uid", ResourceUID::INVALID_ID);
	if (format != "binary") { // text
		if (!script_class.is_empty()) {
			using_script_class = true;
			ver_format = 5;
		} else if (using_named_scene_ids || ver_major == 4) {
			// If we're using named_scene_ids, it's version 4
			ver_format = 4;
			// else go by engine major version
		} else if (ver_major == 3) {
			ver_format = 3;
		} else {
			ver_format = 1;
		}
	}

	if (using_real_t_double) {
		f->real_is_double = true;
	}

#if 0
	relative_paths = p_flags & ResourceSaver::FLAG_RELATIVE_PATHS;
	skip_editor = p_flags & ResourceSaver::FLAG_OMIT_EDITOR_PROPERTIES;
	bundle_resources = p_flags & ResourceSaver::FLAG_BUNDLE_RESOURCES;
	big_endian = p_flags & ResourceSaver::FLAG_SAVE_BIG_ENDIAN;
	takeover_paths = p_flags & ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;
#endif
	if (!p_path.begins_with("res://")) {
		takeover_paths = false;
	}

	local_path = p_path.get_base_dir();
	// TODO: VERIFY THIS!!!
	path = output_dir.path_join(p_path.replace("res://", ""));

	_find_resources(p_resource, true);

	if (!(p_flags & ResourceSaver::FLAG_COMPRESS)) {
		//save header compressed
		static const uint8_t header[4] = { 'R', 'S', 'R', 'C' };
		f->store_buffer(header, 4);
	}

	if (big_endian) {
		f->store_32(1);
		f->set_big_endian(true);
	} else {
		f->store_32(0);
	}

	f->store_32(stored_use_real64 ? 1 : 0); //64 bits file
	f->store_32(ver_major);
	f->store_32(ver_minor);
	f->store_32(ver_format);

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	save_unicode_string(f, _resource_get_class(p_resource));
	uint64_t md_at = f->get_position();

	f->store_64(0); //offset to import metadata

	uint32_t format_flags = 0;
	if (using_named_scene_ids) {
		format_flags |= FORMAT_FLAG_NAMED_SCENE_IDS;
	}
	if (using_uids) {
		format_flags |= FORMAT_FLAG_UIDS;
	}
	if (using_script_class && !script_class.is_empty()) {
		format_flags |= FORMAT_FLAG_HAS_SCRIPT_CLASS;
	}
	if (using_real_t_double) {
		format_flags |= FORMAT_FLAG_REAL_T_IS_DOUBLE;
	}
#if 0
	String script_class;
	{
		uint32_t format_flags = FORMAT_FLAG_NAMED_SCENE_IDS | FORMAT_FLAG_UIDS;
#ifdef REAL_T_IS_DOUBLE
		format_flags |= FORMAT_FLAG_REAL_T_IS_DOUBLE;
#endif
		if (!p_resource->is_class("PackedScene")) {
			Ref<Script> s = p_resource->get_script();
			if (s.is_valid()) {
				script_class = s->get_global_name();
				if (!script_class.is_empty()) {
					format_flags |= ResourceFormatSaverBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS;
				}
			}
		}

		f->store_32(format_flags);
	}
	ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(p_path, true);
#endif
	f->store_32(format_flags);
	f->store_64(uid);
	if (using_script_class && !script_class.is_empty()) {
		save_unicode_string(f, script_class);
	}

	for (int i = 0; i < ResourceFormatSaverCompatBinaryInstance::RESERVED_FIELDS; i++) {
		f->store_32(0); // reserved
	}

	List<ResourceData> resources;

	{
		for (const Ref<Resource> &E : saved_resources) {
			Dictionary missing_resource_properties = E->get_meta(META_MISSING_RESOURCES, Dictionary());

			ResourceData &rd = resources.push_back(ResourceData())->get();
			rd.type = _resource_get_class(E);

			List<PropertyInfo> property_list;
			E->get_property_list(&property_list);

			for (const PropertyInfo &F : property_list) {
				if (skip_editor && F.name.begins_with("__editor")) {
					continue;
				}
				if (F.name == META_PROPERTY_COMPAT_DATA) {
					continue;
				}
				if (F.name == META_PROPERTY_MISSING_RESOURCES) {
					continue;
				}

				if ((F.usage & PROPERTY_USAGE_STORAGE) || missing_resource_properties.has(F.name)) {
					Property p;
					p.name_idx = get_string_index(F.name);

					if (F.usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
						NonPersistentKey npk;
						npk.base = E;
						npk.property = F.name;
						if (non_persistent_map.has(npk)) {
							p.value = non_persistent_map[npk];
						}
					} else {
						p.value = E->get(F.name);
					}

					if (F.type == Variant::OBJECT && missing_resource_properties.has(F.name)) {
						// Was this missing resource overridden? If so do not save the old value.
						Ref<Resource> res = p.value;
						if (res.is_null()) {
							p.value = missing_resource_properties[F.name];
						}
					}

#if 0
					Variant default_value = ClassDB::class_get_default_property_value(E->get_class(), F.name);
#endif
					// save everything by default
					Variant default_value = Variant();
					// Except for the default "Resource" properties
					if (F.name == "resource_name") {
						default_value = "";
					} else if (F.name == "resource_local_to_scene") {
						default_value = false;
					}

					if (default_value.get_type() != Variant::NIL && bool(Variant::evaluate(Variant::OP_EQUAL, p.value, default_value))) {
						continue;
					}

					p.pi = F;

					rd.properties.push_back(p);
				}
			}
		}
	}

	f->store_32(strings.size()); //string table size
	for (int i = 0; i < strings.size(); i++) {
		save_unicode_string(f, strings[i]);
	}

	// save external resource table
	f->store_32(external_resources.size()); //amount of external resources
	Vector<Ref<Resource>> save_order;
	save_order.resize(external_resources.size());

	for (const KeyValue<Ref<Resource>, int> &E : external_resources) {
		save_order.write[E.value] = E.key;
	}

	for (int i = 0; i < save_order.size(); i++) {
		save_unicode_string(f, save_order[i]->get_save_class());
		String res_path = save_order[i]->get_path();
		res_path = relative_paths ? local_path.path_to_file(res_path) : res_path;
		save_unicode_string(f, res_path);
		// ResourceUID::ID ruid = ResourceSaver::get_resource_id_for_path(save_order[i]->get_path(), false);
		if (using_uids) {
			Dictionary dict = save_order[i]->get_meta(META_COMPAT, Dictionary());
			ResourceUID::ID ruid = dict.get("uid", ResourceUID::INVALID_ID);
			f->store_64(ruid);
		}
	}
	// save internal resource table
	f->store_32(saved_resources.size()); //amount of internal resources
	Vector<uint64_t> ofs_pos;
	HashSet<String> used_unique_ids;

	for (Ref<Resource> &r : saved_resources) {
		if (r->is_built_in()) {
			if (!r->get_scene_unique_id().is_empty()) {
				if (used_unique_ids.has(r->get_scene_unique_id())) {
					r->set_scene_unique_id("");
				} else {
					used_unique_ids.insert(r->get_scene_unique_id());
				}
			}
		}
	}

	HashMap<Ref<Resource>, int> resource_map;
	int res_index = 0;
	for (Ref<Resource> &r : saved_resources) {
		if (r->is_built_in()) {
			if (r->get_scene_unique_id().is_empty()) {
				String new_id;

				while (true) {
					new_id = _resource_get_class(r) + "_" + Resource::generate_scene_unique_id();
					if (!used_unique_ids.has(new_id)) {
						break;
					}
				}

				r->set_scene_unique_id(new_id);
				used_unique_ids.insert(new_id);
			}

			save_unicode_string(f, "local://" + r->get_scene_unique_id());
			if (takeover_paths) {
				r->set_path(p_path + "::" + r->get_scene_unique_id(), true);
			}
#ifdef TOOLS_ENABLED
			r->set_edited(false);
#endif
		} else {
			save_unicode_string(f, r->get_path()); //actual external
		}
		ofs_pos.push_back(f->get_position());
		f->store_64(0); //offset in 64 bits
		resource_map[r] = res_index++;
	}

	Vector<uint64_t> ofs_table;

	//now actually save the resources
	for (int i = 0; i < resources.size(); i++) {
		const ResourceData &rd = resources[i];
		ofs_table.push_back(f->get_position());
		save_unicode_string(f, rd.type);
		f->store_32(rd.properties.size());

		for (const Property &p : rd.properties) {
			Variant value = p.value;
			if (p.pi.name == "_bundled") {
				// If this is a real PackedScene (vs. a MissingResource), we may need to fix the format
				if (rd.type == "PackedScene" && saved_resources[i]->get_class() == "PackedScene") {
					Dictionary bundled = p.value;
					int packed_scene_version = bundled.get("version", -1);
					Dictionary dict = saved_resources[i]->get_meta(META_COMPAT, Dictionary());
					int original_scene_version = dict.get("packed_scene_version", -1);
					if (original_scene_version < 0 || original_scene_version != packed_scene_version) {
						value = fix_scene_bundle(saved_resources[i], original_scene_version);
						// we have to fix this
					}
				}
			}
			f->store_32(p.name_idx);
			write_variant(f, value, resource_map, external_resources, string_map, p.pi);
		}
	}

	for (int i = 0; i < ofs_table.size(); i++) {
		f->seek(ofs_pos[i]);
		f->store_64(ofs_table[i]);
	}

	f->seek_end();
	if (imd.is_valid()) {
		uint64_t md_pos = f->get_position();
		save_unicode_string(f, imd->get_editor());
		f->store_32(imd->get_source_count());
		for (int i = 0; i < imd->get_source_count(); i++) {
			save_unicode_string(f, imd->get_source_path(i));
			save_unicode_string(f, imd->get_source_md5(i));
		}
		List<String> options;
		imd->get_options(&options);
		f->store_32(options.size());
		for (List<String>::Element *E = options.front(); E; E = E->next()) {
			save_unicode_string(f, E->get());
			write_variant(f, imd->get_option(E->get()), resource_map, external_resources, string_map);
		}

		f->seek(md_at);
		f->store_64(md_pos);
		f->seek_end();
	}
	f->store_buffer((const uint8_t *)"RSRC", 4); //magic at end

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error ResourceFormatSaverCompatBinaryInstance::set_uid(const String &p_path, ResourceUID::ID p_uid) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, "Cannot open file '" + p_path + "'.");

	Ref<FileAccess> fw;

	local_path = p_path.get_base_dir();

	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		Error err = fac->open_after_magic(f);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot open file '" + p_path + "'.");
		f = fac;

		Ref<FileAccessCompressed> facw;
		facw.instantiate();
		facw->configure("RSCC");
		err = facw->open_internal(p_path + ".uidren", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Cannot create file '" + p_path + ".uidren'.");

		fw = facw;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not a binary resource.
		return ERR_FILE_UNRECOGNIZED;
	} else {
		fw = FileAccess::open(p_path + ".uidren", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(fw.is_null(), ERR_CANT_CREATE, "Cannot create file '" + p_path + ".uidren'.");

		uint8_t magich[4] = { 'R', 'S', 'R', 'C' };
		fw->store_buffer(magich, 4);
	}

	big_endian = f->get_32();
	bool use_real64 = f->get_32();
	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian
#ifdef BIG_ENDIAN_ENABLED
	fw->store_32(!big_endian);
#else
	fw->store_32(big_endian);
#endif
	fw->set_big_endian(big_endian != 0);
	fw->store_32(use_real64); //use real64

	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_format = f->get_32();

	if (ver_format <= FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
		fw.unref();

		{
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			da->remove(p_path + ".uidren");
		}

		// Use the old approach.

		WARN_PRINT("This file is old, so it does not support UIDs.");
		return ERR_UNAVAILABLE;
	}

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED,
				vformat("File '%s' can't be loaded, as it uses a format version (%d) or engine version (%d.%d) which are not supported by your engine version (%s).",
						local_path, ver_format, ver_major, ver_minor, VERSION_BRANCH));
	}

	// Since we're not actually converting the file contents, leave the version
	// numbers in the file untouched.
	fw->store_32(ver_major);
	fw->store_32(ver_minor);
	fw->store_32(ver_format);

	save_ustring(fw, get_ustring(f)); //type

	fw->store_64(f->get_64()); //metadata offset

	uint32_t flags = f->get_32();
	flags |= ResourceFormatSaverCompatBinaryInstance::FORMAT_FLAG_UIDS;
	f->get_64(); // Skip previous UID

	fw->store_32(flags);
	fw->store_64(p_uid);

	if (flags & ResourceFormatSaverCompatBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS) {
		save_ustring(fw, get_ustring(f));
	}

	//rest of file
	uint8_t b = f->get_8();
	while (!f->eof_reached()) {
		fw->store_8(b);
		b = f->get_8();
	}

	f.unref();

	bool all_ok = fw->get_error() == OK;

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	fw.unref();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	da->remove(p_path);
	da->rename(p_path + ".uidren", p_path);
	return OK;
}

/* this is really only appropriate for saving fake-loaded resources right now; don't use it to save anything else*/
Error ResourceFormatSaverCompatBinary::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	String local_path = GDRESettings::get_singleton()->localize_path(p_path);
	ResourceFormatSaverCompatBinaryInstance saver;
	return saver.save(local_path, p_resource, p_flags);
}

Error ResourceFormatSaverCompatBinary::set_uid(const String &p_path, ResourceUID::ID p_uid) {
	String local_path = GDRESettings::get_singleton()->localize_path(p_path);
	ResourceFormatSaverCompatBinaryInstance saver;
	return saver.set_uid(local_path, p_uid);
}

bool ResourceFormatSaverCompatBinary::recognize(const Ref<Resource> &p_resource) const {
	return true; //all recognized
}

void ResourceFormatSaverCompatBinary::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	String base = p_resource->get_base_extension().to_lower();
	p_extensions->push_back(base);
	if (base != "res") {
		p_extensions->push_back("res");
	}
}

ResourceFormatSaverCompatBinary *ResourceFormatSaverCompatBinary::singleton = nullptr;

ResourceFormatSaverCompatBinary::ResourceFormatSaverCompatBinary() {
	// singleton = this;
}

///// NEW FUNCTIONS //////

Ref<Resource> ResourceFormatLoaderCompatBinary::custom_load(const String &p_path, ResourceCompatLoader::LoadType p_load_type, Error *r_error) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	Error err;

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), "Cannot open file '" + p_path + "'.");

	ResourceLoaderCompatBinary loader;
	String path = p_path;
	loader.load_type = p_load_type;
	switch (p_load_type) {
		// TODO: Figure out if we can do caching at all
		case ResourceCompatLoader::FAKE_LOAD:
		case ResourceCompatLoader::NON_GLOBAL_LOAD:
			loader.cache_mode = ResourceFormatLoader::CACHE_MODE_IGNORE;
			loader.use_sub_threads = false;
			break;
		case ResourceCompatLoader::GLTF_LOAD:
			loader.cache_mode = ResourceFormatLoader::CACHE_MODE_REPLACE;
			loader.use_sub_threads = true;
			break;
		case ResourceCompatLoader::REAL_LOAD:
		default:
			loader.cache_mode = ResourceFormatLoader::CACHE_MODE_REPLACE;
			loader.use_sub_threads = true;
			break;
	}
	loader.local_path = GDRESettings::get_singleton()->localize_path(path);
	loader.progress = nullptr;
	loader.res_path = loader.local_path;
	loader.open(f);
	err = loader.load();
	if (r_error) {
		*r_error = err;
	}
	if (err == OK) {
		return loader.get_resource();
	} else {
		return Ref<Resource>();
	}
}

uint64_t ResourceLoaderCompatBinary::get_metadata_size() {
	if (importmd_ofs == 0) {
		return 0;
	}
	if (f.is_null()) {
		return 0;
	}
	uint64_t pos = f->get_position();
	f->seek(importmd_ofs);
	get_unicode_string(); // editor
	int sc = f->get_32();
	for (int i = 0; i < sc; i++) {
		get_unicode_string(); // src
		get_unicode_string(); // md5
	}
	int pc = f->get_32();
	for (int i = 0; i < pc; i++) {
		get_unicode_string(); // name
		Variant val;
		parse_variant(val); // value
	}
	uint64_t mdsize = f->get_position() - importmd_ofs;
	f->seek(pos);
	return mdsize;
}

Error ResourceLoaderCompatBinary::load_import_metadata(bool p_return_to_pos) {
	if (f.is_null()) {
		return ERR_CANT_ACQUIRE_RESOURCE;
	}
	if (importmd_ofs == 0) {
		return ERR_UNAVAILABLE;
	}
	if (imd.is_null()) {
		imd.instantiate();
	}
	uint64_t pos = f->get_position();
	f->seek(importmd_ofs);
	imd->set_editor(get_unicode_string());
	int sc = f->get_32();
	for (int i = 0; i < sc; i++) {
		String src = get_unicode_string();
		String md5 = get_unicode_string();
		imd->add_source(src, md5);
	}
	int pc = f->get_32();

	for (int i = 0; i < pc; i++) {
		String name = get_unicode_string();
		Variant val;
		parse_variant(val);
		imd->set_option(name, val);
	}
	if (p_return_to_pos) {
		f->seek(pos);
	}
	return OK;
}

Ref<ResourceLoader::LoadToken> ResourceLoaderCompatBinary::start_ext_load(const String &p_path, const String &p_type_hint, const ResourceUID::ID uid, const String id) {
	Ref<ResourceLoader::LoadToken> load_token;
	if (!is_real_load()) {
		Error err;
		load_token = Ref<ResourceLoader::LoadToken>(memnew(ResourceLoader::LoadToken));
		load_token->local_path = p_path;
		load_token->user_path = p_path;
		if (load_type == ResourceCompatLoader::GLTF_LOAD) {
			load_token->res_if_unregistered = ResourceCompatLoader::gltf_load(p_path, p_type_hint, &err);
			if (err != OK || load_token->res_if_unregistered.is_null()) {
				load_token = Ref<ResourceLoader::LoadToken>();
			}
		} else {
			load_token->res_if_unregistered = CompatFormatLoader::create_missing_external_resource(p_path, p_type_hint, uid, id);
		}
	} else { // real load
		load_token = ResourceLoader::_load_start(p_path, p_type_hint, use_sub_threads ? ResourceLoader::LOAD_THREAD_DISTRIBUTE : ResourceLoader::LOAD_THREAD_FROM_CURRENT, ResourceFormatLoader::CACHE_MODE_REUSE);
	}
	return load_token;
}

Ref<Resource> ResourceLoaderCompatBinary::finish_ext_load(Ref<ResourceLoader::LoadToken> &load_token, Error *r_err) {
	if (load_type == ResourceCompatLoader::REAL_LOAD) {
		return ResourceLoader::_load_complete(*load_token.ptr(), r_err);
	} else if (load_type == ResourceCompatLoader::GLTF_LOAD) {
		// TODO: if implemented multi-threaded GLTF load, do something; for right now it's already loaded
	} // Fake_load, non-global load
	return load_token->res_if_unregistered;
}

void ResourceLoaderCompatBinary::set_compat_meta(Ref<Resource> &r_res) {
	//
	Dictionary compat = get_resource_info();
	compat["main_resource"] = true;
	if (packed_scene_version >= 0) {
		compat["packed_scene_version"] = packed_scene_version;
	}
	r_res->set_meta(META_COMPAT, compat);
}

Dictionary ResourceLoaderCompatBinary::get_resource_info() {
	Dictionary d;
	d["uid"] = uid;
	d["type"] = type;
	d["ver_major"] = ver_major;
	d["ver_minor"] = ver_minor;
	d["ver_format"] = ver_format;
	d["suspect_version"] = suspect_version;
	d["resource_format"] = "binary";
	d["load_type"] = load_type;
	d["using_script_class"] = using_script_class;
	d["using_real_t_double"] = using_real_t_double;
	d["stored_use_real64"] = stored_use_real64;
	d["stored_big_endian"] = stored_big_endian;
	d["using_named_scene_ids"] = using_named_scene_ids;
	d["using_uids"] = using_uids;
	d["script_class"] = script_class;
	d["import_metadata"] = imd;
	return d;
}