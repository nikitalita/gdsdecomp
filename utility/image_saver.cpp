#include "image_saver.h"

#include "core/io/image.h"
#include "core/io/file_access.h"
#include "core/string/ustring.h"
#include "core/variant/variant.h"
#include "external/tga/tga.h"
#include "utility/common.h"
#include "compat/file_access_encrypted_v3.h"
#include "external/tga/tga.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "vtracer/vtracer.h"


bool ImageSaver::dest_format_supports_mipmaps(const String &ext) {
	return ext == "dds" || ext == "exr";
}


Error ImageSaver::save_image(const String &dest_path, const Ref<Image> &img, bool lossy, float quality) {
	ERR_FAIL_COND_V_MSG(img->is_empty(), ERR_FILE_EOF, "Image data is empty for texture " + dest_path + ", not saving");
	Error err = gdre::ensure_dir(dest_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dest_path);
	String dest_ext = dest_path.get_extension().to_lower();
	if (!dest_format_supports_mipmaps(dest_ext) && img->is_compressed() && img->has_mipmaps()) {
		img->clear_mipmaps();
	}
	GDRE_ERR_DECOMPRESS_OR_FAIL(img);
	if (dest_ext == "jpg" || dest_ext == "jpeg") {
		err = img->save_jpg(dest_path, quality);
	} else if (dest_ext == "webp") {
		err = img->save_webp(dest_path, lossy, quality);
	} else if (dest_ext == "png") {
		err = img->save_png(dest_path);
	} else if (dest_ext == "tga") {
		err = ImageSaver::save_image_as_tga(dest_path, img);
	} else if (dest_ext == "svg") {
		err = ImageSaver::save_image_as_svg(dest_path, img);
	} else if (dest_ext == "dds") {
		err = img->save_dds(dest_path);
	} else if (dest_ext == "exr") {
		err = img->save_exr(dest_path);
	} else if (dest_ext == "bmp") {
		err = ImageSaver::save_image_as_bmp(dest_path, img);
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_BAD_PATH, "Invalid file name: " + dest_path);
	}
	return err;
}

Error ImageSaver::decompress_image(const Ref<Image> &img) {
	Error err;
	if (img->is_compressed()) {
		err = img->decompress();
		if (err == ERR_UNAVAILABLE) {
			return err;
		}
		ERR_FAIL_COND_V_MSG(err != OK || img.is_null(), err, "Failed to decompress image.");
	}
	return OK;
}

class GodotFileInterface : public tga::FileInterface {
	Ref<FileAccess> m_file;

public:
	GodotFileInterface(const String &p_path, FileAccess::ModeFlags p_mode) {
		m_file = FileAccess::open(p_path, p_mode);
	}

	// Returns true if we can read/write bytes from/into the file
	virtual bool ok() const override {
		return m_file.is_valid();
	}

	// Current position in the file
	virtual size_t tell() override {
		return m_file->get_position();
	}

	// Jump to the given position in the file
	virtual void seek(size_t absPos) override {
		m_file->seek(absPos);
	}

	// Returns the next byte in the file or 0 if ok() = false
	virtual uint8_t read8() override {
		return m_file->get_8();
	}

	// Writes one byte in the file (or do nothing if ok() = false)
	virtual void write8(uint8_t value) override {
		m_file->store_8(value);
	}
};

