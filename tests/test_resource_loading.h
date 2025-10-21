#ifndef TEST_RESOURCE_LOADING_H
#define TEST_RESOURCE_LOADING_H

#include <compat/resource_compat_text.h>
#include <compat/resource_loader_compat.h>
#include <modules/gdscript/gdscript_tokenizer_buffer.h>
#include <utility/common.h>
#include <utility/glob.h>

#include "test_common.h"
#include "tests/test_macros.h"
#include "utility/file_access_gdre.h"
#include "utility/resource_info.h"

namespace TestResourceLoading {

TEST_CASE("[GDSDecomp][ResourceLoading] Basic resource loading") {
	REQUIRE(GDRESettings::get_singleton());
	// Get available test versions
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	// Test each version
	for (const String &version : versions) {
		String test_dir = get_test_resources_path().path_join(version);
		String previous_resource_path = GDRESettings::get_singleton()->get_project_path();
		GDRESettings::get_singleton()->set_project_path(test_dir);
		GDREPackedData::get_singleton()->set_default_file_access();

		// Test loading a simple resource
		SUBCASE((String("Load simple resource: ") + version).utf8().get_data()) {
			String resource_path = test_dir.path_join("simple_resource.tres");
			Error error;
			Ref<Resource> resource = ResourceCompatLoader::real_load(resource_path, "", &error);

			CHECK(error == OK);
			CHECK(resource.is_valid());

			// Verify resource properties
			if (resource.is_valid()) {
				CHECK(resource->get_class() == "Resource");
			}
		}

		// Test loading a resource with dependencies
		SUBCASE((String("Load resource with dependencies: ") + version).utf8().get_data()) {
			String resource_path = test_dir.path_join("resource_with_deps.tres");
			Error error;
			Ref<Resource> resource = ResourceCompatLoader::real_load(resource_path, "", &error);

			CHECK(error == OK);
			CHECK(resource.is_valid());

			// Verify dependencies are loaded
			if (resource.is_valid()) {
				// Check for expected dependencies
				// This will depend on the specific test resource structure
				//metadata/test
				//metadata/test2
				Ref<Resource> dependency = resource->get("metadata/test");
				CHECK(dependency.is_valid());
				dependency = resource->get("metadata/test2");
				CHECK(dependency.is_valid());
			}
		}

		// Test error handling for non-existent resource
		SUBCASE((String("Handle non-existent resource: ") + version).utf8().get_data()) {
			String non_existent_path = test_dir.path_join("non_existent_resource.tres");
			Error error;
			Ref<Resource> resource = ResourceCompatLoader::real_load(non_existent_path, "", &error);

			CHECK(error != OK);
			CHECK(resource.is_null());
		}
		GDRESettings::get_singleton()->set_project_path(previous_resource_path);
		GDREPackedData::get_singleton()->reset_default_file_access();
	}
}

TEST_CASE("[GDSDecomp][ResourceLoading] Resource format compatibility") {
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		// SUBCASE(version.utf8().get_data()) {
		String test_dir = get_test_resources_path().path_join(version);

		// Test format version detection
		SUBCASE("Format version detection") {
			String resource_path = test_dir.path_join("simple_resource.tres");
			Error error;
			ResourceFormatLoaderCompatText loader;
			Ref<ResourceInfo> info = loader.get_resource_info(resource_path, &error);

			CHECK(error == OK);
			CHECK(info.is_valid());
			CHECK(info->resource_format == "text");
			CHECK(info->ver_format > 0);
		}

		// Test resource type detection
		SUBCASE("Resource type detection") {
			String resource_path = test_dir.path_join("simple_resource.tres");
			Error error;
			ResourceFormatLoaderCompatText loader;
			Ref<ResourceInfo> info = loader.get_resource_info(resource_path, &error);

			CHECK(error == OK);
			CHECK(info.is_valid());
			CHECK(info->type == "Resource");
		}
		// }
	}
}

TEST_CASE("[GDSDecomp][ResourceLoading] Resource loading modes") {
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		// SUBCASE(version.utf8().get_data()) {
		String test_dir = get_test_resources_path().path_join(version);
		String resource_path = test_dir.path_join("simple_resource.tres");

		// Test different loading modes
		SUBCASE("Different loading modes") {
			Error error;

			// Test REAL_LOAD mode
			Ref<Resource> real_resource = ResourceCompatLoader::real_load(resource_path, "", &error);
			CHECK(error == OK);
			CHECK(real_resource.is_valid());

			// Test FAKE_LOAD mode
			Ref<Resource> fake_resource = ResourceCompatLoader::fake_load(resource_path, "", &error);
			CHECK(error == OK);
			CHECK(fake_resource.is_valid());

			// Test NON_GLOBAL_LOAD mode
			Ref<Resource> non_global_resource = ResourceCompatLoader::non_global_load(resource_path, "", &error);
			CHECK(error == OK);
			CHECK(non_global_resource.is_valid());
		}

		// Test different cache modes
		SUBCASE("Different cache modes") {
			Error error;

			// Test CACHE_MODE_IGNORE
			Ref<Resource> ignore_resource = ResourceCompatLoader::real_load(
					resource_path, "", &error, ResourceFormatLoader::CACHE_MODE_IGNORE);
			CHECK(error == OK);

			// Test CACHE_MODE_REUSE
			Ref<Resource> reuse_resource = ResourceCompatLoader::real_load(
					resource_path, "", &error, ResourceFormatLoader::CACHE_MODE_REUSE);
			CHECK(error == OK);

			// Test CACHE_MODE_REPLACE
			Ref<Resource> replace_resource = ResourceCompatLoader::real_load(
					resource_path, "", &error, ResourceFormatLoader::CACHE_MODE_REPLACE);
			CHECK(error == OK);
		}
		// }
	}
}

