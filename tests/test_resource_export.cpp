#include "tests/test_macros.h"

#include "core/io/resource_importer.h"
#include "modules/vorbis/audio_stream_ogg_vorbis.h"
#include "test_common.h"
#include "test_config_helpers.inc"
#include "tests/test_macros.h"
#include "utility/common.h"
#include "utility/gdre_config.h"
#include "utility/gdre_settings.h"
#include "bytecode/bytecode_versions.h"
#include "compat/fake_gdscript.h"
#include "compat/visual_shader_compat.h"
#include "core/io/missing_resource.h"
#include "core/version_generated.gen.h"
#include "modules/gdscript/gdscript.h"
#include <compat/resource_compat_text.h>
#include <compat/resource_loader_compat.h>
#include <core/object/class_db.h>
#include <exporters/fontfile_exporter.h>
#include <modules/gdsdecomp/exporters/resource_exporter.h>
#include <scene/resources/audio/audio_stream_wav.h>
#include <scene/resources/font.h>

TEST_FORCE_LINK(test_resource_export)
namespace TestResourceExport {

String get_resource_type_dir(const String &version, const String &resource_type) {
	return get_test_resources_path().path_join(version).path_join(resource_type);
}

bool check_if_resource_type_dir_exists(const String &version, const String &resource_type) {
	String resource_type_dir = get_resource_type_dir(version, resource_type);
	return DirAccess::exists(resource_type_dir);
}

Error test_export_sample(const String &version) {
	String test_dir = get_test_resources_path().path_join(version).path_join("sample");
	Vector<String> files = gdre::get_recursive_dir_list(test_dir, { "*.sample" });
	String output_dir = get_tmp_path().path_join(version).path_join("sample");
	for (const String &file : files) {
		// original file
		String original_file = file.rsplit("-", true, 1)[0];
		String output_file = output_dir.path_join(original_file.get_file());
		Error err = Exporter::export_file(output_file, file);
		CHECK(err == OK);
		// load the sample file, not the wav
		Ref<AudioStreamWAV> original_sample = ResourceCompatLoader::non_global_load(file);
		CHECK(original_sample.is_valid());
		Ref<AudioStreamWAV> exported_sample = AudioStreamWAV::load_from_file(output_file, {});
		CHECK(exported_sample.is_valid());
		CHECK(original_sample->get_data() == exported_sample->get_data());
	}
	return OK;
}

Error test_export_oggvorbisstr(const String &version) {
	String test_dir = get_test_resources_path().path_join(version).path_join("ogg");
	Vector<String> files = gdre::get_recursive_dir_list(test_dir, { "*.oggvorbisstr" });
	String output_dir = get_tmp_path().path_join(version).path_join("ogg");
	for (const String &file : files) {
		// original file
		String original_file = file.rsplit("-", true, 1)[0];
		String output_file = output_dir.path_join(original_file.get_file());
		Error err = Exporter::export_file(output_file, file);
		CHECK(err == OK);
		Ref<AudioStreamOggVorbis> original_audio = ResourceCompatLoader::non_global_load(file);
		CHECK(original_audio.is_valid());
		Ref<AudioStreamOggVorbis> exported_audio = AudioStreamOggVorbis::load_from_file(output_file);
		CHECK(exported_audio.is_valid());
		auto original_packet_sequence = original_audio->get_packet_sequence();
		auto exported_packet_sequence = exported_audio->get_packet_sequence();
		Array original_packet_data = original_packet_sequence->get_packet_data();
		Array exported_packet_data = exported_packet_sequence->get_packet_data();
		CHECK(original_packet_data.size() == exported_packet_data.size());
		for (int i = 0; i < original_packet_data.size(); i++) {
			CHECK(original_packet_data[i] == exported_packet_data[i]);
		}
	}
	return OK;
}

Error test_export_texture(const String &version) {
	String test_dir = get_test_resources_path().path_join(version).path_join("texture");
	Vector<String> files = gdre::get_recursive_dir_list(test_dir, { "*.ctex" });
	String output_dir = get_tmp_path().path_join(version).path_join("texture");
	for (const String &file : files) {
		// original file
		String original_file = file.rsplit("-", true, 1)[0];
		String output_file = output_dir.path_join(original_file.get_file());
		gdre::ensure_dir(output_file.get_base_dir());

		Error err = Exporter::export_file(output_file, file);
		CHECK(err == OK);

		Ref<Texture2D> original_texture = ResourceCompatLoader::non_global_load(file);
		CHECK(original_texture.is_valid());

		Ref<Image> original_image = original_texture->get_image();
		CHECK(original_image.is_valid());

		Ref<Image> exported_image;
		exported_image.instantiate();
		exported_image->load(output_file);
		CHECK(original_image->get_width() == exported_image->get_width());
		CHECK(original_image->get_height() == exported_image->get_height());
		for (int64_t x = 0; x < original_image->get_width(); x++) {
			for (int64_t y = 0; y < original_image->get_height(); y++) {
				Color c = original_image->get_pixel(x, y);
				Color c2 = exported_image->get_pixel(x, y);
				if (c != c2) {
					CHECK(c.a == 0.0);
					CHECK(c.a == c2.a);
				} else {
					CHECK(c == c2);
				}
			}
		}
	}
	return OK;
}

List<String> process_dependencies(const List<String> &_deps, const String &test_dir) {
	List<String> deps;
	for (const String &dep : _deps) {
		auto f = dep.split("::", false);
		auto d = f[f.size() - 1];
		deps.push_back(test_dir.path_join(d.strip_edges().trim_prefix("res://")));
	}
	return deps;
}

List<String> get_dependencies(const String &file, const String &test_dir) {
	List<String> _deps;
	ResourceCompatLoader::get_dependencies(file, &_deps, false);
	return process_dependencies(_deps, test_dir);
}

Error test_export_bmfont(const String &version) {
	String test_dir = get_test_resources_path().path_join(version).path_join("bmfont");
	// We have to set the resource dir first, since the fontdata files have dependencies
	String previous_resource_dir = GDRESettings::get_singleton()->get_project_path();
	GDRESettings::get_singleton()->set_project_path(test_dir);

	Vector<String> files = gdre::get_recursive_dir_list(test_dir, { "*.fontdata" });
	String output_dir = get_tmp_path().path_join(version).path_join("bmfont");
	for (const String &file : files) {
		auto original_dependencies = get_dependencies(file, test_dir);
		// original file is in the root; the exports are in the .godot/imported folder
		String original_file = test_dir.path_join(file.rsplit("-", true, 1)[0].get_file());
		String output_file = output_dir.path_join(original_file.get_file());
		Error err = Exporter::export_file(output_file, file);
		CHECK(err == OK);
		Ref<FontFile> original_fontfile;
		original_fontfile.instantiate();
		err = original_fontfile->_load_bitmap_font(original_file, &original_dependencies);
		REQUIRE(err == OK);
		REQUIRE(original_fontfile.is_valid());

		Ref<FontFile> exported_fontfile;
		exported_fontfile.instantiate();
		// We use the same dependencies here, because the fontfile exporter doesn't copy the image dependencies (that is taken care of by the texture exporter)
		err = exported_fontfile->_load_bitmap_font(output_file, &original_dependencies);
		REQUIRE(err == OK);
		REQUIRE(exported_fontfile.is_valid());
		CHECK(original_fontfile->get_data() == exported_fontfile->get_data());
		CHECK(original_fontfile->get_fallbacks() == exported_fontfile->get_fallbacks());
		CHECK(original_fontfile->get_fixed_size_scale_mode() == exported_fontfile->get_fixed_size_scale_mode());
		CHECK(original_fontfile->get_antialiasing() == exported_fontfile->get_antialiasing());
		CHECK(original_fontfile->get_disable_embedded_bitmaps() == exported_fontfile->get_disable_embedded_bitmaps());
		CHECK(original_fontfile->get_hinting() == exported_fontfile->get_hinting());
		// go through all the glyphs and check if the glyphs are the same
		int cache_count = original_fontfile->get_cache_count();
		CHECK(cache_count == exported_fontfile->get_cache_count());
		static const int font_size = 16;
		static const Vector2i font_size_vec(font_size, 0);
		for (int i = 0; i < cache_count; i++) {
			auto glyphs = original_fontfile->get_glyph_list(i, font_size_vec);
			auto exported_glyphs = exported_fontfile->get_glyph_list(i, font_size_vec);
			CHECK(glyphs.size() == exported_glyphs.size());
			for (int j = 0; j < glyphs.size(); j++) {
				CHECK(glyphs[j] == exported_glyphs[j]);
				CHECK(original_fontfile->get_glyph_size(i, font_size_vec, glyphs[j]) == exported_fontfile->get_glyph_size(i, font_size_vec, exported_glyphs[j]));
				CHECK(original_fontfile->get_glyph_offset(i, font_size_vec, glyphs[j]) == exported_fontfile->get_glyph_offset(i, font_size_vec, exported_glyphs[j]));
				CHECK(original_fontfile->get_glyph_texture_idx(i, font_size_vec, glyphs[j]) == exported_fontfile->get_glyph_texture_idx(i, font_size_vec, exported_glyphs[j]));
				CHECK(original_fontfile->get_glyph_uv_rect(i, font_size_vec, glyphs[j]) == exported_fontfile->get_glyph_uv_rect(i, font_size_vec, exported_glyphs[j]));
				CHECK(original_fontfile->get_kerning(i, font_size, Vector2i(glyphs[j], exported_glyphs[j])) == exported_fontfile->get_kerning(i, font_size, Vector2i(exported_glyphs[j], glyphs[j])));
				CHECK(original_fontfile->get_glyph_advance(i, font_size, glyphs[j]) == exported_fontfile->get_glyph_advance(i, font_size, exported_glyphs[j]));
				char32_t char_from_glyph = original_fontfile->get_char_from_glyph_index(font_size, glyphs[j]);
				char32_t exported_char_from_glyph = exported_fontfile->get_char_from_glyph_index(font_size, exported_glyphs[j]);
				CHECK(char_from_glyph == exported_char_from_glyph);
				CHECK(original_fontfile->get_glyph_index(font_size, char_from_glyph) == exported_fontfile->get_glyph_index(font_size, exported_char_from_glyph));
			}
			auto kerning_pairs = original_fontfile->get_kerning_list(i, font_size);
			auto exported_kerning_pairs = exported_fontfile->get_kerning_list(i, font_size);
			CHECK(kerning_pairs.size() == exported_kerning_pairs.size());
			for (int j = 0; j < kerning_pairs.size(); j++) {
				CHECK(kerning_pairs[j] == exported_kerning_pairs[j]);
			}
		}
	}
	GDRESettings::get_singleton()->set_project_path(previous_resource_dir);
	return OK;
}

void test_import_options() {
// this only works in editor builds
#ifndef TOOLS_ENABLED
	return;
#else
	StringName resource_importer_class_name = "ResourceImporter";
	// query the classdb for all the classes that derive from ResourceImporter
	LocalVector<StringName> classes;
	// ClassDB::get_class_list(classes);
	ClassDB::get_inheriters_from_class(resource_importer_class_name, classes);
	for (const StringName &class_name : classes) {
		Engine::get_singleton()->set_editor_hint(true);
		Object *obj = ClassDB::instantiate(class_name);
		Engine::get_singleton()->set_editor_hint(false);
		REQUIRE(obj != nullptr);
		Ref<ResourceImporter> importer = Object::cast_to<ResourceImporter>(obj);
		REQUIRE(importer.is_valid());
		List<ResourceImporter::ImportOption> options;
		String importer_name = importer->get_importer_name();
		Ref<ResourceExporter> exporter = Exporter::get_exporter(importer_name, "");

		importer->get_import_options("", &options);
		for (const ResourceImporter::ImportOption &option : options) {
			// TODO: make the exporters output the import options for the current version major/minor so we can check them.
			CHECK(option.option.name != "");
		}
	}
#endif
}

TEST_CASE("[GDSDecomp][ResourceExport] Test import options") {
	test_import_options();
}

TEST_CASE("[GDSDecomp][ResourceExport] Export sample") {
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		if (!check_if_resource_type_dir_exists(version, "sample")) {
			continue;
		}
		SUBCASE(vformat("%s: Test export sample", version).utf8().get_data()) {
			test_export_sample(version);
		}
	}
}

