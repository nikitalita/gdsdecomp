
#include "image_parser_v2.h"
#include "core/io/image.h"

void _advance_padding(FileAccess *f, uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); // pad to 32
		}
	}
}

V2Image::Format ImageParserV2::get_format_from_string(const String &fmt_id) {
	if (fmt_id == "CUSTOM") {
		return V2Image::IMAGE_FORMAT_CUSTOM;
	}
	for (int i = 0; i < V2Image::IMAGE_FORMAT_V2_MAX; i++) {
		if (V2Image::format_names[i] == fmt_id) {
			return (V2Image::Format)i;
		}
	}
	return V2Image::IMAGE_FORMAT_V2_MAX;
}
String ImageParserV2::get_format_name(V2Image::Format p_format) {
	if (p_format == V2Image::IMAGE_FORMAT_CUSTOM) {
		return "Custom";
	}
	ERR_FAIL_INDEX_V(p_format, V2Image::IMAGE_FORMAT_V2_MAX, String());
	return V2Image::format_names[p_format];
}

String ImageParserV2::get_format_identifier(V2Image::Format p_format) {
	if (p_format == V2Image::IMAGE_FORMAT_CUSTOM) {
		return "CUSTOM";
	}
	ERR_FAIL_INDEX_V(p_format, V2Image::IMAGE_FORMAT_V2_MAX, String());
	return V2Image::format_identifiers[p_format];
}

Ref<Image> ImageParserV2::convert_indexed_image(const Vector<uint8_t> &p_imgdata, int p_width, int p_height, int p_mipmaps, V2Image::Format p_format, Error *r_error) {
	Vector<uint8_t> r_imgdata;
	Image::Format r_format;
	int datalen = p_imgdata.size();
	if ((p_format != 5 && p_format != 6 && p_format != 1)) {
		if (r_error) {
			*r_error = ERR_INVALID_PARAMETER;
		}
		return Ref<Image>();
	}
	if (p_format == V2Image::IMAGE_FORMAT_INTENSITY) {
		r_format = Image::FORMAT_RGBA8;
		r_imgdata.resize(datalen * 4);
		for (uint32_t i = 0; i < datalen; i++) {
			r_imgdata.write[i * 4] = 255;
			r_imgdata.write[i * 4 + 1] = 255;
			r_imgdata.write[i * 4 + 2] = 255;
			r_imgdata.write[i * 4 + 3] = p_imgdata[i];
		}
	} else {
		int pal_width;
		if (p_format == V2Image::IMAGE_FORMAT_INDEXED) {
			r_format = Image::FORMAT_RGB8;
			pal_width = 3;
		} else { // V2Image::IMAGE_FORMAT_INDEXED_ALPHA
			r_format = Image::FORMAT_RGBA8;
			pal_width = 4;
		}

		Vector<Vector<uint8_t>> palette;

		// palette data starts at end of pixel data, is equal to 256 * pal_width
		for (int dataidx = p_width * p_height; dataidx < p_imgdata.size(); dataidx += pal_width) {
			palette.push_back(p_imgdata.slice(dataidx, dataidx + pal_width));
		}
		// pixel data is index into palette
		for (uint32_t i = 0; i < p_width * p_height; i++) {
			r_imgdata.append_array(palette[p_imgdata[i]]);
		}
	}
	Ref<Image> img;
	img.instantiate();
	img->create(p_width, p_height, p_mipmaps > 0, r_format, r_imgdata);
	if (img.is_null() && r_error) {
		*r_error = ERR_PARSE_ERROR;
	}
	return img;
}

