#include "fontfile_exporter.h"

#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "exporters/export_report.h"
#include "utility/common.h"
#include "utility/image_saver.h"

static const HashSet<String> dynamic_font_supported_extensions = { "otf", "ttf", "woff", "woff2", "ttc", "otc", "pfb", "pfm" };
static const HashSet<String> bitmap_font_supported_extensions = { "fnt", "font" };
Error FontFileExporter::export_file(const String &p_dest_path, const String &p_src_path) {
	String ext = p_dest_path.get_extension().to_lower();

	if (dynamic_font_supported_extensions.has(ext)) {
		return _export_font_data_dynamic(p_dest_path, p_src_path);
	} else if (bitmap_font_supported_extensions.has(ext)) {
		Ref<FontFile> fontfile;
		return _export_bitmap_font(p_dest_path, p_src_path, fontfile);
	}
	Ref<Image> r_image;
	return _export_image(p_dest_path, p_src_path, r_image);
}

Error FontFileExporter::_export_font_data_dynamic(const String &p_dest_path, const String &p_src_path) {
	Error err;
	Ref<Resource> fontfile = ResourceCompatLoader::fake_load(p_src_path, "", &err);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to load font file " + p_src_path);
	PackedByteArray data = fontfile->get("data");
	ERR_FAIL_COND_V_MSG(data.size() == 0, ERR_FILE_CORRUPT, "Font file " + p_src_path + " is empty");
	return write_to_file(p_dest_path, data);
}

Error FontFileExporter::_export_image(const String &p_dest_path, const String &p_src_path, Ref<Image> &r_image) {
	Error err;
	Ref<FontFile> fontfile = ResourceCompatLoader::non_global_load(p_src_path, "", &err);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to load font file " + p_src_path);
	r_image = fontfile->get_texture_image(0, {}, 0);
	ERR_FAIL_COND_V_MSG(r_image.is_null(), ERR_FILE_CORRUPT, "Font file " + p_src_path + " is not an image");
	r_image = r_image->duplicate();
	return ImageSaver::save_image(p_dest_path, r_image, !ImageSaver::ver_supports_lossless_webp(ResourceInfo::get_ver_major_minor(fontfile)), 1.0, false);
}