void set_prop_dict_with_v4_variants(TypedDictionary<StringName, Variant> &dict) {
	dict["rect2i_property"] = Rect2i(1, 2, 3, 4);
	dict["vector2i_property"] = Vector2i(1, 2);
	dict["vector3i_property"] = Vector3i(1, 2, 3);
	dict["vector4i_property"] = Vector4i(1, 2, 3, 4);
	dict["vector4_property"] = Vector4(1.0, 2.0, 3.0, 4.0);
	dict["packed_int64_property"] = PackedInt64Array({ 0, 1, -1, INT64_MIN, INT64_MAX });
	dict["packed_float64_property"] = PackedFloat64Array({ 0.0, 1.234567, INFINITY, -INFINITY, NAN });
	dict["packed_vector4_array_property"] = PackedVector4Array({ Vector4(1.0, 2.0, 3.0, 4.0), Vector4(5.0, 6.0, 7.0, 8.0) });
}

TypedDictionary<StringName, Variant> get_prop_dict(bool v4) {
	TypedDictionary<StringName, Variant> dict = {
		{ "int_property", 123 },
		{ "float_property", 123.456 },
		{ "string_property", "Hello, World!" },
		{ "bool_property", true },
		{ "vector2_property", Vector2(1.0, 2.0) },
		{ "rect2_property", Rect2(1.0, 2.0, 3.0, 4.0) },
		{ "vector3_property", Vector3(1.0, 2.0, 3.0) },
		{ "plane_property", Plane(1.0, 2.0, 3.0, 4.0) },
		{ "quaternion_property", Quaternion(1.0, 2.0, 3.0, 4.0) },
		{ "aabb_property", AABB(Vector3(1.0, 2.0, 3.0), Vector3(4.0, 5.0, 6.0)) },
		{ "basis_property", Basis(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0) },
		{ "transform3d_property", Transform3D(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0) },
		{ "transform2d_property", Transform2D(1.0, 2.0, 3.0, 4.0, 5.0, 6.0) },
		{ "color_property", Color(1.0, 2.0, 3.0, 4.0) },
		{ "node_path_property", NodePath("res://test.tres") },
		{ "packed_byte_array_property", PackedByteArray({ 0, 1, 2, 3, 4, 5, 6 }) },
		{ "packed_int32_property", PackedInt32Array({ 0, 1, -1, INT_MIN, INT_MAX }) },
		{ "packed_float32_property", PackedFloat32Array({ 0.0, 1.234567, INFINITY, -INFINITY, NAN }) },
		{ "packed_string_array_property", PackedStringArray({ "Hello", "World" }) },
		{ "packed_vector2_array_property", PackedVector2Array({ Vector2(1.0, 2.0), Vector2(3.0, 4.0) }) },
		{ "packed_vector3_array_property", PackedVector3Array({ Vector3(1.0, 2.0, 3.0), Vector3(4.0, 5.0, 6.0) }) },
		{ "packed_color_array_property", PackedColorArray({ Color(1.0, 2.0, 3.0, 4.0), Color(5.0, 6.0, 7.0, 8.0) }) },
		{ "dictionary_property", Dictionary({ { "key1", "value1" }, { "key2", 2 } }) },
		{ "array_property", Array({ 1, 2, 3 }) },
		{ "object_property", InputEventKey::create_reference(Key::KEY_1) },
	};
	if (v4) {
		set_prop_dict_with_v4_variants(dict);
	}
	return dict;
}

Ref<Resource> get_test_resource_with_data(bool v4) {
	Ref<Resource> resource = memnew(Resource);

	for (auto &[key, value] : get_prop_dict(v4)) {
		resource->set_meta(key, value);
	}

	return resource;
}

void check_resource_data(const Ref<Resource> &loaded_resource, bool v4) {
	REQUIRE(loaded_resource.is_valid());
	for (auto &[key, expected_value] : get_prop_dict(v4)) {
		Variant loaded_value = loaded_resource->get_meta(key);
		if (loaded_value.get_type() == Variant::Type::OBJECT) {
			REQUIRE(loaded_value.operator Object *());
			CHECK(loaded_value.operator Object *()->to_string() == expected_value.operator Object *()->to_string());
		} else {
			CHECK(loaded_value == expected_value);
		}
	}
}