V2Image::Format ImageParserV2::new_format_to_v2_format(Image::Format p_format, bool hacks_for_dropped_fmt = true) {
	switch (p_format) {
		// convert old image format types to new ones
		case Image::FORMAT_L8:
			return V2Image::IMAGE_FORMAT_GRAYSCALE;
		case Image::FORMAT_LA8:
			return V2Image::IMAGE_FORMAT_GRAYSCALE_ALPHA;
		case Image::FORMAT_RGB8:
			return V2Image::IMAGE_FORMAT_RGB;
		case Image::FORMAT_RGBA8:
			return V2Image::IMAGE_FORMAT_RGBA;
		case Image::FORMAT_DXT1:
			return V2Image::IMAGE_FORMAT_BC1;
		case Image::FORMAT_DXT3:
			return V2Image::IMAGE_FORMAT_BC2;
		case Image::FORMAT_DXT5:
			return V2Image::IMAGE_FORMAT_BC3;
		case Image::FORMAT_RGTC_R:
			return V2Image::IMAGE_FORMAT_BC4;
		case Image::FORMAT_RGTC_RG:
			return V2Image::IMAGE_FORMAT_BC5;
		case Image::FORMAT_PVRTC1_2:
			return V2Image::IMAGE_FORMAT_PVRTC2;
		case Image::FORMAT_PVRTC1_2A:
			return V2Image::IMAGE_FORMAT_PVRTC2_ALPHA;
		case Image::FORMAT_PVRTC1_4:
			return V2Image::IMAGE_FORMAT_PVRTC4;
		case Image::FORMAT_PVRTC1_4A:
			return V2Image::IMAGE_FORMAT_PVRTC4_ALPHA;
		case Image::FORMAT_ETC:
			return V2Image::IMAGE_FORMAT_ETC;
		default: {
			if (hacks_for_dropped_fmt) {
				switch (p_format) {
					// Hacks for no-longer supported image formats
					// the v4 formats in "get_image_required_mipmaps" map to the required mipmaps for the deprecated formats
					case Image::FORMAT_ETC2_R11:
						return V2Image::IMAGE_FORMAT_INTENSITY;
					case Image::FORMAT_ETC2_R11S:
						return V2Image::IMAGE_FORMAT_INDEXED;
					case Image::FORMAT_ETC2_RG11:
						return V2Image::IMAGE_FORMAT_INDEXED_ALPHA;
					case Image::FORMAT_ETC2_RG11S:
						return V2Image::IMAGE_FORMAT_ATC;
					case Image::FORMAT_ETC2_RGB8:
						return V2Image::IMAGE_FORMAT_ATC_ALPHA_EXPLICIT;
					case Image::FORMAT_ETC2_RGB8A1:
						return V2Image::IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED;
					case Image::FORMAT_ETC2_RA_AS_RG:
						return V2Image::IMAGE_FORMAT_CUSTOM;
				}
			}
		}
	}
	return V2Image::IMAGE_FORMAT_V2_MAX;
}

Image::Format ImageParserV2::v2_format_to_new_format(V2Image::Format p_format, bool hacks_for_dropped_fmt = true) {
	switch (p_format) {
		// convert old image format types to new ones
		case V2Image::IMAGE_FORMAT_GRAYSCALE:
			return Image::FORMAT_L8;
		case V2Image::IMAGE_FORMAT_GRAYSCALE_ALPHA:
			return Image::FORMAT_LA8;
		case V2Image::IMAGE_FORMAT_RGB:
			return Image::FORMAT_RGB8;
		case V2Image::IMAGE_FORMAT_RGBA:
			return Image::FORMAT_RGBA8;
		case V2Image::IMAGE_FORMAT_BC1:
			return Image::FORMAT_DXT1;
		case V2Image::IMAGE_FORMAT_BC2:
			return Image::FORMAT_DXT3;
		case V2Image::IMAGE_FORMAT_BC3:
			return Image::FORMAT_DXT5;
		case V2Image::IMAGE_FORMAT_BC4:
			return Image::FORMAT_RGTC_R;
		case V2Image::IMAGE_FORMAT_BC5:
			return Image::FORMAT_RGTC_RG;
		case V2Image::IMAGE_FORMAT_PVRTC2:
			return Image::FORMAT_PVRTC1_2;
		case V2Image::IMAGE_FORMAT_PVRTC2_ALPHA:
			return Image::FORMAT_PVRTC1_2A;
		case V2Image::IMAGE_FORMAT_PVRTC4:
			return Image::FORMAT_PVRTC1_4;
		case V2Image::IMAGE_FORMAT_PVRTC4_ALPHA:
			return Image::FORMAT_PVRTC1_4A;
		case V2Image::IMAGE_FORMAT_ETC:
			return Image::FORMAT_ETC;
		// If this is a deprecated or unsupported format...
		default: {
			// Hacks for no-longer supported image formats
			// We change the format to something that V2 didn't have support for as a placeholder
			// This is just in the case of converting a bin resource to a text resource
			// It gets handled in the above converters
			if (hacks_for_dropped_fmt) {
				switch (p_format) {
					case V2Image::IMAGE_FORMAT_INTENSITY:
						return Image::FORMAT_ETC2_R11;
					case V2Image::IMAGE_FORMAT_INDEXED:
						return Image::FORMAT_ETC2_R11S;
					case V2Image::IMAGE_FORMAT_INDEXED_ALPHA:
						return Image::FORMAT_ETC2_RG11;
					case V2Image::IMAGE_FORMAT_ATC:
						return Image::FORMAT_ETC2_RG11S;
					case V2Image::IMAGE_FORMAT_ATC_ALPHA_EXPLICIT:
						return Image::FORMAT_ETC2_RGB8;
					case V2Image::IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED:
						return Image::FORMAT_ETC2_RGB8A1;
					case V2Image::IMAGE_FORMAT_CUSTOM:
						return Image::FORMAT_ETC2_RA_AS_RG;
						// We can't convert YUV format
				}
			}
		}
	}
	return Image::FORMAT_MAX;
}