Error FontFileExporter::_export_bitmap_font(const String &p_dest_path, const String &p_src_path, Ref<FontFile> &r_fontfile) {
	Error err;
	r_fontfile = ResourceCompatLoader::custom_load(p_src_path, "", ResourceInfo::LoadType::GLTF_LOAD, &err, false, ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP);
	ERR_FAIL_COND_V_MSG(err || r_fontfile.is_null(), ERR_CANT_OPEN, "Failed to load font file " + p_src_path);

	const int cache_index = 0;
	int base_size = r_fontfile->get_fixed_size();
	if (base_size == 0) {
		base_size = 16; // Default size
	}
	const Vector2i size(base_size, 0);

	// Get font properties
	String font_name = r_fontfile->get_font_name();
	if (font_name.is_empty()) {
		font_name = "Untitled";
	}
	BitField<TextServer::FontStyle> font_style = r_fontfile->get_font_style();
	real_t ascent = r_fontfile->get_cache_ascent(cache_index, base_size);
	real_t descent = r_fontfile->get_cache_descent(cache_index, base_size);
	int line_height = (int)(ascent + descent);

	// Get texture count and images
	int texture_count = r_fontfile->get_texture_count(cache_index, size);
	ERR_FAIL_COND_V_MSG(texture_count == 0, ERR_FILE_CORRUPT, "Font has no textures");

	// Check for outline textures
	const Vector2i outline_size(base_size, 1);
	int outline_texture_count = r_fontfile->get_texture_count(cache_index, outline_size);
	bool has_outline = (outline_texture_count > 0);

	// Get glyph list
	PackedInt32Array glyph_list = r_fontfile->get_glyph_list(cache_index, size);

	// Get kerning pairs
	TypedArray<Vector2i> kerning_pairs = r_fontfile->get_kerning_list(cache_index, base_size);

	// Detect format by examining first texture
	Ref<Image> first_texture = r_fontfile->get_texture_image(cache_index, size, 0);
	ERR_FAIL_COND_V_MSG(first_texture.is_null(), ERR_FILE_CORRUPT, "First texture is null");

	int texture_width = first_texture->get_width();
	int texture_height = first_texture->get_height();
	Image::Format first_format = first_texture->get_format();

#if 0
	// Detect if packed format (texture indices are multiples of 4, and we have at least 4 textures)
	bool is_packed = (texture_count % 4 == 0) && (texture_count >= 4) && first_format == Image::FORMAT_LA8;
	// Check if textures match packed pattern by examining glyph texture indices
	if (is_packed && glyph_list.size() > 0) {
		// Sample a few glyphs to see if texture indices follow packed pattern
		int packed_sample_count = 0;
		int non_packed_count = 0;
		for (int i = 0; i < MIN(glyph_list.size(), 10); i++) {
			int32_t glyph_index = glyph_list[i];
			int texture_idx = r_fontfile->get_glyph_texture_idx(cache_index, size, glyph_index);
			if (texture_idx < texture_count && texture_idx % 4 < 4) {
				packed_sample_count++;
			} else {
				non_packed_count++;
			}
		}
		// If most samples don't follow packed pattern, it's not packed
		if (non_packed_count > packed_sample_count) {
			is_packed = false;
		}
	}
#else
	// disable packed for now, Godot doesn't support them, no point in checking for them
	bool is_packed = false;
#endif

	if (is_packed) {
		return ERR_UNAVAILABLE;
	}

	// Detect format type
	bool is_monochrome = (first_format == Image::FORMAT_LA8 || first_format == Image::FORMAT_L8);

	// Detect channel mappings for non-packed formats
	uint8_t ch[4] = { 0, 0, 0, 0 }; // RGBA channel mappings
	int first_gl_ch = -1; // First channel with glyph data (0)
	int first_ol_ch = -1; // First channel with outline data (1)

	if (!is_packed && has_outline) {
		// Check which format: separate glyph/outline textures or combined
		// If we have outline textures, determine if they're separate or combined
		if (outline_texture_count == texture_count) {
			// Separate outline textures - monochrome 8-bit with outline
			first_gl_ch = 0; // We'll use red channel for glyph
			first_ol_ch = 1; // We'll use green channel for outline
		}
		// Note: Combined format (4-bit) handling can be added later if needed
	}

	// Determine outline thickness (estimate from existence of outline textures)
	uint8_t outline_thickness = has_outline ? 1 : 0;

	// Save texture images and collect filenames
	Vector<String> texture_filenames;
	String base_dir = p_dest_path.get_base_dir();
	String base_name = p_dest_path.get_file().get_basename();

	// Determine actual page count (for packed, divide by 4)
	int actual_page_count = is_packed ? (texture_count / 4) : texture_count;

	if (is_packed) {
		// Packed format: combine 4 LA8 textures back into one RGBA8
		for (int page = 0; page < actual_page_count; page++) {
			Ref<Image> img_r = r_fontfile->get_texture_image(cache_index, size, page * 4 + 0);
			Ref<Image> img_g = r_fontfile->get_texture_image(cache_index, size, page * 4 + 1);
			Ref<Image> img_b = r_fontfile->get_texture_image(cache_index, size, page * 4 + 2);
			Ref<Image> img_a = r_fontfile->get_texture_image(cache_index, size, page * 4 + 3);

			if (img_r.is_null() || img_g.is_null() || img_b.is_null() || img_a.is_null()) {
				continue; // Skip incomplete packed textures
			}

			int w = img_r->get_width();
			int h = img_r->get_height();

			// Extract data from LA8 textures (or convert if needed)
			PackedByteArray data_r, data_g, data_b, data_a;
			const uint8_t *r_ptr, *g_ptr, *b_ptr, *a_ptr;

			if (img_r->get_format() == Image::FORMAT_LA8) {
				data_r = img_r->get_data();
				r_ptr = data_r.ptr();
			} else {
				Ref<Image> img_r_conv = img_r->duplicate();
				img_r_conv->convert(Image::FORMAT_LA8);
				data_r = img_r_conv->get_data();
				r_ptr = data_r.ptr();
			}

			if (img_g->get_format() == Image::FORMAT_LA8) {
				data_g = img_g->get_data();
				g_ptr = data_g.ptr();
			} else {
				Ref<Image> img_g_conv = img_g->duplicate();
				img_g_conv->convert(Image::FORMAT_LA8);
				data_g = img_g_conv->get_data();
				g_ptr = data_g.ptr();
			}

			if (img_b->get_format() == Image::FORMAT_LA8) {
				data_b = img_b->get_data();
				b_ptr = data_b.ptr();
			} else {
				Ref<Image> img_b_conv = img_b->duplicate();
				img_b_conv->convert(Image::FORMAT_LA8);
				data_b = img_b_conv->get_data();
				b_ptr = data_b.ptr();
			}

			if (img_a->get_format() == Image::FORMAT_LA8) {
				data_a = img_a->get_data();
				a_ptr = data_a.ptr();
			} else {
				Ref<Image> img_a_conv = img_a->duplicate();
				img_a_conv->convert(Image::FORMAT_LA8);
				data_a = img_a_conv->get_data();
				a_ptr = data_a.ptr();
			}

			// Combine LA8 textures into RGBA8
			// LA8 format: [alpha, luminance] per pixel (2 bytes)
			// _convert_packed_8bit stores: [255, channel_value]
			// So we extract the luminance byte (index 1) which contains the original channel value
			PackedByteArray combined_data;
			combined_data.resize(w * h * 4);
			uint8_t *combined_ptr = combined_data.ptrw();

			for (int i = 0; i < w * h; i++) {
				combined_ptr[i * 4 + 0] = r_ptr[i * 2 + 1]; // R from R texture luminance
				combined_ptr[i * 4 + 1] = g_ptr[i * 2 + 1]; // G from G texture luminance
				combined_ptr[i * 4 + 2] = b_ptr[i * 2 + 1]; // B from B texture luminance
				combined_ptr[i * 4 + 3] = a_ptr[i * 2 + 1]; // A from A texture luminance
			}

			Ref<Image> combined = Image::create_from_data(w, h, false, Image::FORMAT_RGBA8, combined_data);

			String texture_filename = actual_page_count == 1 ? (base_name + ".png") : (base_name + "_" + String::num_int64(page) + ".png");
			String texture_path = base_dir.path_join(texture_filename);

			err = ImageSaver::save_image(texture_path, combined, false, 1.0, false);
			ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save texture " + texture_path);

			texture_filenames.push_back(texture_filename);
		}
	} else {
		// Non-packed format: export textures directly
		if (is_monochrome && has_outline && first_gl_ch >= 0 && first_ol_ch >= 0) {
			// Monochrome 8-bit with separate outline textures - combine glyph and outline
			for (int i = 0; i < texture_count; i++) {
				Ref<Image> glyph_img = r_fontfile->get_texture_image(cache_index, size, i);
				Ref<Image> outline_img = (i < outline_texture_count) ? r_fontfile->get_texture_image(cache_index, outline_size, i) : Ref<Image>();

				ERR_CONTINUE_MSG(glyph_img.is_null(), vformat("Glyph texture %d is null", i));

				int w = glyph_img->get_width();
				int h = glyph_img->get_height();

				// Extract glyph data
				PackedByteArray glyph_data;
				if (glyph_img->get_format() == Image::FORMAT_LA8) {
					glyph_data = glyph_img->get_data();
				} else {
					// Convert L8 to LA8 format
					glyph_img = glyph_img->duplicate();
					glyph_img->convert(Image::FORMAT_LA8);
					glyph_data = glyph_img->get_data();
				}

				// Extract outline data
				PackedByteArray outline_data;
				if (outline_img.is_valid()) {
					if (outline_img->get_format() == Image::FORMAT_LA8) {
						outline_data = outline_img->get_data();
					} else if (outline_img->get_format() == Image::FORMAT_L8) {
						PackedByteArray l_data = outline_img->get_data();
						outline_data.resize(w * h * 2);
						const uint8_t *l_ptr = l_data.ptr();
						uint8_t *la_ptr = outline_data.ptrw();
						for (int j = 0; j < w * h; j++) {
							la_ptr[j * 2 + 0] = 255;
							la_ptr[j * 2 + 1] = l_ptr[j];
						}
					} else {
						outline_img = outline_img->duplicate();
						outline_img->convert(Image::FORMAT_LA8);
						outline_data = outline_img->get_data();
					}
				}

				// Create RGBA8 image with glyph in red channel and outline in green channel
				PackedByteArray rgba_data;
				rgba_data.resize(w * h * 4);
				const uint8_t *gl_ptr = glyph_data.ptr();
				const uint8_t *ol_ptr = outline_data.is_empty() ? nullptr : outline_data.ptr();
				uint8_t *rgba_ptr = rgba_data.ptrw();

				for (int j = 0; j < w * h; j++) {
					rgba_ptr[j * 4 + 0] = gl_ptr[j * 2 + 1]; // R = glyph luminance
					rgba_ptr[j * 4 + 1] = (ol_ptr != nullptr) ? ol_ptr[j * 2 + 1] : 0; // G = outline luminance
					rgba_ptr[j * 4 + 2] = 0; // B = unused
					rgba_ptr[j * 4 + 3] = 255; // A = opaque
				}

				Ref<Image> combined = Image::create_from_data(w, h, false, Image::FORMAT_RGBA8, rgba_data);

				String texture_filename = actual_page_count == 1 ? (base_name + ".png") : (base_name + "_" + String::num_int64(i) + ".png");
				String texture_path = base_dir.path_join(texture_filename);

				err = ImageSaver::save_image(texture_path, combined, false, 1.0, false);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save texture " + texture_path);

				texture_filenames.push_back(texture_filename);
			}

			// Set channel mappings for monochrome with outline
			ch[0] = 0; // red = glyph
			ch[1] = 1; // green = outline
			ch[2] = 3; // blue = unused (set to zero)
			ch[3] = 3; // alpha = unused (set to zero)
		} else if (!is_monochrome && has_outline) {
			// RGBA4 color format with outline - combine glyph and outline textures
			for (int i = 0; i < texture_count; i++) {
				Ref<Image> glyph_img = r_fontfile->get_texture_image(cache_index, size, i);
				Ref<Image> outline_img = (i < outline_texture_count) ? r_fontfile->get_texture_image(cache_index, outline_size, i) : Ref<Image>();

				ERR_CONTINUE_MSG(glyph_img.is_null(), vformat("Glyph texture %d is null", i));

				// Convert both to RGBA8 if needed
				if (glyph_img->get_format() != Image::FORMAT_RGBA8) {
					glyph_img = glyph_img->duplicate();
					glyph_img->convert(Image::FORMAT_RGBA8);
				}
				if (outline_img.is_valid() && outline_img->get_format() != Image::FORMAT_RGBA8) {
					outline_img = outline_img->duplicate();
					outline_img->convert(Image::FORMAT_RGBA8);
				}

				int w = glyph_img->get_width();
				int h = glyph_img->get_height();

				PackedByteArray glyph_data = glyph_img->get_data();
				PackedByteArray outline_data = outline_img.is_valid() ? outline_img->get_data() : PackedByteArray();

				const uint8_t *gl_ptr = glyph_data.ptr();
				const uint8_t *ol_ptr = outline_data.is_empty() ? nullptr : outline_data.ptr();

				// Combine glyph and outline into RGBA8 format
				// Reverse of _convert_rgba_4bit:
				// - Source value > 0x7F → stored in glyph texture as-is, outline = 0
				// - Source value <= 0x7F → glyph = 0, stored in outline texture as (value * 2)
				// So to reverse:
				// - If glyph > 0x7F: use glyph (original was > 0x7F)
				// - Else if outline > 0: use outline/2 (original was <= 0x7F, non-zero)
				// - Else: use 0 (original was 0)
				PackedByteArray combined_data;
				combined_data.resize(w * h * 4);
				uint8_t *combined_ptr = combined_data.ptrw();

				for (int j = 0; j < w * h; j++) {
					for (int ch = 0; ch < 4; ch++) {
						uint8_t gl_val = gl_ptr[j * 4 + ch];
						uint8_t ol_val = (ol_ptr != nullptr) ? ol_ptr[j * 4 + ch] : 0;

						if (gl_val > 0x7F) {
							// Glyph data - original value was > 0x7F, stored as-is
							combined_ptr[j * 4 + ch] = gl_val;
						} else if (ol_val > 0) {
							// Outline data - original value was <= 0x7F, stored as (value * 2)
							combined_ptr[j * 4 + ch] = ol_val / 2;
						} else {
							// No data - original value was 0
							combined_ptr[j * 4 + ch] = 0;
						}
					}
				}

				Ref<Image> combined = Image::create_from_data(w, h, false, Image::FORMAT_RGBA8, combined_data);

				String texture_filename = actual_page_count == 1 ? (base_name + ".png") : (base_name + "_" + String::num_int64(i) + ".png");
				String texture_path = base_dir.path_join(texture_filename);

				err = ImageSaver::save_image(texture_path, combined, false, 1.0, false);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save texture " + texture_path);

				texture_filenames.push_back(texture_filename);
			}

			// Set channel mappings for RGBA4 with outline (all channels = 2 = combined)
			ch[0] = 2; // red = combined
			ch[1] = 2; // green = combined
			ch[2] = 2; // blue = combined
			ch[3] = 2; // alpha = combined
		} else {
			// Simple format: export textures directly
			for (int i = 0; i < texture_count; i++) {
				Ref<Image> texture_img = r_fontfile->get_texture_image(cache_index, size, i);
				ERR_CONTINUE_MSG(texture_img.is_null(), vformat("Texture %d is null", i));

				Ref<Image> export_img = texture_img->duplicate();

				if (is_monochrome) {
					// Convert LA8/L8 to L8 or extract single channel
					if (export_img->get_format() == Image::FORMAT_LA8) {
						// Extract luminance channel to create L8
						int w = export_img->get_width();
						int h = export_img->get_height();
						PackedByteArray la_data = export_img->get_data();
						PackedByteArray l_data;
						l_data.resize(w * h);

						const uint8_t *la_ptr = la_data.ptr();
						uint8_t *l_ptr = l_data.ptrw();

						for (int j = 0; j < w * h; j++) {
							l_ptr[j] = la_ptr[j * 2 + 1]; // Luminance is in second byte
						}

						export_img = Image::create_from_data(w, h, false, Image::FORMAT_L8, l_data);
					} else if (export_img->get_format() == Image::FORMAT_L8) {
						// Already L8, keep as is
					} else {
						// Convert to RGBA8 for color
						export_img->convert(Image::FORMAT_RGBA8);
					}

					// Set channel mappings for monochrome (no outline)
					if (first_gl_ch < 0) {
						first_gl_ch = 0; // Use red channel
						ch[0] = 0; // red = glyph
						ch[1] = 3; // green = zero
						ch[2] = 3; // blue = zero
						ch[3] = 3; // alpha = zero
					}
				} else {
					// Color format, convert to RGBA8 if needed
					if (export_img->get_format() != Image::FORMAT_RGBA8) {
						export_img->convert(Image::FORMAT_RGBA8);
					}

					// Set channel mappings for color (no outline)
					ch[0] = 0; // red = glyph
					ch[1] = 0; // green = glyph
					ch[2] = 0; // blue = glyph
					ch[3] = 0; // alpha = glyph
				}

				// Generate texture filename
				String texture_filename;
				if (actual_page_count == 1) {
					texture_filename = base_name + ".png";
				} else {
					texture_filename = base_name + "_" + String::num_int64(i) + ".png";
				}
				String texture_path = base_dir.path_join(texture_filename);

				// Save texture
				err = ImageSaver::save_image(texture_path, export_img, false, 1.0, false);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save texture " + texture_path);

				texture_filenames.push_back(texture_filename);
			}
		}
	}

	// Prepare binary data
	Ref<FileAccess> fa = FileAccess::open(p_dest_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(fa.is_null(), ERR_FILE_CANT_WRITE, "Failed to open file " + p_dest_path);

	// Write header: "BMF" + version 3
	static constexpr uint8_t bmf_header[] = { 'B', 'M', 'F', 3 };
	fa->store_buffer(bmf_header, 4);

	// Block 1: info
	{
		uint32_t block_size = 14 + font_name.utf8().length() + 1; // +1 for null terminator
		fa->store_8(1); // block type
		fa->store_32(block_size);

		int16_t fontSize = (int16_t)base_size;
		fa->store_16(fontSize);

		uint8_t bitField = 0;
		if (font_style.has_flag(TextServer::FONT_BOLD)) {
			bitField |= (1 << 3);
		}
		if (font_style.has_flag(TextServer::FONT_ITALIC)) {
			bitField |= (1 << 2);
		}
		bitField |= 0x02; // unicode
		fa->store_8(bitField);

		fa->store_8(0); // charSet (OEM charset, 0 for unicode)

		uint16_t stretchH = 100; // 100% = no stretch
		fa->store_16(stretchH);

		fa->store_8(1); // aa (supersampling level)

		fa->store_8(0); // paddingUp
		fa->store_8(0); // paddingRight
		fa->store_8(0); // paddingDown
		fa->store_8(0); // paddingLeft

		fa->store_8(0); // spacingHoriz
		fa->store_8(0); // spacingVert

		fa->store_8(outline_thickness); // outline

		// fontName (null-terminated)
		CharString font_name_utf8 = font_name.utf8();
		fa->store_buffer((const uint8_t *)font_name_utf8.get_data(), font_name_utf8.length());
		fa->store_8(0); // null terminator
	}

	// Block 2: common
	{
		uint32_t block_size = 15;
		fa->store_8(2); // block type
		fa->store_32(block_size);

		uint16_t lineHeight = (uint16_t)line_height;
		fa->store_16(lineHeight);

		uint16_t base = (uint16_t)ascent;
		fa->store_16(base);

		uint16_t scaleW = (uint16_t)texture_width;
		fa->store_16(scaleW);

		uint16_t scaleH = (uint16_t)texture_height;
		fa->store_16(scaleH);

		uint16_t pages = (uint16_t)actual_page_count;
		fa->store_16(pages);

		uint8_t bitField = is_packed ? 0x80 : 0; // bit 7 = packed
		fa->store_8(bitField);

		// Channel mappings based on detected format
		fa->store_8(ch[3]); // alphaChnl
		fa->store_8(ch[0]); // redChnl
		fa->store_8(ch[1]); // greenChnl
		fa->store_8(ch[2]); // blueChnl
	}

	// Block 3: pages
	{
		// Calculate block size: sum of all filename lengths + null terminators
		uint32_t block_size = 0;
		for (int i = 0; i < texture_filenames.size(); i++) {
			block_size += texture_filenames[i].utf8().length() + 1;
		}

		fa->store_8(3); // block type
		fa->store_32(block_size);

		// Write page names (null-terminated strings)
		for (int i = 0; i < texture_filenames.size(); i++) {
			CharString filename_utf8 = texture_filenames[i].utf8();
			fa->store_buffer((const uint8_t *)filename_utf8.get_data(), filename_utf8.length());
			fa->store_8(0); // null terminator
		}
	}

	// Block 4: chars
	{
		uint32_t block_size = glyph_list.size() * 20; // 20 bytes per character
		fa->store_8(4); // block type
		fa->store_32(block_size);

		// Write each character
		for (int i = 0; i < glyph_list.size(); i++) {
			int32_t glyph_index = glyph_list[i];
			Vector2 advance = r_fontfile->get_glyph_advance(cache_index, base_size, glyph_index);
			Vector2 offset = r_fontfile->get_glyph_offset(cache_index, size, glyph_index);
			Vector2 glyph_size = r_fontfile->get_glyph_size(cache_index, size, glyph_index);
			Rect2 uv_rect = r_fontfile->get_glyph_uv_rect(cache_index, size, glyph_index);
			int texture_idx = r_fontfile->get_glyph_texture_idx(cache_index, size, glyph_index);

			// Convert glyph_index to character (if possible)
			char32_t char_id = glyph_index;
			// Try to get character from glyph index
			char32_t char_from_glyph = r_fontfile->get_char_from_glyph_index(base_size, glyph_index);
			if (char_from_glyph != 0) {
				char_id = char_from_glyph;
			}

			// id (4 bytes)
			fa->store_32(char_id);

			// x (2 bytes)
			uint16_t x = (uint16_t)uv_rect.position.x;
			fa->store_16(x);

			// y (2 bytes)
			uint16_t y = (uint16_t)uv_rect.position.y;
			fa->store_16(y);

			// width (2 bytes)
			uint16_t width = (uint16_t)glyph_size.width;
			fa->store_16(width);

			// height (2 bytes)
			uint16_t height = (uint16_t)glyph_size.height;
			fa->store_16(height);

			// xoffset (2 bytes, signed)
			int16_t xoffset = (int16_t)offset.x;
			fa->store_16(xoffset);

			// yoffset (2 bytes, signed) - BMFont uses positive down, Godot uses positive up
			int16_t yoffset = (int16_t)(offset.y + ascent);
			fa->store_16(yoffset);

			// xadvance (2 bytes, signed)
			int16_t xadvance = (int16_t)advance.x;
			fa->store_16(xadvance);

			// page (1 byte) and chnl (1 byte)
			uint8_t page;
			uint8_t chnl;

			if (is_packed) {
				// Packed format: texture_idx = page * 4 + channel_index
				page = (uint8_t)(texture_idx / 4);
				int channel_index = texture_idx % 4;
				// BMFont channel mapping: 1=blue, 2=green, 4=red, 8=alpha
				switch (channel_index) {
					case 0:
						chnl = 4;
						break; // R
					case 1:
						chnl = 2;
						break; // G
					case 2:
						chnl = 1;
						break; // B
					case 3:
						chnl = 8;
						break; // A
					default:
						chnl = 4;
						break;
				}
			} else {
				// Non-packed format
				page = (uint8_t)texture_idx;

				if (is_monochrome) {
					// Monochrome format
					if (has_outline && first_gl_ch >= 0 && first_ol_ch >= 0) {
						// Monochrome with outline - channel depends on which texture
						// For now, assume all glyphs use the same channel (red = glyph)
						chnl = 4; // red channel = glyph
					} else {
						// Monochrome without outline
						if (first_gl_ch == 0) {
							chnl = 4; // red
						} else if (first_gl_ch == 1) {
							chnl = 2; // green
						} else if (first_gl_ch == 2) {
							chnl = 1; // blue
						} else if (first_gl_ch == 3) {
							chnl = 8; // alpha
						} else {
							chnl = 4; // default to red
						}
					}
				} else {
					// Color format (RGBA8) - all channels
					chnl = 15; // all channels
				}
			}

			fa->store_8(page);
			fa->store_8(chnl);
		}
	}

	// Block 5: kerning pairs (only if there are any)
	if (kerning_pairs.size() > 0) {
		uint32_t block_size = kerning_pairs.size() * 10; // 10 bytes per pair
		fa->store_8(5); // block type
		fa->store_32(block_size);

		// Write each kerning pair
		for (int i = 0; i < kerning_pairs.size(); i++) {
			Vector2i pair = kerning_pairs[i];
			Vector2 kerning = r_fontfile->get_kerning(cache_index, base_size, pair);

			// first (4 bytes)
			fa->store_32(pair.x);

			// second (4 bytes)
			fa->store_32(pair.y);

			// amount (2 bytes, signed)
			int16_t amount = (int16_t)kerning.x;
			fa->store_16(amount);
		}
	}

	// Write output to file
	fa->close();
	return OK;
}

Ref<ExportReport> FontFileExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	// Check if the exporter can handle the given importer and resource type
	String src_path = import_infos->get_path();
	String dst_path = output_dir.path_join(import_infos->get_export_dest().replace("res://", ""));
	Ref<ExportReport> report = memnew(ExportReport(import_infos, get_name()));
	report->set_resources_used({ import_infos->get_path() });
	if (import_infos->get_importer() == "font_data_dynamic") {
		Error err = _export_font_data_dynamic(dst_path, src_path);
		report->set_error(err);
		report->set_saved_path(dst_path);
		if (err == OK && import_infos->get_ver_major() >= 4) {
			Ref<FontFile> fontfile = ResourceCompatLoader::custom_load(src_path, "", ResourceInfo::LoadType::GLTF_LOAD, &err, false, ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP);
			if (!(err || fontfile.is_null())) {
				auto res_info = ResourceInfo::get_info_from_resource(fontfile);
				Dictionary params;
				// r_options->push_back(ImportOption(PropertyInfo(Variant::NIL, "Rendering", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));

				// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "antialiasing", PROPERTY_HINT_ENUM, "None,Grayscale,LCD Subpixel"), 1));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "generate_mipmaps"), false));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "disable_embedded_bitmaps"), true));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "multichannel_signed_distance_field", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), (msdf) ? true : false));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "msdf_pixel_range", PROPERTY_HINT_RANGE, "1,100,1"), 8));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "msdf_size", PROPERTY_HINT_RANGE, "1,250,1"), 48));

				// r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "allow_system_fallback"), true));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force_autohinter"), false));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "modulate_color_glyphs"), false));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "hinting", PROPERTY_HINT_ENUM, "None,Light,Normal"), 1));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "subpixel_positioning", PROPERTY_HINT_ENUM, "Disabled,Auto,One Half of a Pixel,One Quarter of a Pixel,Auto (Except Pixel Fonts)"), 4));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "keep_rounding_remainders"), true));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "oversampling", PROPERTY_HINT_RANGE, "0,10,0.1"), 0.0));

				// ConfigFile interprets setting a key to null as erasing the key, so we have to use a special value that'll get replaced when saving.
				params["Rendering"] = ImportInfo::NULL_REPLACEMENT;
				params["antialiasing"] = fontfile->get_antialiasing();
				params["generate_mipmaps"] = fontfile->get_generate_mipmaps();
				if (import_infos->get_ver_minor() >= 3) {
					params["disable_embedded_bitmaps"] = fontfile->get_disable_embedded_bitmaps();
				}
				params["multichannel_signed_distance_field"] = fontfile->is_multichannel_signed_distance_field();
				params["msdf_pixel_range"] = fontfile->get_msdf_pixel_range();
				params["msdf_size"] = fontfile->get_msdf_size();
				params["allow_system_fallback"] = fontfile->is_allow_system_fallback();
				params["force_autohinter"] = fontfile->is_force_autohinter();
				if (import_infos->get_ver_minor() >= 5) {
					params["modulate_color_glyphs"] = fontfile->is_modulate_color_glyphs();
				}
				params["hinting"] = fontfile->get_hinting();
				if (import_infos->get_ver_minor() >= 4) {
					params["subpixel_positioning"] = fontfile->get_subpixel_positioning();
					params["keep_rounding_remainders"] = fontfile->get_keep_rounding_remainders();
				}
				params["oversampling"] = fontfile->get_oversampling();

				// r_options->push_back(ImportOption(PropertyInfo(Variant::NIL, "Fallbacks", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::ARRAY, "fallbacks", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("Font")), Array()));
				params["Fallbacks"] = ImportInfo::NULL_REPLACEMENT;
				Array fallbacks;
				for (Ref<Font> fallback : fontfile->get_fallbacks()) {
					if (fallback.is_null()) {
						continue;
					}
					fallbacks.push_back(fallback);
				}
				params["fallbacks"] = fallbacks;

				// options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Compress", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
				// options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "compress", PROPERTY_HINT_NONE, ""), false));
				params["Compress"] = ImportInfo::NULL_REPLACEMENT;
				params["compress"] = res_info.is_valid() ? res_info->is_compressed : true;

				// // Hide from the main UI, only for advanced import dialog.
				// r_options->push_back(ImportOption(PropertyInfo(Variant::ARRAY, "preload", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), Array()));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::DICTIONARY, "language_support", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), Dictionary()));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::DICTIONARY, "script_support", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), Dictionary()));
				// r_options->push_back(ImportOption(PropertyInfo(Variant::DICTIONARY, "opentype_features", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), Dictionary()));

				// TODO? FontFile doesn't seem to store anything that would indicate that there were preload configurations set
				params["preload"] = Array();
				Dictionary language_support;
				for (auto &language : fontfile->get_language_support_overrides()) {
					language_support[language] = fontfile->get_language_support_override(language);
				}
				params["language_support"] = language_support;
				Dictionary script_support;
				for (auto &script : fontfile->get_script_support_overrides()) {
					script_support[script] = fontfile->get_script_support_override(script);
				}
				params["script_support"] = script_support;
				params["opentype_features"] = fontfile->get_opentype_features();
				import_infos->set_params(params);
			}
		}
	} else if (import_infos->get_importer() == "font_data_image") {
		Ref<Image> r_image;
		Error err = _export_image(dst_path, src_path, r_image);
		report->set_error(err);
		report->set_saved_path(dst_path);
		if (err == OK && import_infos->get_ver_major() >= 4) {
			_set_image_import_info(import_infos, r_image);
		}
	} else if (import_infos->get_importer() == "font_data_bmfont") {
		Ref<FontFile> fontfile;
		Error err = _export_bitmap_font(dst_path, src_path, fontfile);
		report->set_error(err);
		report->set_saved_path(dst_path);
		if (err == ERR_UNAVAILABLE) {
			// We don't currently support packed bmfont formats
			report->set_unsupported_format_type("Packed bmfont format");
		}
		if (import_infos->get_ver_major() >= 4) {
			Ref<ResourceInfo> res_info = ResourceInfo::get_info_from_resource(fontfile);
			Dictionary params;
			Array fallbacks;
			for (Ref<Font> fallback : fontfile->get_fallbacks()) {
				if (fallback.is_null()) {
					continue;
				}
				fallbacks.push_back(fallback);
			}
			params["fallbacks"] = fallbacks;

			params["compressed"] = res_info.is_valid() ? res_info->is_compressed : true;
			params["scaling_mode"] = (int)fontfile->get_fixed_size_scale_mode();
			import_infos->set_params(params);
		}
	}
	return report;
}