Error ImageSaver::save_image_as_bmp(const String &p_path, const Ref<Image> &p_img) {
	// Microsoft BMP format - BGR ordering
	Ref<Image> source_image = p_img->duplicate();
	GDRE_ERR_DECOMPRESS_OR_FAIL(source_image);

	int width = source_image->get_width();
	int height = source_image->get_height();

	// Convert to RGBA8 if needed
	bool has_alpha = source_image->detect_alpha();
	if (source_image->get_format() != Image::FORMAT_RGBA8 && source_image->get_format() != Image::FORMAT_RGB8) {
		if (has_alpha) {
			source_image->convert(Image::FORMAT_RGBA8);
		} else {
			source_image->convert(Image::FORMAT_RGB8);
		}
	} else if (!has_alpha && source_image->get_format() == Image::FORMAT_RGBA8) {
		source_image->convert(Image::FORMAT_RGB8);
	}
	int pixel_stride = has_alpha ? 4 : 3;

	// Determine BMP format based on alpha presence
	bool use_32bit = has_alpha;
	int bytes_per_pixel = use_32bit ? 4 : 3;
	int row_size = width * bytes_per_pixel;
	int padding_size = use_32bit ? 0 : (4 - (row_size % 4)) % 4; // 32-bit has no padding
	int padded_row_size = row_size + padding_size;

	// BMP file header (14 bytes)
	uint32_t info_header_size = use_32bit ? 108 : 40; // V4 header (108 bytes) for 32-bit, V3 header (40 bytes) for 24-bit
	uint32_t file_size = 14 + info_header_size + (padded_row_size * height);
	uint32_t pixel_data_offset = 14 + info_header_size;

	// BMP info header
	uint16_t planes = 1;
	uint16_t bits_per_pixel = use_32bit ? 32 : 24;
	uint32_t compression = use_32bit ? 3 : 0; // BI_BITFIELDS for 32-bit, BI_RGB for 24-bit
	uint32_t image_size = padded_row_size * height;
	uint32_t x_pixels_per_meter = 2835; // 72 DPI
	uint32_t y_pixels_per_meter = 2835; // 72 DPI
	uint32_t colors_used = 0;
	uint32_t important_colors = 0;

	Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::WRITE);
	if (fa.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}

	// Write file header
	fa->store_16(0x4D42); // "BM" signature
	fa->store_32(file_size);
	fa->store_16(0); // Reserved
	fa->store_16(0); // Reserved
	fa->store_32(pixel_data_offset);

	// Write info header
	fa->store_32(info_header_size);
	fa->store_32(width);
	fa->store_32(height);
	fa->store_16(planes);
	fa->store_16(bits_per_pixel);
	fa->store_32(compression);
	fa->store_32(image_size);
	fa->store_32(x_pixels_per_meter);
	fa->store_32(y_pixels_per_meter);
	fa->store_32(colors_used);
	fa->store_32(important_colors);

	// Write V4 header fields for 32-bit format
	if (use_32bit) {
		// Color masks (same as V3 but in V4 header)
		fa->store_32(0x00FF0000); // Blue mask
		fa->store_32(0x0000FF00); // Green mask
		fa->store_32(0x000000FF); // Red mask
		fa->store_32(0xFF000000); // Alpha mask

		// Color space type (linear RGB = 0x4C494E45)
		fa->store_32(0x4C494E45); // "LINE"

		// CIEXYZTRIPLE for endpoints (unused for linear RGB)
		for (int i = 0; i < 9; i++) {
			fa->store_32(0); // 3 endpoints × 3 coordinates × 4 bytes each
		}

		// Gamma values (unused for linear RGB)
		fa->store_32(0); // Red gamma
		fa->store_32(0); // Green gamma
		fa->store_32(0); // Blue gamma
	}

	// Write pixel data (BMP stores rows bottom-up)
	Vector<uint8_t> image_data = source_image->get_data();
	constexpr uint8_t padding[4] = { 0, 0, 0, 0 }; // Padding bytes

	for (int y = height - 1; y >= 0; y--) { // Bottom-up order
		for (int x = 0; x < width; x++) {
			int src_index = (y * width + x) * pixel_stride;
			uint8_t r = image_data[src_index + 0];
			uint8_t g = image_data[src_index + 1];
			uint8_t b = image_data[src_index + 2];

			if (use_32bit) {
				uint8_t a = image_data[src_index + 3];
				fa->store_8(b);
				fa->store_8(g);
				fa->store_8(r);
				fa->store_8(a);
			} else {
				fa->store_8(b);
				fa->store_8(g);
				fa->store_8(r);
			}
		}

		// Write row padding (only for 24-bit format)
		if (!use_32bit && padding_size > 0) {
			fa->store_buffer(padding, padding_size);
		}
	}

	fa->close();
	return OK;
}