String ImageParserV2::image_v2_to_string(const Variant &r_v) {
	Ref<Image> img = r_v;

	if (img.is_null() || img->is_empty()) {
		return String("Image()");
	}

	String imgstr = "Image( ";
	imgstr += itos(img->get_width());
	imgstr += ", " + itos(img->get_height());
	String subimgstr = ", " + itos(img->get_mipmap_count()) + ", ";
	Image::Format fmt = img->get_format();
	String fmt_id = get_format_identifier(new_format_to_v2_format(fmt, true));
	// special case for hacked image formats
	switch (fmt) {
		case Image::FORMAT_ETC2_R11:
			// reset substr
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_L8)) + ", ";
			break;
		case Image::FORMAT_ETC2_R11S:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_L8)) + ", ";
			break;
		case Image::FORMAT_ETC2_RG11:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_L8)) + ", ";
			break;
		case Image::FORMAT_ETC2_RG11S:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_PVRTC1_4A)) + ", ";
			break;
		case Image::FORMAT_ETC2_RGB8:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_BPTC_RGBA)) + ", ";
			break;
		case Image::FORMAT_ETC2_RGB8A1:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_BPTC_RGBA)) + ", ";
			break;
		case Image::FORMAT_ETC2_RA_AS_RG:
			subimgstr = ", " + itos(1) + ", ";
			break;
		default: {
		}
	}
	imgstr += subimgstr + fmt_id;

	String s;

	Vector<uint8_t> data = img->get_data();
	int len = data.size();
	for (int i = 0; i < len; i++) {
		if (i > 0)
			s += ", ";
		s += itos(data[i]);
	}

	imgstr += ", " + s + " )";
	return imgstr;
}