struct GlyphInfo {
	int glyph_index;
	Vector2 advance;
	Vector2 offset;
	Vector2 size;
	Rect2 uv_rect;
	int texture_idx;

	String to_string() const {
		int advance_value = advance.x - size.x;
		Vector2i offset_value = offset - Vector2(0, -(size.y / 2));
		if (offset_value == Vector2i() && advance_value == 0) {
			return String::num_int64(glyph_index);
		}
		return vformat("%d %d %d %d", glyph_index, advance_value, offset_value.x, offset_value.y);
	}
};

Vector<GlyphInfo> _get_character_ranges(Ref<Resource> fontfile) {
	// the reason we got it as a fake resource; we need to get the glyphs in the order that they are in the fontfile
	// 	[resource]
	// antialiasing = 0
	// subpixel_positioning = 0
	// allow_system_fallback = false
	// hinting = 0
	// oversampling = 1.0
	// fixed_size = 16
	// fixed_size_scale_mode = 2
	// cache/0/16/0/ascent = 8.0
	// cache/0/16/0/descent = 8.0
	// cache/0/16/0/underline_position = 0.0
	// cache/0/16/0/underline_thickness = 0.0
	// cache/0/16/0/scale = 1.0
	// cache/0/16/0/textures/0/offsets = PackedInt32Array()
	// cache/0/16/0/textures/0/image = SubResource("Image_uer0y")
	// cache/0/16/0/glyphs/65/advance = Vector2(10, 0)
	// cache/0/16/0/glyphs/65/offset = Vector2(0, -8)
	// cache/0/16/0/glyphs/65/size = Vector2(10, 16)
	// cache/0/16/0/glyphs/65/uv_rect = Rect2(0, 0, 10, 16)
	// cache/0/16/0/glyphs/65/texture_idx = 0
	// cache/0/16/0/glyphs/66/advance = Vector2(10, 0)
	// cache/0/16/0/glyphs/66/offset = Vector2(0, -8)
	// cache/0/16/0/glyphs/66/size = Vector2(10, 16)
	// cache/0/16/0/glyphs/66/uv_rect = Rect2(10, 0, 10, 16)
	// cache/0/16/0/glyphs/66/texture_idx = 0

	HashMap<int, GlyphInfo> glyph_map;
	Vector<int> glyph_list;

	List<PropertyInfo> property_list;
	fontfile->get_property_list(&property_list);
	for (auto &property : property_list) {
		if (property.name.begins_with("cache/") && property.name.contains("/glyphs/")) {
			PackedStringArray tokens = property.name.split("/");
			if (tokens.size() >= 6) {
				int glyph_index = tokens[5].to_int();
				if (!glyph_map.has(glyph_index)) {
					glyph_map[glyph_index] = GlyphInfo();
					glyph_map[glyph_index].glyph_index = glyph_index;
					glyph_list.push_back(glyph_index);
				}
				if (tokens[6] == "advance") {
					glyph_map[glyph_index].advance = fontfile->get(property.name);
				} else if (tokens[6] == "offset") {
					glyph_map[glyph_index].offset = fontfile->get(property.name);
				} else if (tokens[6] == "size") {
					glyph_map[glyph_index].size = fontfile->get(property.name);
				} else if (tokens[6] == "uv_rect") {
					glyph_map[glyph_index].uv_rect = fontfile->get(property.name);
				} else if (tokens[6] == "texture_idx") {
					glyph_map[glyph_index].texture_idx = fontfile->get(property.name);
				}
			}
		}
	}
	Vector<GlyphInfo> glyph_infos;
	// to ensure order is preserved
	for (auto &glyph : glyph_list) {
		glyph_infos.push_back(glyph_map[glyph]);
	}
	return glyph_infos;
}