Error ImageSaver::save_image_as_tga(const String &p_path, const Ref<Image> &p_img) {
	Vector<uint8_t> buffer;
	Ref<Image> source_image = p_img->duplicate();
	GDRE_ERR_DECOMPRESS_OR_FAIL(source_image);
	int width = source_image->get_width();
	int height = source_image->get_height();
	bool isRGB = true;
	bool isAlpha = source_image->detect_alpha();
	switch (source_image->get_format()) {
		case Image::FORMAT_L8:
			isRGB = false;
			break;
		case Image::FORMAT_LA8:
			isRGB = true;
			source_image->convert(Image::FORMAT_RGBA8);
			break;
		case Image::FORMAT_RGB8:
			// we still need to convert it to RGBA8 even if it doesn't have alpha due to the encoder requiring 4 bytes per pixel
			source_image->convert(Image::FORMAT_RGBA8);
			break;
		case Image::FORMAT_RGBA8:
			break;
		default:
			source_image->convert(Image::FORMAT_RGBA8);
			isRGB = true;
			break;
	}

	tga::Header header;
	header.idLength = 0;
	header.colormapType = 0;
	header.imageType = isRGB ? tga::ImageType::RleRgb : tga::ImageType::RleGray;
	header.colormapOrigin = 0;
	header.colormapLength = 0;
	header.colormapDepth = 0;

	header.xOrigin = 0;
	header.yOrigin = 0;
	header.width = width;
	header.height = height;
	header.bitsPerPixel = isRGB ? (isAlpha ? 32 : 24) : 8;
	header.imageDescriptor = isAlpha ? 0xf : 0; // top-left origin always
	tga::Image tga_image{};
	Vector<uint8_t> tga_data = source_image->get_data(); //isRGB ? rgba_to_bgra(source_image->get_data()) : source_image->get_data();
	tga_image.pixels = tga_data.ptrw();
	tga_image.bytesPerPixel = isRGB ? 4 : 1;
	tga_image.rowstride = width * tga_image.bytesPerPixel;
	GodotFileInterface file_interface(p_path, FileAccess::WRITE);
	if (!file_interface.ok()) {
		return ERR_FILE_CANT_WRITE;
	}
	tga::Encoder encoder(&file_interface);
	encoder.writeHeader(header);
	encoder.writeImage(header, tga_image);
	encoder.writeFooter();
	return OK;
}

Error ImageSaver::save_image_as_svg(const String &p_path, const Ref<Image> &p_img) {
	VTracerConfig config;
	vtracer_set_default_config(&config);
	// this config converts the raster image to a vector image with a box for each pixel
	// this will ensure that the image will match the original image when re-imported into Godot
	config.color_mode = V_TRACER_COLOR_MODE_COLOR;
	config.hierarchical = V_TRACER_HIERARCHICAL_STACKED;
	config.mode = V_TRACER_PATH_SIMPLIFY_MODE_NONE;
	config.filter_speckle = 0;
	config.color_precision = 8;
	config.layer_difference = 0;
	config.corner_threshold = 60;
	config.length_threshold = 4.0;
	config.splice_threshold = 45;
	config.max_iterations = 10;
	config.path_precision = 2;
	config.keying_threshold = 0.0;

	Ref<Image> img = p_img->duplicate();
	GDRE_ERR_DECOMPRESS_OR_FAIL(img);
	// check if the image is RGBA; if not, convert it to RGBA
	if (img->get_format() != Image::FORMAT_RGBA8) {
		img->convert(Image::FORMAT_RGBA8);
	}
	auto data = img->get_data();
	VTracerColorImage svg_data{
		data.ptrw(),
		(size_t)img->get_width(),
		(size_t)img->get_height(),
	};
	const char *err_str = vtracer_convert_image_memory_to_svg(&svg_data, p_path.utf8().get_data(), &config);
	String err_msg;
	if (err_str) {
		err_msg = vformat("Failed to convert image to SVG: %s", err_str);
		vtracer_free_string(err_str);
	}
	ERR_FAIL_COND_V_MSG(!err_msg.is_empty(), ERR_CANT_CREATE, err_msg);
	return OK;
}

void ImageSaver::_bind_methods() {
	ClassDB::bind_static_method(get_class_static(), D_METHOD("decompress_image", "image"), &ImageSaver::decompress_image);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("save_image", "dest_path", "image", "lossy", "quality"), &ImageSaver::save_image, DEFVAL(false), DEFVAL(1.0));
	ClassDB::bind_static_method(get_class_static(), D_METHOD("save_image_as_tga", "dest_path", "image"), &ImageSaver::save_image_as_tga);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("save_image_as_svg", "dest_path", "image"), &ImageSaver::save_image_as_svg);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("save_image_as_bmp", "dest_path", "image"), &ImageSaver::save_image_as_bmp);
}