Error ImageParserV2::write_image_v2_to_bin(FileAccess *f, const Variant &r_v, const PropertyHint p_hint) {
	Ref<Image> val = r_v;
	if (val.is_null() || val->is_empty()) {
		f->store_32(V2Image::IMAGE_ENCODING_EMPTY);
		return OK;
	}

	int encoding = V2Image::IMAGE_ENCODING_RAW;
	float quality = 0.7;

	if (val->get_format() <= Image::FORMAT_RGB565) {
		// can only compress uncompressed stuff
		if (p_hint == PROPERTY_HINT_IMAGE_COMPRESS_LOSSLESS && Image::png_packer) {
			encoding = V2Image::IMAGE_ENCODING_LOSSLESS;
		}
	}

	f->store_32(encoding); // raw encoding

	if (encoding == V2Image::IMAGE_ENCODING_RAW) {
		f->store_32(val->get_width());
		f->store_32(val->get_height());
		int mipmaps = val->get_mipmap_count();

		V2Image::Format fmt = new_format_to_v2_format(val->get_format(), true);
		switch (val->get_format()) {
			case Image::FORMAT_ETC2_R11: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_L8);
			} break;
			case Image::FORMAT_ETC2_R11S: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_L8);
			} break;
			case Image::FORMAT_ETC2_RG11: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_L8);
			} break;
			case Image::FORMAT_ETC2_RG11S: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_PVRTC1_4A);
			} break;
			case Image::FORMAT_ETC2_RGB8: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_BPTC_RGBA);
			} break;
			case Image::FORMAT_ETC2_RGB8A1: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_BPTC_RGBA);
			} break;
			case Image::FORMAT_ETC2_RA_AS_RG: {
				mipmaps = 0;
			} break;

			default: {
				ERR_FAIL_V(ERR_FILE_CORRUPT);
			}
		}
		f->store_32(mipmaps);
		f->store_32(fmt);
		int dlen = val->get_data().size();
		f->store_32(dlen);
		f->store_buffer(val->get_data().ptr(), dlen);
		_advance_padding(f, dlen);
	} else {
		Vector<uint8_t> data;
		if (encoding == V2Image::IMAGE_ENCODING_LOSSY) {
			data = Image::webp_lossy_packer(val, quality);
		} else if (encoding == V2Image::IMAGE_ENCODING_LOSSLESS) {
			data = Image::png_packer(val);
		}

		int ds = data.size();
		f->store_32(ds);
		if (ds > 0) {
			f->store_buffer(data.ptr(), ds);
			_advance_padding(f, ds);
		}
	}
	return OK;
}

Error ImageParserV2::decode_image_v2(FileAccess *f, Variant &r_v, bool hacks_for_dropped_fmt, bool convert_indexed) {
	uint32_t encoding = f->get_32();
	Ref<Image> img;

	if (encoding == V2Image::IMAGE_ENCODING_EMPTY) {
		img.instantiate();
		r_v = img;
		return OK;
	} else if (encoding == V2Image::IMAGE_ENCODING_RAW) {
		uint32_t width = f->get_32();
		uint32_t height = f->get_32();
		uint32_t mipmaps = f->get_32();
		uint32_t old_format = f->get_32();
		Image::Format fmt = v2_format_to_new_format((V2Image::Format)old_format, hacks_for_dropped_fmt);

		uint32_t datalen = f->get_32();

		Vector<uint8_t> imgdata;
		imgdata.resize(datalen);
		uint8_t *w = imgdata.ptrw();
		f->get_buffer(w, datalen);
		_advance_padding(f, datalen);
		// This is for if we're saving the image as a png.
		if (convert_indexed && (old_format == 5 || old_format == 6 || old_format == 1)) {
			Error err;
			img = ImageParserV2::convert_indexed_image(imgdata, width, height, mipmaps, (V2Image::Format)old_format, &err);
			ERR_FAIL_COND_V_MSG(err, err,
					"Can't convert deprecated image format " + get_format_name((V2Image::Format)old_format) + " to new image formats!");
		} else {
			// We wait until we've skipped all the data to do this
			ERR_FAIL_COND_V_MSG(
					fmt == Image::FORMAT_MAX,
					ERR_UNAVAILABLE,
					"Converting deprecated image format " + get_format_name((V2Image::Format)old_format) + " not implemented.");
			img.instantiate();
			img->create(width, height, mipmaps > 0, fmt, imgdata);
		}
	} else {
		// compressed
		Vector<uint8_t> data;
		data.resize(f->get_32());
		uint8_t *w = data.ptrw();
		f->get_buffer(w, data.size());

		if (encoding == V2Image::IMAGE_ENCODING_LOSSY && Image::webp_unpacker) {
			img = img->webp_unpacker(data);
		} else if (encoding == V2Image::IMAGE_ENCODING_LOSSLESS && Image::png_unpacker) {
			img = img->png_unpacker(data);
		}
		_advance_padding(f, data.size());
	}
	r_v = img;
	return OK;
}