void FontFileExporter::_set_image_import_info(Ref<ImportInfo> import_infos, Ref<Image> image) {
	// we have to set the parameters of the image import info

	Error err;
	Ref<Resource> fontfile = ResourceCompatLoader::fake_load(import_infos->get_path(), "", &err);
	ERR_FAIL_COND_MSG(err, "Failed to load font file " + import_infos->get_path());

	Vector<GlyphInfo> glyph_infos = _get_character_ranges(fontfile);
	ERR_FAIL_COND_MSG(glyph_infos.is_empty(), "No glyphs found in font file " + import_infos->get_path());
	PackedStringArray character_ranges;
	Vector<Vector2> sizes;

	HashSet<int> y_rect_set;
	Array fallbacks = fontfile->get("fallbacks");
	int x_rect_idx = 0;
	int max_x_rect_idx = 0;
	int chr_width = glyph_infos[0].size.x;
	int chr_height = glyph_infos[0].size.y;
	int chr_cell_width = 0;
	int chr_cell_height = 0;
	for (auto &glyph : glyph_infos) {
		// font->set_glyph_uv_rect(0, Vector2i(chr_height, 0), idx, Rect2(img_margin.position.x + chr_cell_width * x + char_margin.position.x, img_margin.position.y + chr_cell_height * y + char_margin.position.y, chr_width, chr_height));
		if (!y_rect_set.has((int)(glyph.uv_rect.position.y))) {
			if (y_rect_set.size() == 1 && chr_cell_height == 0) {
				chr_cell_height = glyph.uv_rect.position.y - glyph_infos[0].uv_rect.position.y;
			}
			y_rect_set.insert((int)(glyph.uv_rect.position.y));
			// new row
			max_x_rect_idx = MAX(max_x_rect_idx, x_rect_idx);
			x_rect_idx = 0;
		}
		if (x_rect_idx == 1 && chr_cell_width == 0) {
			chr_cell_width = glyph.uv_rect.position.x - glyph_infos[0].uv_rect.position.x;
		}
		character_ranges.push_back(glyph.to_string());
		sizes.push_back(glyph.size);
		x_rect_idx++;
	}
	int rows = y_rect_set.size();
	int columns = max_x_rect_idx;

	// font->set_glyph_uv_rect(0, Vector2i(chr_height, 0), idx, Rect2(img_margin.position.x + chr_cell_width * x + char_margin.position.x, img_margin.position.y + chr_cell_height * y + char_margin.position.y, chr_width, chr_height));
	int img_margin_pos_x_plus_char_margin_pos_x = glyph_infos[0].uv_rect.position.x; // x = 0
	int img_margin_pos_y_plus_char_margin_pos_y = glyph_infos[0].uv_rect.position.y; // y = 0
	// int chr_width = chr_cell_width - char_margin.position.x - char_margin.size.x;
	// int chr_height = chr_cell_height - char_margin.position.y - char_margin.size.y;
	int char_margin_pos_x_plus_size_x = -(chr_width - chr_cell_width);
	int char_margin_pos_y_plus_size_y = -(chr_height - chr_cell_height);
	// int chr_cell_width = (img->get_width() - img_margin.position.x - img_margin.size.x) / columns;
	// int chr_cell_height = (img->get_height() - img_margin.position.y - img_margin.size.y) / rows;
	int img_margin_pos_x_plus_size_x = -(columns * chr_cell_width - image->get_width());
	int img_margin_pos_y_plus_size_y = -(rows * chr_cell_height - image->get_height());
	int img_margin_size_x_plus_char_margin_size_x = (img_margin_pos_x_plus_size_x + char_margin_pos_x_plus_size_x) - img_margin_pos_x_plus_char_margin_pos_x;
	int img_margin_size_y_plus_char_margin_size_y = (img_margin_pos_y_plus_size_y + char_margin_pos_y_plus_size_y) - img_margin_pos_y_plus_char_margin_pos_y;

	int char_margin_size_x = 0; // TODO!
	int char_margin_size_y = 0; // TODO!
	int char_margin_pos_x = 0; // TODO!
	int char_margin_pos_y = 0; // TODO!
	int img_margin_pos_x = 0; // TODO!
	int img_margin_pos_y = 0; // TODO!
	int img_margin_size_x = 0; // TODO!
	int img_margin_size_y = 0; // TODO!

	if (img_margin_pos_x_plus_size_x != 0 || char_margin_pos_x_plus_size_x != 0) {
		for (int i = 0; i < chr_cell_width; i++) {
			for (int j = 0; j < chr_cell_width; j++) {
				char_margin_size_x = i;
				char_margin_pos_x = j;

				if (char_margin_size_x + char_margin_pos_x != char_margin_pos_x_plus_size_x) {
					continue;
				}

				img_margin_pos_x = img_margin_pos_x_plus_char_margin_pos_x - char_margin_pos_x;
				img_margin_size_x = img_margin_size_x_plus_char_margin_size_x - char_margin_size_x;

				if (img_margin_pos_x + img_margin_size_x == img_margin_pos_x_plus_size_x) {
					break;
				}
			}
		}
		// do the same for the y axis
		for (int i = 0; i < chr_cell_height; i++) {
			for (int j = 0; j < chr_cell_height; j++) {
				char_margin_size_y = i;
				char_margin_pos_y = j;

				if (char_margin_size_y + char_margin_pos_y != char_margin_pos_y_plus_size_y) {
					continue;
				}

				img_margin_pos_y = img_margin_pos_y_plus_char_margin_pos_y - char_margin_pos_y;
				img_margin_size_y = img_margin_size_y_plus_char_margin_size_y - char_margin_size_y;

				if (img_margin_pos_y + img_margin_size_y == img_margin_pos_y_plus_size_y) {
					break;
				}
			}
		}
	}

	List<PropertyInfo> property_list;
	fontfile->get_property_list(&property_list);

	int ascent = 0;
	int descent = 0;
	// float scale = 0;
	int fixed_size_scale_mode = fontfile->get("fixed_size_scale_mode");
	for (auto &property : property_list) {
		if (property.name.ends_with("/ascent")) {
			ascent = fontfile->get(property.name);
		} else if (property.name.ends_with("/descent")) {
			descent = fontfile->get(property.name);
			// } else if (property.name.ends_with("/scale")) {
			// 	scale = fontfile->get(property.name);
		}
	}

	// r_options->push_back(ImportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "character_ranges"), Vector<String>()));
	// r_options->push_back(ImportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "kerning_pairs"), Vector<String>()));
	// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "columns", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), 1));
	// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "rows", PROPERTY_HINT_RANGE, "1,1024,1,or_greater"), 1));
	// r_options->push_back(ImportOption(PropertyInfo(Variant::RECT2I, "image_margin"), Rect2i()));
	// r_options->push_back(ImportOption(PropertyInfo(Variant::RECT2I, "character_margin"), Rect2i()));
	// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "ascent"), 0));
	// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "descent"), 0));

	// r_options->push_back(ImportOption(PropertyInfo(Variant::ARRAY, "fallbacks", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("Font")), Array()));

	// r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "compress"), true));
	// r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "scaling_mode", PROPERTY_HINT_ENUM, "Disabled,Enabled (Integer),Enabled (Fractional)"), TextServer::FIXED_SIZE_SCALE_ENABLED));

	Dictionary params;
	params["character_ranges"] = character_ranges;
	params["kerning_pairs"] = PackedStringArray(); // TODO
	params["columns"] = columns;
	params["rows"] = rows;
	params["image_margin"] = Rect2i(img_margin_pos_x, img_margin_pos_y, img_margin_size_x, img_margin_size_y);
	params["character_margin"] = Rect2i(char_margin_pos_x, char_margin_pos_y, char_margin_size_x, char_margin_size_y);
	params["ascent"] = ascent;
	params["descent"] = descent;
	params["fallbacks"] = fallbacks;
	params["compress"] = true;
	params["scaling_mode"] = fixed_size_scale_mode;
	import_infos->set_params(params);
}

void FontFileExporter::get_handled_types(List<String> *out) const {
	out->push_back("FontFile");
}

void FontFileExporter::get_handled_importers(List<String> *out) const {
	out->push_back("font_data_dynamic");
	out->push_back("font_data_image");
	out->push_back("font_data_bmfont");
}

String FontFileExporter::get_name() const {
	return EXPORTER_NAME;
}

String FontFileExporter::get_default_export_extension(const String &res_path) const {
	return "ttf";
}

Vector<String> FontFileExporter::get_export_extensions(const String &res_path) const {
	Vector<String> extensions = gdre::hashset_to_vector(dynamic_font_supported_extensions);
	extensions.append_array(gdre::hashset_to_vector(bitmap_font_supported_extensions));
	extensions.append_array(ImageSaver::get_supported_extensions());
	return extensions;
}