TEST_CASE("[GDSDecomp][ResourceExport] Export oggvorbisstr") {
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		if (!check_if_resource_type_dir_exists(version, "ogg")) {
			continue;
		}
		SUBCASE(vformat("%s: Test export oggvorbisstr", version).utf8().get_data()) {
			test_export_oggvorbisstr(version);
		}
	}
}

TEST_CASE("[GDSDecomp][ResourceExport] Export texture") {
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		if (!check_if_resource_type_dir_exists(version, "texture")) {
			continue;
		}
		SUBCASE(vformat("%s: Test export texture", version).utf8().get_data()) {
			test_export_texture(version);
		}
	}
}

TEST_CASE("[GDSDecomp][ResourceExport] Export bmfont") {
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		if (!check_if_resource_type_dir_exists(version, "bmfont")) {
			continue;
		}
		SUBCASE(vformat("%s: Test export bmfont", version).utf8().get_data()) {
			test_export_bmfont(version);
		}
	}
}

TEST_CASE("[GDSDecomp][ResourceExport][ConfigTest] Recovery/execute_visual_shader_node_scripts") {
	{
		// Real scripts can only be created if the GDScript language is up; otherwise there is nothing to test.
		Ref<GDScript> probe;
		probe.instantiate();
		probe->set_source_code("extends RefCounted\n");
		if (probe->reload() != OK) {
			MESSAGE("GDScript can't compile scripts in this environment, skipping");
			return;
		}
	}
	static const char *script_source = "@tool\nextends VisualShaderNodeCustom\n\nfunc _get_name():\n\treturn \"ConfigTestNode\"\n";

	Ref<FakeGDScript> fake_script;
	fake_script.instantiate();
	fake_script->set_override_bytecode_revision(GDScriptDecompVersion::LATEST_GDSCRIPT_COMMIT);
	fake_script->set_source_code(script_source);
	REQUIRE(fake_script->is_loaded());

	Ref<MissingResource> missing;
	missing.instantiate();
	missing->set_original_class("VisualShaderNodeCustom");
	missing->set_recording_properties(true);
	missing->set_script(fake_script);

	Ref<VisualShaderNodeCustomConverterCompat> converter;
	converter.instantiate();
	REQUIRE(converter->handles_type("VisualShaderNodeCustom", GODOT_VERSION_MAJOR));

	auto get_real_script = [](const Ref<Resource> &p_res) -> Ref<GDScript> {
		return p_res->get_script();
	};

	SUBCASE("Disabled by default: the script is not executed") {
		CHECK_FALSE(bool(GDREConfig::get_singleton()->get_setting(CoreSettings::RECOVERY_EXECUTE_VISUAL_SHADER_NODE_SCRIPTS)));
		ERR_PRINT_OFF;
		Error err = OK;
		Ref<Resource> converted = converter->convert(missing, ResourceInfo::REAL_LOAD, GODOT_VERSION_MAJOR, &err);
		ERR_PRINT_ON;
		REQUIRE(converted.is_valid());
		CHECK(Object::cast_to<VisualShaderNodeCustom>(converted.ptr()) != nullptr);
		CHECK(get_real_script(converted).is_null());
	}
	SUBCASE("Enabled: the script is compiled and attached") {
		TestConfigHelpers::ScopedSetting scoped(CoreSettings::RECOVERY_EXECUTE_VISUAL_SHADER_NODE_SCRIPTS, true);
		Error err = OK;
		Ref<Resource> converted = converter->convert(missing, ResourceInfo::REAL_LOAD, GODOT_VERSION_MAJOR, &err);
		REQUIRE(converted.is_valid());
		CHECK(Object::cast_to<VisualShaderNodeCustom>(converted.ptr()) != nullptr);
		Ref<GDScript> real_script = get_real_script(converted);
		REQUIRE(real_script.is_valid());
		CHECK(real_script->get_source_code().contains("ConfigTestNode"));
	}
	SUBCASE("Enabled, but not a real load: the script is still not executed") {
		TestConfigHelpers::ScopedSetting scoped(CoreSettings::RECOVERY_EXECUTE_VISUAL_SHADER_NODE_SCRIPTS, true);
		Error err = OK;
		Ref<Resource> converted = converter->convert(missing, ResourceInfo::FAKE_LOAD, GODOT_VERSION_MAJOR, &err);
		REQUIRE(converted.is_valid());
		CHECK(get_real_script(converted).is_null());
	}
}

} // namespace TestResourceExport
