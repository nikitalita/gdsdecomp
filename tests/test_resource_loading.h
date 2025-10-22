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

void set_prop_dict_with_v4_variants(TypedDictionary<String, Variant> &dict) {
	dict["rect2i_property"] = Rect2i(1, 2, 3, 4);
	dict["vector2i_property"] = Vector2i(1, 2);
	dict["vector3i_property"] = Vector3i(1, 2, 3);
	dict["vector4i_property"] = Vector4i(1, 2, 3, 4);
	dict["vector4_property"] = Vector4(1.0, 2.0, 3.0, 4.0);
	dict["packed_int64_property"] = PackedInt64Array({ 0, 1, -1, INT64_MIN, INT64_MAX });
	dict["packed_float64_property"] = PackedFloat64Array({ 0.0, 1.234567, INFINITY, -INFINITY, NAN });
}

TypedDictionary<String, Variant> get_prop_dict(bool v4) {
	Ref<Resource> subresource = memnew(Resource);
	subresource->set_name("subresource");
	TypedDictionary<String, Variant> dict = {
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
		{ "subresource_property", subresource },
		{ "input_event_property", InputEventKey::create_reference(Key::KEY_1) },
		{ "image_property", Image::create_empty(4, 4, false, Image::FORMAT_RGBA8) }
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
	resource->set_name("resource_with_data");

	return resource;
}

inline void check_variant_data(const Ref<Resource> &loaded_resource, const String &key, const Variant &expected_value) {
	Variant loaded_value = loaded_resource->get_meta(key);
	if (key == "subresource_property") {
		Ref<Resource> resource = Object::cast_to<Resource>(loaded_value.operator Object *());
		CHECK(resource->get_name() == "subresource");
	} else if (expected_value.get_type() == Variant::Type::OBJECT) {
		REQUIRE(loaded_value.operator Object *());
		Ref<Image> expected_image = expected_value;
		if (expected_image.is_valid()) {
			Ref<Image> loaded_image = loaded_value;
			REQUIRE(loaded_image.is_valid());
			CHECK(loaded_image->get_size() == expected_image->get_size());
			CHECK(loaded_image->get_format() == expected_image->get_format());
			CHECK(loaded_image->get_mipmap_count() == expected_image->get_mipmap_count());
			CHECK(loaded_image->get_data().size() == expected_image->get_data().size());
			CHECK(loaded_image->get_data() == expected_image->get_data());
		} else {
			CHECK(loaded_value.operator Object *()->to_string() == expected_value.operator Object *()->to_string());
		}
	} else {
		CHECK(loaded_value == expected_value);
	}
}

void check_resource_data(const Ref<Resource> &loaded_resource, bool v4) {
	REQUIRE(loaded_resource.is_valid());
	CHECK(loaded_resource->get_name() == "resource_with_data");
	for (auto &[key, expected_value] : get_prop_dict(v4)) {
		check_variant_data(loaded_resource, key, expected_value);
	}
}

Ref<Resource> save_with_real_and_load_with_compat(const Ref<Resource> &resource, const String &resource_path) {
	resource->set_path_cache(resource_path);
	gdre::ensure_dir(resource_path.get_base_dir());
	Error error = ResourceSaver::save(resource, resource_path);
	CHECK(error == OK);

	Ref<Resource> loaded_resource = ResourceCompatLoader::real_load(resource_path, "", &error, ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP);
	CHECK(error == OK);
	REQUIRE(loaded_resource.is_valid());
	return loaded_resource;
}

Error save_with_compat(const Ref<Resource> &resource, const String &resource_path, Pair<int, int> version) {
	resource->set_path_cache(resource_path);
	gdre::ensure_dir(resource_path.get_base_dir());
	return ResourceCompatLoader::save_custom(resource, resource_path, version.first, version.second);
}

Ref<Resource> save_with_compat_and_load_with_compat(const Ref<Resource> &resource, const String &resource_path, Pair<int, int> version, bool is_text) {
	Error error = save_with_compat(resource, resource_path, version);
	CHECK(error == OK);

	Ref<Resource> loaded_resource = ResourceCompatLoader::real_load(resource_path, "", &error, ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP);
	CHECK(error == OK);
	REQUIRE(loaded_resource.is_valid());
	return loaded_resource;
}

TEST_CASE("[GDSDecomp][ResourceLoading] Resource with data") {
	String tmp_dir = get_tmp_path().path_join("resource_loading_test");
	gdre::rimraf(tmp_dir);
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

static const Vector<Pair<int, int>> versions_to_test = {
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

static const Vector<Pair<int, int>> test_ver_4_x = {
	{ 4, 0 },
	{ 4, 1 },
	{ 4, 2 },
	{ 4, 3 },
	{ 4, 4 },
	{ 4, 5 },
	{ 4, 6 },
};

TEST_CASE("[GDSDecomp][ResourceSaving] Resource with data") {
	String tmp_dir = get_tmp_path().path_join("resource_loading_test");
	gdre::rimraf(tmp_dir);
	REQUIRE(gdre::ensure_dir(tmp_dir) == OK);
	SUBCASE("Ensure default format version is 3 for 4.x") {
		Ref<Resource> resource = memnew(Resource);
		REQUIRE(resource.is_valid());
		resource->set_name("test");

		for (const auto &version : test_ver_4_x) {
			const String resource_path = tmp_dir.path_join("resource_with_data.tres");
			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(resource, resource_path, version, true);
			CHECK(loaded_resource.is_valid());
			Ref<ResourceInfo> info = ResourceInfo::get_info_from_resource(loaded_resource);
			REQUIRE(info.is_valid());
			CHECK(info->ver_format == 3);
		}
	}

	SUBCASE("Ensure setting a long PackedByteArray property forces format version to 4 if version is 4.3 and above") {
		PackedByteArray long_packed_byte_array;
		long_packed_byte_array.resize_initialized(1024);
		for (const auto &version : test_ver_4_x) {
			Ref<Resource> resource = memnew(Resource);
			resource->set_name("test");
			resource->set_meta("packed_byte_array_property", long_packed_byte_array);
			REQUIRE(resource.is_valid());
			const String resource_path = tmp_dir.path_join("resource_with_data.tres");
			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(resource, resource_path, version, true);
			CHECK(loaded_resource.is_valid());
			Ref<ResourceInfo> info = ResourceInfo::get_info_from_resource(loaded_resource);
			REQUIRE(info.is_valid());
			if (version.first >= 4 && version.second >= 3) {
				CHECK(info->ver_format == 4);
			} else {
				CHECK(info->ver_format == 3);
			}
		}
	}

	SUBCASE("Ensure setting a PackedVector4Array property forces format version to 4 for all 4.x versions") {
		for (const auto &version : test_ver_4_x) {
			Ref<Resource> resource = memnew(Resource);
			resource->set_name("test");
			resource->set_meta("packed_vector4_array_property", PackedVector4Array({ Vector4(1.0, 2.0, 3.0, 4.0), Vector4(5.0, 6.0, 7.0, 8.0) }));
			REQUIRE(resource.is_valid());
			const String resource_path = tmp_dir.path_join("resource_with_data.tres");
			ERR_PRINT_OFF; // silence the warning about Forcing format version to 4
			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(resource, resource_path, version, true);
			ERR_PRINT_ON;
			CHECK(loaded_resource.is_valid());
			Ref<ResourceInfo> info = ResourceInfo::get_info_from_resource(loaded_resource);
			REQUIRE(info.is_valid());
			CHECK(info->ver_format == 4);
		}
	}

	SUBCASE("Ensure 4.2 and below does not save PackedByteArray base64-encoded") {
		Vector<Pair<int, int>> test_ver = {
			{ 4, 0 },
			{ 4, 1 },
			{ 4, 2 },
		};
		for (const auto &version : test_ver) {
			const String resource_path = tmp_dir.path_join(vformat("ensure_pb_%d_%d.tres", version.first, version.second));
			bool use_v4 = version.first >= 4;
			Ref<Resource> resource = get_test_resource_with_data(use_v4);
			Error error = save_with_compat(resource, resource_path, version);
			CHECK(error == OK);
			String content = FileAccess::get_file_as_string(resource_path);
			CHECK(content.contains("PackedByteArray"));
			// 'PackedByteArray("' should not be present; they should have been saved as PackedByteArray(0, 1, 2, 3, 4...)
			CHECK(!content.contains("PackedByteArray(\""));
		}
	}

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
			TypedDictionary<String, Variant> v4_only_dict;
			set_prop_dict_with_v4_variants(v4_only_dict);
			for (const auto &meta : list) {
				CHECK(v4_only_dict.has(meta) == false);
			}
		}
	}
	SUBCASE("Save and load resource (binary format) with different versions") {
		for (const auto &version : versions_to_test) {
			const String resource_path = tmp_dir.path_join(vformat("resource_with_data_%d_%d.res", version.first, version.second));
			bool use_v4 = version.first >= 4;
			Ref<Resource> resource = get_test_resource_with_data(use_v4);
			if (version.first == 2) {
				// InputEvents were not written to binary resources in v2
				resource->set_meta("input_event_property", Variant());
			}
			if (version.first == 3) {
				bool foo = false;
			}

			REQUIRE(resource.is_valid());

			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(resource, resource_path, version, false);
			REQUIRE(loaded_resource.is_valid());
			CHECK(loaded_resource->get_name() == "resource_with_data");
			Dictionary expected_dict = get_prop_dict(use_v4);
			if (version.first == 2) {
				expected_dict.erase("input_event_property");
			}
			for (auto &[key, expected_value] : expected_dict) {
				check_variant_data(loaded_resource, key, expected_value);
			}
		}
	}

	SUBCASE("Save and load resource (text format) with different versions") {
		for (const auto &version : versions_to_test) {
			const String resource_path = tmp_dir.path_join(vformat("resource_with_data_%d_%d.tres", version.first, version.second));
			bool use_v4 = version.first >= 4;
			Ref<Resource> resource = get_test_resource_with_data(use_v4);
			REQUIRE(resource.is_valid());

			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(resource, resource_path, version, true);
			check_resource_data(loaded_resource, use_v4);
		}
	}
}

#include "core/os/thread_safe.h"

Ref<PackedScene> get_test_scene() {
	bool old_thread_safe = is_current_thread_safe_for_nodes();
	set_current_thread_safe_for_nodes(true);
	Node *root = memnew(Node);
	root->set_name("root");
	Node *child = memnew(Node);
	child->set_name("child");
	root->add_child(child);
	child->set_owner(root);
	Node *grandchild1 = memnew(Node);
	grandchild1->set_name("grandchild1");
	child->add_child(grandchild1);
	grandchild1->set_owner(root);
	Node *grandchild2 = memnew(Node);
	grandchild2->set_name("grandchild2");
	child->add_child(grandchild2);
	grandchild2->set_owner(root);
	Ref<PackedScene> scene = memnew(PackedScene);
	scene->pack(root);
	memdelete(root);
	set_current_thread_safe_for_nodes(old_thread_safe);
	return scene;
}

void check_loaded_scene(const Ref<PackedScene> &loaded_scene) {
	REQUIRE(loaded_scene.is_valid());

	auto root = loaded_scene->instantiate();
	{
		REQUIRE(root != nullptr);
		CHECK(root->get_name() == "root");
		auto child = root->get_node(NodePath("child"));
		REQUIRE(child != nullptr);
		CHECK(child->get_name() == "child");
		auto grandchild1 = child->get_node(NodePath("grandchild1"));
		REQUIRE(grandchild1 != nullptr);
		CHECK(grandchild1->get_name() == "grandchild1");
		auto grandchild2 = child->get_node(NodePath("grandchild2"));
		REQUIRE(grandchild2 != nullptr);
		CHECK(grandchild2->get_name() == "grandchild2");
	}
	memdelete(root);
}

TEST_CASE("[GDSDecomp][ResourceSaving][Scene] Simple Scene") {
	auto saved_scene = get_test_scene();
	REQUIRE(saved_scene.is_valid());
	String tmp_dir = get_tmp_path().path_join("scene_saving_test");
	gdre::rimraf(tmp_dir);
	REQUIRE(gdre::ensure_dir(tmp_dir) == OK);

	SUBCASE("Save and load scene (text format)") {
		const String resource_path = tmp_dir.path_join("test_scene_text.tscn");
		Ref<Resource> loaded_resource = save_with_real_and_load_with_compat(saved_scene, resource_path);
		REQUIRE(loaded_resource.is_valid());
		Ref<PackedScene> loaded_scene = loaded_resource;
		check_loaded_scene(loaded_scene);
	}

	SUBCASE("Save and load scene (binary format)") {
		const String resource_path = tmp_dir.path_join("test_scene_binary.scn");
		Ref<Resource> loaded_resource = save_with_real_and_load_with_compat(saved_scene, resource_path);
		REQUIRE(loaded_resource.is_valid());
		Ref<PackedScene> loaded_scene = loaded_resource;
		check_loaded_scene(loaded_scene);
	}

	SUBCASE("Save and load scene (binary format) all versions") {
		for (const auto &version : versions_to_test) {
			const String resource_path = tmp_dir.path_join(vformat("test_scene_binary_%d_%d.scn", version.first, version.second));
			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(saved_scene, resource_path, version, false);
			REQUIRE(loaded_resource.is_valid());
			Ref<PackedScene> loaded_scene = loaded_resource;
			check_loaded_scene(loaded_scene);
		}
	}

	SUBCASE("Save and load scene (text format) all versions") {
		for (const auto &version : versions_to_test) {
			const String resource_path = tmp_dir.path_join(vformat("test_scene_text_%d_%d.tscn", version.first, version.second));
			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(saved_scene, resource_path, version, true);
			REQUIRE(loaded_resource.is_valid());
			Ref<PackedScene> loaded_scene = loaded_resource;
			check_loaded_scene(loaded_scene);
		}
	}
}

void check_external_test(const Ref<Resource> &loaded_resource, const Ref<Resource> &reference_external_resource) {
	REQUIRE(loaded_resource.is_valid());
	CHECK(loaded_resource->get_meta("external_resource").operator Object *());
	Ref<Resource> external_resource_from_loaded_resource = loaded_resource->get_meta("external_resource");
	REQUIRE(external_resource_from_loaded_resource.is_valid());
	CHECK(external_resource_from_loaded_resource->get_name() == reference_external_resource->get_name());
	CHECK(external_resource_from_loaded_resource->get_path() == reference_external_resource->get_path());
}

TEST_CASE("[GDSDecomp][ResourceSaving] Test external resources") {
	String tmp_dir = get_tmp_path().path_join("external_resources_saving_test");
	gdre::rimraf(tmp_dir);
	GDRESettings::get_singleton()->set_project_path(tmp_dir);
	GDREPackedData::get_singleton()->set_default_file_access();
	REQUIRE(gdre::ensure_dir(tmp_dir) == OK);
	Ref<Resource> resource = memnew(Resource);
	const String external_resource_text_path = "res://external_resource.tres";
	const String external_resource_binary_path = "res://external_resource.res";
	resource->set_name("resource");
	Ref<Resource> external_resource = memnew(Resource);
	external_resource->set_name("external_resource");
	resource->set_meta("external_resource", external_resource);

	SUBCASE("Save and load external resource (text format)") {
		const String resource_path = "res://resource.tres";
		REQUIRE(GDRESettings::get_singleton()->get_project_path() == tmp_dir);
		REQUIRE(GDRESettings::get_singleton()->globalize_path(resource_path) == tmp_dir.path_join(resource_path.get_file()));

		resource->set_path_cache(resource_path);
		external_resource->set_path_cache(external_resource_text_path);

		Ref<Resource> loaded_external_resource = save_with_real_and_load_with_compat(external_resource, external_resource_text_path);
		REQUIRE(loaded_external_resource.is_valid());
		Ref<Resource> loaded_resource = save_with_real_and_load_with_compat(resource, resource_path);
		REQUIRE(loaded_resource.is_valid());
		check_external_test(loaded_resource, external_resource);
	}
	SUBCASE("Save and load external resource (binary format)") {
		const String resource_path = "res://resource.res";
		REQUIRE(GDRESettings::get_singleton()->get_project_path() == tmp_dir);

		REQUIRE(GDRESettings::get_singleton()->globalize_path(resource_path) == tmp_dir.path_join(resource_path.get_file()));
		resource->set_path_cache(resource_path);
		external_resource->set_path_cache(external_resource_binary_path);

		Ref<Resource> loaded_external_resource = save_with_real_and_load_with_compat(external_resource, external_resource_binary_path);
		REQUIRE(loaded_external_resource.is_valid());
		Ref<Resource> loaded_resource = save_with_real_and_load_with_compat(resource, resource_path);
		REQUIRE(loaded_resource.is_valid());
		check_external_test(loaded_resource, external_resource);
	}
	SUBCASE("Save and load external resource (binary format) all versions") {
		REQUIRE(GDRESettings::get_singleton()->get_project_path() == tmp_dir);
		for (const auto &version : versions_to_test) {
			const String resource_path = vformat("res://resource_%d_%d.res", version.first, version.second);
			resource->set_path_cache(resource_path);
			external_resource->set_path_cache(external_resource_binary_path);
			REQUIRE(GDRESettings::get_singleton()->get_project_path() == tmp_dir);
			REQUIRE(GDRESettings::get_singleton()->globalize_path(resource_path) == tmp_dir.path_join(resource_path.get_file()));

			Ref<Resource> loaded_external_resource = save_with_compat_and_load_with_compat(external_resource, external_resource_binary_path, version, false);
			REQUIRE(loaded_external_resource.is_valid());
			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(resource, resource_path, version, false);
			REQUIRE(loaded_resource.is_valid());
			check_external_test(loaded_resource, loaded_external_resource);
		}
	}
	SUBCASE("Save and load external resource (text format) all versions") {
		REQUIRE(GDRESettings::get_singleton()->get_project_path() == tmp_dir);
		for (const auto &version : versions_to_test) {
			// ONLY THE RESOURCE PATH, NOT THE EXTERNAL RESOURCE PATH
			const String resource_path = vformat("res://resource_%d_%d.tres", version.first, version.second);
			resource->set_path_cache(resource_path);
			external_resource->set_path_cache(external_resource_text_path);
			REQUIRE(GDRESettings::get_singleton()->get_project_path() == tmp_dir);
			REQUIRE(GDRESettings::get_singleton()->globalize_path(resource_path) == tmp_dir.path_join(resource_path.get_file()));

			Ref<Resource> loaded_external_resource = save_with_compat_and_load_with_compat(external_resource, external_resource_text_path, version, true);
			REQUIRE(loaded_external_resource.is_valid());
			Ref<Resource> loaded_resource = save_with_compat_and_load_with_compat(resource, resource_path, version, true);
			REQUIRE(loaded_resource.is_valid());
			check_external_test(loaded_resource, loaded_external_resource);
		}
	}
	GDREPackedData::get_singleton()->reset_default_file_access();
	GDRESettings::get_singleton()->set_project_path("");
}

} //namespace TestResourceLoading

#endif