Ref<Resource> save_with_real_and_load_with_compat(const Ref<Resource> &resource, const String &resource_path) {
	resource->set_path_cache(resource_path);
	gdre::ensure_dir(resource_path.get_base_dir());
	Error error = ResourceSaver::save(resource, resource_path);
	CHECK(error == OK);

	Ref<Resource> loaded_resource = ResourceCompatLoader::real_load(resource_path, "", &error);
	CHECK(error == OK);
	REQUIRE(loaded_resource.is_valid());
	return loaded_resource;
}

Ref<Resource> save_with_compat_and_load_with_compat(const Ref<Resource> &resource, const String &resource_path, Pair<int, int> version, bool is_text) {
	resource->set_path_cache(resource_path);
	gdre::ensure_dir(resource_path.get_base_dir());
	Error error = ResourceCompatLoader::save_custom(resource, resource_path, version.first, version.second);
	CHECK(error == OK);

	Ref<Resource> loaded_resource = ResourceCompatLoader::real_load(resource_path, "", &error);
	CHECK(error == OK);
	REQUIRE(loaded_resource.is_valid());
	return loaded_resource;
}

TEST_CASE("[GDSDecomp][ResourceLoading] Resource with data") {
	String tmp_dir = get_tmp_path().path_join("resource_loading_test");
	REQUIRE(gdre::ensure_dir(tmp_dir) == OK);

	SUBCASE("Save and load resource (text format)") {
		bool use_v4 = true;
		Ref<Resource> resource = get_test_resource_with_data(true);
		REQUIRE(resource.is_valid());
		const String resource_path = tmp_dir.path_join("resource_with_data.tres");
		Ref<Resource> loaded_resource = save_with_real_and_load_with_compat(resource, resource_path);
		check_resource_data(loaded_resource, use_v4);
	}

	SUBCASE("Save and load resource (binary format)") {
		bool use_v4 = true;
		Ref<Resource> resource = get_test_resource_with_data(true);
		REQUIRE(resource.is_valid());
		const String resource_path = tmp_dir.path_join("resource_with_data.res");
		Ref<Resource> loaded_resource = save_with_real_and_load_with_compat(resource, resource_path);
		check_resource_data(loaded_resource, use_v4);
	}
}

TEST_CASE("[GDSDecomp][ResourceSaving] Resource with data") {
	String tmp_dir = get_tmp_path().path_join("resource_loading_test");
	REQUIRE(gdre::ensure_dir(tmp_dir) == OK);

	Vector<Pair<int, int>> versions = {
		{ 2, 0 },
		{ 3, 0 },
		{ 3, 1 },
		{ 3, 2 },
		{ 3, 3 },
		{ 3, 4 },
		{ 3, 5 },
		{ 3, 6 },
		{ 4, 0 },
		{ 4, 1 },
		{ 4, 2 },
		{ 4, 3 },
		{ 4, 4 },
		{ 4, 5 },
		{ 4, 6 },
	};

	SUBCASE("Saving a resource with v4 variants on v2 and v3 (text format)") {
		Vector<Pair<int, int>> test_ver = {
			{ 2, 0 },
			{ 3, 0 },
		};
		for (const auto &version : test_ver) {
			const String resource_path = tmp_dir.path_join("resource_with_data_v4_on_v2_v3.tres");
			Ref<Resource> resource = get_test_resource_with_data(true);
			REQUIRE(resource.is_valid());
			// VariantWriterCompat::write_compat_v2_v3 will spam a lot of errors here, so turn off error printing
			ERR_PRINT_OFF
			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(resource, resource_path, version, true);
			ERR_PRINT_ON
			check_resource_data(loaded_resource, false);
			auto compat = ResourceInfo::get_info_from_resource(loaded_resource);
			// erase it so that the meta list size is correct
			loaded_resource->set_meta(META_COMPAT, Variant());

			auto v2_v3_dict = get_prop_dict(false);
			List<StringName> list;
			loaded_resource->get_meta_list(&list);
			CHECK(list.size() == v2_v3_dict.size());
			TypedDictionary<StringName, Variant> v4_only_dict;
			set_prop_dict_with_v4_variants(v4_only_dict);
			for (const auto &meta : list) {
				CHECK(v4_only_dict.has(meta) == false);
			}
		}
	}
	SUBCASE("Save and load resource (text format) with different versions") {
		const String resource_path = tmp_dir.path_join("resource_with_data.tres");

		for (const auto &version : versions) {
			bool use_v4 = version.first >= 4;
			Ref<Resource> resource = get_test_resource_with_data(use_v4);
			REQUIRE(resource.is_valid());

			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(resource, resource_path, version, true);
			check_resource_data(loaded_resource, use_v4);
		}
	}
}
} //namespace TestResourceLoading

#endif