#define ERR_PARSE_V2IMAGE_FAIL(c_type, error_string) \
	if (token.type != c_type) {                      \
		r_err_str = error_string;                    \
		return ERR_PARSE_ERROR;                      \
	}

#define EXPECT_COMMA()                                          \
	VariantParser::get_token(p_stream, token, line, r_err_str); \
	ERR_PARSE_V2IMAGE_FAIL(VariantParser::TK_COMMA, "Expected comma in Image variant")

Error ImageParserV2::parse_image_construct_v2(VariantParser::Stream *p_stream, Variant &r_v, bool hacks_for_dropped_fmt, bool convert_indexed, int &line, String &p_err_str) {
	String r_err_str;
	VariantParser::Token token;
	uint32_t width;
	uint32_t height;
	uint32_t mipmaps;
	V2Image::Format old_format;
	Image::Format fmt = Image::FORMAT_MAX;
	Vector<uint8_t> data;
	Ref<Image> img;

	// The "Image" identifier has already been parsed, start with parantheses
	VariantParser::get_token(p_stream, token, line, r_err_str);
	ERR_PARSE_V2IMAGE_FAIL(VariantParser::TK_PARENTHESIS_OPEN, "Expected '(' in constructor");

	// w, h, mipmap count, format string, data...
	// width
	VariantParser::get_token(p_stream, token, line, r_err_str);
	ERR_PARSE_V2IMAGE_FAIL(VariantParser::TK_NUMBER, "Expected width in Image variant");
	width = token.value;
	EXPECT_COMMA();

	// height
	VariantParser::get_token(p_stream, token, line, r_err_str);
	ERR_PARSE_V2IMAGE_FAIL(VariantParser::TK_NUMBER, "Expected height in Image variant");
	height = token.value;
	EXPECT_COMMA();

	// mipmaps
	VariantParser::get_token(p_stream, token, line, r_err_str);
	ERR_PARSE_V2IMAGE_FAIL(VariantParser::TK_NUMBER, "Expected mipmap count in Image variant");
	mipmaps = token.value;
	EXPECT_COMMA();

	// format string
	VariantParser::get_token(p_stream, token, line, r_err_str);
	ERR_PARSE_V2IMAGE_FAIL(VariantParser::TK_STRING, "Expected format string in Image variant");
	old_format = get_format_from_string(token.value);
	fmt = v2_format_to_new_format(old_format, hacks_for_dropped_fmt);
	bool first = true;
	EXPECT_COMMA();

	// data
	while (true) {
		if (!first) {
			VariantParser::get_token(p_stream, token, line, r_err_str);
			if (token.type == VariantParser::TK_PARENTHESIS_CLOSE) {
				break;
			} else {
				ERR_PARSE_V2IMAGE_FAIL(VariantParser::TK_COMMA, "Expected ',' or ')'");
			}
		}
		VariantParser::get_token(p_stream, token, line, r_err_str);
		if (first && token.type == VariantParser::TK_PARENTHESIS_CLOSE) {
			break;
		}
		ERR_PARSE_V2IMAGE_FAIL(VariantParser::TK_NUMBER, "Expected int in image data");

		data.push_back(token.value);
		first = false;
	}
	if (fmt == Image::FORMAT_MAX) {
		r_err_str = "Converting deprecated image format " + get_format_name((V2Image::Format)old_format) + " not implemented.";
		return ERR_PARSE_ERROR;
	}
	if (convert_indexed && (old_format == 5 || old_format == 6 || old_format == 1)) {
		Error err;
		img = ImageParserV2::convert_indexed_image(data, width, height, mipmaps, (V2Image::Format)old_format, &err);
		if (img->is_empty()) {
			r_err_str = "Failed to convert deprecated image format " + get_format_name((V2Image::Format)old_format) + " to new image format!";
			return err;
		}
	} else {
		img.instantiate();
		img->create(width, height, mipmaps > 0, fmt, data);
	}
	if (img->is_empty()) {
		r_err_str = "Failed to create image";
		return ERR_PARSE_ERROR;
	}

	r_v = img;
	return OK;
}