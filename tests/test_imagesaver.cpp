#include "test_common.h"

#include "core/io/image.h"
#include "tests/test_macros.h"
#include "utility/common.h"
#include "utility/gdre_settings.h"
#include "utility/image_saver.h"

TEST_FORCE_LINK(test_imagesaver)
namespace TestImageSaver {

static inline String get_color_string(const Color &color) {
	return vformat("(r: %d, g: %d, b: %d, a: %d)", color.get_r8(), color.get_g8(), color.get_b8(), color.get_a8());
}

void test_svg_saving(const String &file, const String &test_files_dir, const String &output_dir) {
	Ref<Image> img = gdre::load_image_from_file(test_files_dir.path_join(file));
	REQUIRE(img.is_valid());
	CHECK(ImageSaver::save_image(output_dir.path_join(file), img, false) == OK);

	Ref<Image> resaved_image = gdre::load_image_from_file(output_dir.path_join(file));
	REQUIRE(resaved_image.is_valid());
	if (resaved_image->get_data() != img->get_data()) {
		// save them both as pngs to the output path
		Ref<Image> diff_image = Image::create_empty(img->get_width(), img->get_height(), false, Image::FORMAT_RGBA8);
		Vector<Pair<Vector2i, Pair<Color, Color>>> diff_colors;
		for (int i = 0; i < img->get_width(); i++) {
			for (int j = 0; j < img->get_height(); j++) {
				Color img_color = img->get_pixel(i, j);
				Color resaved_color = resaved_image->get_pixel(i, j);
				if (abs(img_color.get_r8() - resaved_color.get_r8()) > 1 ||
						abs(img_color.get_g8() - resaved_color.get_g8()) > 1 ||
						abs(img_color.get_b8() - resaved_color.get_b8()) > 1 ||
						abs(img_color.get_a8() - resaved_color.get_a8()) > 1) {
					Color diff = img_color - resaved_color;
					diff.r = abs(diff.r);
					diff.g = abs(diff.g);
					diff.b = abs(diff.b);
					diff.a = abs(diff.a);
					if (diff.a == 0) {
						diff.a = 1;
					}
					diff_image->set_pixel(i, j, diff);
					diff_colors.push_back({ Vector2i(i, j), { img->get_pixel(i, j), resaved_image->get_pixel(i, j) } });
				} else {
					diff_image->set_pixel(i, j, Color(0, 0, 0, 0));
				}
			}
		}
		String error_message = "";
		if (diff_colors.size() > 0) {
#ifdef DEBUG_ENABLED
			auto a_path = output_dir.path_join(file.get_basename() + ".png");
			auto b_path = output_dir.path_join(file.get_basename() + "_resaved.png");
			img->save_png(a_path);
			resaved_image->save_png(b_path);
			diff_image->save_png(output_dir.path_join(file.get_basename() + "_diff.png"));
#endif
			error_message = vformat("%d pixels differ in %s: ", diff_colors.size(), file);
			for (const auto &diff : diff_colors) {
				error_message += vformat("Diff at %d, %d: %s vs %s", diff.first.x, diff.first.y, get_color_string(diff.second.first), get_color_string(diff.second.second)) + "\n";
			}
		}
		CHECK(error_message == "");
	}

	// the svg that we save should be pixel-for-pixel accurate to the original when loaded as raster images
	// CHECK(resaved_image->get_data() == img->get_data());
}
} //namespace TestImageSaver

TEST_CASE("[GDSDecomp][ImageSaver] Test SVG saving") {
	auto editor_icons_path = GDRESettings::get_singleton()->get_cwd().path_join("editor/icons");
	auto output_path = get_tmp_path().path_join("image_saver");
	gdre::rimraf(output_path);
	gdre::ensure_dir(output_path);
	auto files = gdre::get_files_at(editor_icons_path, { "*.svg" }, false);
	for (const String &file : files) {
		SUBCASE(file.utf8().get_data()) {
			TestImageSaver::test_svg_saving(file, editor_icons_path, output_path);
		}
	}
}

TEST_CASE("[GDSDecomp][ImageSaver] Test saving images") {
	Ref<Image> img = Image::create_empty(100, 100, false, Image::FORMAT_RGB8);
	img->fill(Color(1, 0, 0));
	String temp_dir = get_tmp_path().path_join("image_saver");
	gdre::rimraf(temp_dir);
	gdre::ensure_dir(temp_dir);
	auto exts = ImageSaver::get_supported_extensions();
	for (const String &ext : exts) {
		SUBCASE(vformat("Image saver can save in format %s", ext).utf8().get_data()) {
			String temp_path = temp_dir.path_join(vformat("test.%s", ext));
			CHECK(ImageSaver::save_image(temp_path, img, false) == OK);
			CHECK(FileAccess::get_file_as_bytes(temp_path).size() > 0);
		}
	}
}

TEST_CASE("[GDSDecomp][ImageSaver] Test saving images with compression") {
	String temp_dir = get_tmp_path().path_join("image_saver");
	gdre::rimraf(temp_dir);
	gdre::ensure_dir(temp_dir);
	Vector<Image::Format> compress_modes = {
		Image::FORMAT_DXT1, // BC1
		Image::FORMAT_DXT3, // BC2
		Image::FORMAT_DXT5, // BC3
		Image::FORMAT_RGTC_R, // BC4
		Image::FORMAT_RGTC_RG, // BC5
		Image::FORMAT_BPTC_RGBA, // BC7
		Image::FORMAT_BPTC_RGBF, // BC6 Signed
		Image::FORMAT_BPTC_RGBFU, // BC6 Unsigned
		Image::FORMAT_ETC, // ETC1
		Image::FORMAT_ETC2_R11,
		// Image::FORMAT_ETC2_R11S, // Signed, NOT srgb.
		Image::FORMAT_ETC2_RG11,
		// Image::FORMAT_ETC2_RG11S, // Signed, NOT srgb.
		Image::FORMAT_ETC2_RGB8,
		Image::FORMAT_ETC2_RGBA8,
		// Image::FORMAT_ETC2_RGB8A1,
		Image::FORMAT_ETC2_RA_AS_RG, // ETC2 RGBA with a RA-RG swizzle for normal maps.
		Image::FORMAT_DXT5_RA_AS_RG, // BC3 with a RA-RG swizzle for normal maps.
		Image::FORMAT_ASTC_4x4,
		Image::FORMAT_ASTC_4x4_HDR,
		Image::FORMAT_ASTC_8x8,
		Image::FORMAT_ASTC_8x8_HDR,
	};
	for (const auto &p_format : compress_modes) {
		String ext = Image::format_names[p_format];
		SUBCASE(vformat("Image saver can save images compressed with %s", ext).utf8().get_data()) {
			Ref<Image> img = Image::create_empty(128, 128, false, p_format);
			String temp_path = temp_dir.path_join(vformat("test_compressed_%s.png", ext));
			CHECK(ImageSaver::save_image(temp_path, img, false) == OK);
			CHECK(FileAccess::get_file_as_bytes(temp_path).size() > 0);
		}
	}
}
