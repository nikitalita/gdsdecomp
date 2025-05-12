#include "texture_exporter.h"

#include "compat/resource_compat_binary.h"
#include "compat/resource_loader_compat.h"
#include "utility/common.h"

#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "core/io/image_loader.h"
#include "core/io/resource_loader.h"
#include "scene/resources/atlas_texture.h"
#include "utility/resource_info.h"

namespace {
bool get_bit(const Vector<uint8_t> &bitmask, int width, int p_x, int p_y) {
	int ofs = width * p_y + p_x;
	int bbyte = ofs / 8;
	int bbit = ofs % 8;

	return (bitmask[bbyte] & (1 << bbit)) != 0;
}
} //namespace

// Format is the same on V2 - V4
Ref<Image> TextureExporter::load_image_from_bitmap(const String p_path, Error *r_err) {
	Error err;
	Ref<Image> image;
	image.instantiate();
	ResourceFormatLoaderCompatBinary rlcb;
	auto res = ResourceCompatLoader::fake_load(p_path, "", &err);
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Image>(), "Cannot open resource '" + p_path + "'.");

	String name;
	Vector2 size;
	Dictionary data;
	PackedByteArray bitmask;
	int width;
	int height;

	// Load the main resource, which should be the ImageTexture
	name = ResourceCompatConverter::get_resource_name(res, 0);
	data = res->get("data");
	bitmask = data.get("data", PackedByteArray());
	size = data.get("size", Vector2());
	width = size.width;
	height = size.height;
	image->initialize_data(width, height, false, Image::FORMAT_L8);

	if (!name.is_empty()) {
		image->set_name(name);
	}
	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			image->set_pixel(i, j, get_bit(bitmask, width, i, j) ? Color(1, 1, 1) : Color(0, 0, 0));
		}
	}
	ERR_FAIL_COND_V_MSG(image.is_null() || image->is_empty(), Ref<Image>(), "Failed to load image from " + p_path);
	return image;
}

Error TextureExporter::_convert_bitmap(const String &p_path, const String &dest_path, bool lossy = true) {
	String dst_dir = dest_path.get_base_dir();
	Error err;
	Ref<Image> img = load_image_from_bitmap(p_path, &err);
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		// TODO: Not reporting here because we can't get the deprecated format type yet,
		// implement functionality to pass it back
		print_line("Did not convert deprecated Bitmap resource " + p_path);
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to load bitmap " + p_path);
	err = gdre::ensure_dir(dst_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dest_path);
	err = save_image(dest_path, img, lossy);
	if (err == ERR_UNAVAILABLE) {
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	print_verbose("Converted " + p_path + " to " + dest_path);
	return OK;
}

Error TextureExporter::save_image(const String &dest_path, const Ref<Image> &img, bool lossy) {
	ERR_FAIL_COND_V_MSG(img->is_empty(), ERR_FILE_EOF, "Image data is empty for texture " + dest_path + ", not saving");
	Error err = gdre::ensure_dir(dest_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dest_path);
	String dest_ext = dest_path.get_extension().to_lower();
	if (img->is_compressed() && img->has_mipmaps()) {
		img->clear_mipmaps();
	}
	GDRE_ERR_DECOMPRESS_OR_FAIL(img);
	if (dest_ext == "jpg" || dest_ext == "jpeg") {
		err = gdre::save_image_as_jpeg(dest_path, img);
	} else if (dest_ext == "webp") {
		err = gdre::save_image_as_webp(dest_path, img, lossy);
	} else if (dest_ext == "png") {
		err = img->save_png(dest_path);
	} else if (dest_ext == "tga") {
		err = gdre::save_image_as_tga(dest_path, img);
	} else if (dest_ext == "svg") {
		err = gdre::save_image_as_svg(dest_path, img);
	} else if (dest_ext == "dds") {
		err = img->save_dds(dest_path);
	} else if (dest_ext == "exr") {
		err = img->save_exr(dest_path);
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_BAD_PATH, "Invalid file name: " + dest_path);
	}
	return err;
}

Error TextureExporter::_convert_tex(const String &p_path, const String &dest_path, bool lossy, String &image_format) {
	Error err;
	String dst_dir = dest_path.get_base_dir();
	Ref<Texture2D> tex;
	tex = ResourceCompatLoader::non_global_load(p_path, "", &err);
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		// TODO: Not reporting here because we can't get the deprecated format type yet,
		// implement functionality to pass it back
		image_format = "Unknown deprecated image format";
		print_line("Did not convert deprecated Texture resource " + p_path);
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK || tex.is_null(), err, "Failed to load texture " + p_path);

	Ref<Image> img = tex->get_image();

	ERR_FAIL_COND_V_MSG(img.is_null(), ERR_PARSE_ERROR, "Failed to load image for texture " + p_path);
	image_format = Image::get_format_name(img->get_format());
	err = save_image(dest_path, img, lossy);
	if (err == ERR_UNAVAILABLE) {
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	print_verbose("Converted " + p_path + " to " + dest_path);
	return OK;
}

Error TextureExporter::_convert_atex(const String &p_path, const String &dest_path, bool lossy, String &image_format) {
	Error err;
	String dst_dir = dest_path.get_base_dir();
	Ref<AtlasTexture> atex = ResourceCompatLoader::custom_load(p_path, "", ResourceInfo::GLTF_LOAD, &err, false, ResourceFormatLoader::CACHE_MODE_IGNORE);
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		// TODO: Not reporting here because we can't get the deprecated format type yet,
		// implement functionality to pass it back
		image_format = "Unknown deprecated image format";
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK || atex.is_null(), err, "Failed to load texture " + p_path);
	Ref<Texture2D> tex = atex->get_atlas();
	ERR_FAIL_COND_V_MSG(tex.is_null(), ERR_PARSE_ERROR, "Failed to load atlas texture " + p_path);
	Ref<Image> img = tex->get_image();

	ERR_FAIL_COND_V_MSG(img.is_null(), ERR_PARSE_ERROR, "Failed to load image for texture " + p_path);
	ERR_FAIL_COND_V_MSG(img->is_empty(), ERR_FILE_EOF, "Image data is empty for texture " + p_path + ", not saving");
	image_format = Image::get_format_name(img->get_format());

	img = img->duplicate();
	if (img->is_compressed() && img->has_mipmaps()) {
		img->clear_mipmaps();
	}

	// resize it according to the properties of the atlas
	GDRE_ERR_DECOMPRESS_OR_FAIL(img);
	if (img->get_format() != Image::FORMAT_RGBA8) {
		img->convert(Image::FORMAT_RGBA8);
	}

	auto margin = atex->get_margin();
	auto region = atex->get_region();

	// now we have to add the margin padding
	Ref<Image> new_img = Image::create_empty(atex->get_width(), atex->get_height(), false, img->get_format());
	new_img->blit_rect(img, region, Point2i(margin.position.x, margin.position.y));
	err = save_image(dest_path, new_img, lossy);
	if (err == ERR_UNAVAILABLE) {
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	print_verbose("Converted " + p_path + " to " + dest_path);
	return OK;
}

Error TextureExporter::export_file(const String &out_path, const String &res_path) {
	Error err;
	auto res_info = ResourceCompatLoader::get_resource_info(res_path, "", &err);
	if (!handles_import("", res_info.type)) {
		return ERR_FILE_UNRECOGNIZED;
	}
	if (res_info.type == "BitMap") {
		return _convert_bitmap(res_path, out_path);
	}
	String fmt_name;
	if (res_info.type == "AtlasTexture") {
		return _convert_atex(res_path, out_path, false, fmt_name);
	}
	return _convert_tex(res_path, out_path, false, fmt_name);
}

Error TextureExporter::_convert_3d(const String &p_path, const String &dest_path, bool lossy, String &image_format, Ref<ExportReport> report) {
	Error err;
	String dst_dir = dest_path.get_base_dir();
	Ref<Texture3D> tex;
	tex = ResourceCompatLoader::non_global_load(p_path, "", &err);
	Ref<ImportInfo> iinfo;
	if (report.is_valid()) {
		iinfo = report->get_import_info();
	}

	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		image_format = "Unknown deprecated image format";
		print_line("Did not convert deprecated Texture resource " + p_path);
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK || tex.is_null(), err, "Failed to load texture " + p_path);

	auto layer_count = tex->get_depth();
	int width = tex->get_width();
	int height = tex->get_height();
	Vector<Ref<Image>> images = tex->get_data();
	Vector<Vector<uint8_t>> images_data;
	for (int64_t i = 0; i < layer_count; i++) {
		Ref<Image> img = images[i];
		ERR_FAIL_COND_V_MSG(img.is_null(), ERR_PARSE_ERROR, "Failed to load image index " + itos(i) + " for texture " + p_path);
		if (img->is_compressed() && img->has_mipmaps()) {
			img->clear_mipmaps();
		}
		GDRE_ERR_DECOMPRESS_OR_FAIL(img);
		if (img->get_format() != Image::FORMAT_RGBA8) {
			img->convert(Image::FORMAT_RGBA8);
		}
		images.push_back(img);
		images_data.push_back(img->get_data());
	}
	int64_t num_images_w = -1;
	int64_t num_images_h = -1;
	size_t new_width;
	size_t new_height;
	Dictionary params;
	if (iinfo.is_valid()) {
		params = iinfo->get_params();
	}
	bool had_valid_params = false;
	if (iinfo.is_valid()) {
		num_images_w = params.get("slices/horizontal", -1);
		num_images_h = params.get("slices/vertical", -1);
		had_valid_params = num_images_w != -1 && num_images_h != -1;
	}
	if (!had_valid_params) {
		if (layer_count == 64) {
			num_images_w = 8;
			num_images_h = 8;
		} else if (layer_count == 16) {
			num_images_w = 4;
			num_images_h = 4;
		} else if (layer_count == 4) {
			num_images_w = 2;
			num_images_h = 2;
		} else {
			num_images_w = layer_count;
			num_images_h = 1;
		}
	}

	new_width = width * num_images_w;
	new_height = height * num_images_h;

	Vector<uint8_t> new_image_data;

	for (int i = 0; i < new_height; i++) {
		for (int img_idx = 0; img_idx < layer_count; img_idx++) {
			// We're concatenating the images horizontally; so we have to take a width-sized slice of the image
			// and copy it into the new image data
			size_t start_idx = i * width * 4;
			size_t end_idx = start_idx + (width * 4);
			ERR_FAIL_COND_V(images_data[img_idx].size() < end_idx, ERR_PARSE_ERROR);
			new_image_data.append_array(images_data[img_idx].slice(start_idx, end_idx));
		}
	}
	Ref<Image> img = Image::create_from_data(new_width, new_height, false, Image::FORMAT_RGBA8, new_image_data);
	ERR_FAIL_COND_V_MSG(img.is_null(), ERR_PARSE_ERROR, "Failed to create image for texture " + p_path);
	image_format = Image::get_format_name(img->get_format());
	err = save_image(dest_path, img, lossy);
	if (err == ERR_UNAVAILABLE) {
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	if (!had_valid_params && iinfo.is_valid()) {
		iinfo->set_param("slices/horizontal", num_images_w);
		iinfo->set_param("slices/vertical", num_images_h);
	}
	print_verbose("Converted " + p_path + " to " + dest_path);
	return OK;
}

Error TextureExporter::_convert_layered_2d(const String &p_path, const String &dest_path, bool lossy, String &image_format, Ref<ExportReport> report) {
	Error err;
	String dst_dir = dest_path.get_base_dir();
	Ref<TextureLayered> tex;
	tex = ResourceCompatLoader::non_global_load(p_path, "", &err);
	Ref<ImportInfo> iinfo;
	if (report.is_valid()) {
		iinfo = report->get_import_info();
	}

	if (err == ERR_UNAVAILABLE) {
		image_format = "Unknown deprecated image format";
		print_line("Did not convert deprecated Texture resource " + p_path);
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK || tex.is_null(), err, "Failed to load texture " + p_path);

	auto layer_count = tex->get_layers();
	int width = tex->get_width();
	int height = tex->get_height();
	Vector<Ref<Image>> images;
	Vector<Vector<uint8_t>> images_data;
	for (int64_t i = 0; i < layer_count; i++) {
		Ref<Image> img = tex->get_layer_data(i);
		if (img.is_null()) {
			if (report.is_valid()) {
				report->set_error(ERR_PARSE_ERROR);
				report->set_message("Failed to load image for texture " + p_path);
			}
			return err;
		}
		if (img->is_compressed() && img->has_mipmaps()) {
			img->clear_mipmaps();
		}
		GDRE_ERR_DECOMPRESS_OR_FAIL(img);
		if (img->get_format() != Image::FORMAT_RGBA8) {
			img->convert(Image::FORMAT_RGBA8);
		}
		images.push_back(img);
		images_data.push_back(img->get_data());
	}
	int64_t num_images_w = -1;
	int64_t num_images_h = -1;
	int64_t arrangement = -1;
	int64_t layout = -1;
	int64_t amount = -1;
	size_t new_width;
	size_t new_height;
	auto mode = tex->get_layered_type();
	Dictionary params;
	if (iinfo.is_valid()) {
		params = iinfo->get_params();
	}
	bool had_valid_params = false;
	if (mode == TextureLayered::LAYERED_TYPE_2D_ARRAY) {
		// get the square root of the number of layers; if it's a whole number, then we have a square
		if (iinfo.is_valid()) {
			num_images_w = params.get("slices/horizontal", -1);
			num_images_h = params.get("slices/vertical", -1);
			had_valid_params = num_images_w != -1 && num_images_h != -1;
		}
		if (!had_valid_params) {
			if (layer_count == 64) {
				num_images_w = 8;
				num_images_h = 8;
			} else if (layer_count == 16) {
				num_images_w = 4;
				num_images_h = 4;
			} else if (layer_count == 4) {
				num_images_w = 2;
				num_images_h = 2;
			} else {
				num_images_w = tex->get_layers();
				num_images_h = 1;
			}
		}
	} else if (mode == TextureLayered::LAYERED_TYPE_CUBEMAP || mode == TextureLayered::LAYERED_TYPE_CUBEMAP_ARRAY) {
		if (iinfo.is_valid()) {
			arrangement = params.get("slices/arrangement", -1);
			if (arrangement != -1 && mode == TextureLayered::LAYERED_TYPE_CUBEMAP) {
				had_valid_params = true;
			}
			if (arrangement == 0) {
				num_images_w = 1;
				num_images_h = 6;
			} else if (arrangement == 1) {
				num_images_w = 2;
				num_images_h = 3;
			} else if (arrangement == 2) {
				num_images_w = 3;
				num_images_h = 2;
			} else if (arrangement == 3) {
				num_images_w = 6;
				num_images_h = 1;
			}
			if (arrangement != -1 && mode == TextureLayered::LAYERED_TYPE_CUBEMAP_ARRAY) {
				layout = params.get("slices/layout", -1);
				amount = params.get("slices/amount", -1);
				if (layout != -1 && amount != -1) {
					if (layout == 0) {
						num_images_w *= amount;
					} else if (layout == 1) {
						num_images_h *= amount;
					}
					had_valid_params = true;
				}
			}
		}
		if (!had_valid_params) {
			arrangement = 0;
			layout = 0;
			amount = layer_count / 6;
			num_images_w = layer_count;
			num_images_h = 1;
		}
	}
	new_width = width * num_images_w;
	new_height = height * num_images_h;

	Vector<uint8_t> new_image_data;

	for (int i = 0; i < new_height; i++) {
		for (int img_idx = 0; img_idx < layer_count; img_idx++) {
			// We're concatenating the images horizontally; so we have to take a width-sized slice of the image
			// and copy it into the new image data
			size_t start_idx = i * width * 4;
			size_t end_idx = start_idx + (width * 4);
			ERR_FAIL_COND_V(images_data[img_idx].size() < end_idx, ERR_PARSE_ERROR);
			new_image_data.append_array(images_data[img_idx].slice(start_idx, end_idx));
		}
	}
	Ref<Image> img = Image::create_from_data(new_width, new_height, false, Image::FORMAT_RGBA8, new_image_data);
	ERR_FAIL_COND_V_MSG(img.is_null(), ERR_PARSE_ERROR, "Failed to create image for texture " + p_path);
	image_format = Image::get_format_name(img->get_format());
	err = save_image(dest_path, img, lossy);
	if (err == ERR_UNAVAILABLE) {
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	if (!had_valid_params && iinfo.is_valid()) {
		if (mode == TextureLayered::LAYERED_TYPE_2D_ARRAY) {
			iinfo->set_param("slices/horizontal", num_images_w);
			iinfo->set_param("slices/vertical", num_images_h);
		} else if (mode == TextureLayered::LAYERED_TYPE_CUBEMAP) {
			// 1x6
			iinfo->set_param("slices/arrangement", 0);
		} else if (mode == TextureLayered::LAYERED_TYPE_CUBEMAP_ARRAY) {
			// 1x6
			iinfo->set_param("slices/arrangement", 0);
			iinfo->set_param("slices/layout", 0);
			iinfo->set_param("slices/amount", layer_count / 6);
		}
	}
	print_verbose("Converted " + p_path + " to " + dest_path);
	return OK;
}

Ref<ExportReport> TextureExporter::export_resource(const String &output_dir, Ref<ImportInfo> iinfo) {
	String path = iinfo->get_path();
	String source = iinfo->get_source_file();
	bool lossy = false;
	int ver_major = iinfo->get_ver_major();
	int ver_minor = iinfo->get_ver_minor();
	Ref<ExportReport> report = memnew(ExportReport(iinfo));

	// Sonic Colors Unlimited specific hack: We don't support atsc and nx-low formats, so we need to set the path to something else
	if (ver_major == 3 && ver_minor == 1) {
		String format_type = path.get_basename().get_extension();
		Vector<String> banned_types = { "atsc", "nx-low", "atsc-low" };
		if (banned_types.has(format_type)) {
			bool found = false;
			Vector<String> dest_files = iinfo->get_dest_files();
			if (dest_files.size() > 0) {
				String new_path = path.get_basename().get_basename() + ".s3tc" + path.get_extension();
				if (FileAccess::exists(new_path)) {
					path = new_path;
					found = true;
				}
				if (!found) {
					for (auto &dest : dest_files) {
						String fmt = dest.get_basename().get_extension();
						if (!banned_types.has(format_type) && FileAccess::exists(new_path)) {
							path = dest;
							found = true;
							break;
						}
					}
				}
			}
			if (!found) {
				report->set_error(ERR_UNAVAILABLE);
				report->set_message("Cannot convert custom SCU texture format");
				report->set_unsupported_format_type(format_type);
			}
		}
	}
	if (!FileAccess::exists(path)) {
		path = "";
		for (auto &dest : iinfo->get_dest_files()) {
			if (FileAccess::exists(dest)) {
				path = dest;
				break;
			}
		}
		if (path.is_empty()) {
			report->set_error(ERR_FILE_NOT_FOUND);
			report->set_message("No existing textures found for this import");
			report->append_message_detail({ "Possibles:" });
			report->append_message_detail(iinfo->get_dest_files());
			return report;
		}
	}
	// for Godot 2.x resources, we can easily rewrite the metadata to point to a renamed file with a different extension,
	// but this isn't the case for 3.x and greater, so we have to save in the original (lossy) format.
	String source_ext = source.get_extension().to_lower();
	if (source_ext != "png" || ver_major == 2) {
		if (ver_major > 2) {
			if ((source_ext == "jpg" || source_ext == "jpeg")) {
				lossy = true;
				report->set_loss_type(ImportInfo::STORED_LOSSY);
			} else if (source_ext == "webp") {
				// if the engine is <3.4, it can't handle lossless encoded WEBPs
				if (ver_major < 4 && !(ver_major == 3 && ver_minor >= 4)) {
					lossy = true;
				}
				report->set_loss_type(ImportInfo::STORED_LOSSY);
			} else if (source_ext == "tga") {
				lossy = false;
			} else if (source_ext == "svg") {
				lossy = true;
				report->set_loss_type(ImportInfo::STORED_LOSSY);
			} else if (source_ext == "dds") {
				lossy = false;
			} else if (source_ext == "exr") {
				lossy = false;
			} else {
				iinfo->set_export_dest(iinfo->get_export_dest().get_basename() + ".png");
				// If this is version 3-4, we need to rewrite the import metadata to point to the new resource name
				// save it under .assets, which won't be picked up for import by the godot editor
				if (false) {
					// disable this for now
					// iinfo->set_source_file(iinfo->get_export_dest());
				} else {
					if (!iinfo->get_export_dest().replace("res://", "").begins_with(".assets")) {
						String prefix = ".assets";
						if (iinfo->get_export_dest().begins_with("res://")) {
							prefix = "res://.assets";
						}
						iinfo->set_export_dest(prefix.path_join(iinfo->get_export_dest().replace("res://", "")));
					}
				}
			}
		} else { //version 2
			iinfo->set_export_dest(iinfo->get_export_dest().get_basename() + ".png");
		}
	}
	report->set_new_source_path(iinfo->get_export_dest());

	Error err;
	String img_format = "bitmap";
	String importer = iinfo->get_importer();
	String dest_path = output_dir.path_join(iinfo->get_export_dest().replace("res://", ""));
	if (importer == "image") {
		ResourceFormatLoaderImage rli;
		Ref<Image> img = rli.load(path, "", &err, false, nullptr, ResourceFormatLoader::CACHE_MODE_IGNORE);
		if (!err && !img.is_null()) {
			img_format = Image::get_format_name(img->get_format());
			err = save_image(dest_path, img, lossy);
		}
	} else if (importer == "texture_atlas") {
		err = _convert_atex(path, dest_path, lossy, img_format);
	} else if (importer == "bitmap") {
		err = _convert_bitmap(path, dest_path, lossy);
		// } else if (importer == "cubemap_texture") {
		// 	err = _convert_cubemap(path, dest_path, lossy, img_format);
	} else if (importer == "2d_array_texture" || importer == "cubemap_array_texture" || importer == "cubemap_texture" || importer == "texture_array") {
		err = _convert_layered_2d(path, dest_path, lossy, img_format, report);
		// } else if (importer == "cubemap_array_texture") {
		// 	err = _convert_cubemap_array(path, dest_path, lossy, img_format);
	} else if (importer == "3d_texture" || importer == "texture_3d") {
		err = _convert_3d(path, dest_path, lossy, img_format, report);
	} else if (importer == "texture") {
		err = _convert_tex(path, dest_path, lossy, img_format);
	} else {
		report->set_error(ERR_UNAVAILABLE);
		report->set_message("Unsupported texture importer: " + importer);
		return report;
	}
	report->set_error(err);
	if (err == ERR_UNAVAILABLE) {
		report->set_unsupported_format_type(img_format);
		report->set_message("Decompression not implemented yet for texture format " + img_format);
		// Already reported in export functions above
		return report;
	} else if (err) {
		return report;
	}
	report->set_saved_path(dest_path);
	// If lossy, also convert it as a png
	if (lossy && importer == "texture") {
		String dest = iinfo->get_export_dest().get_basename() + ".png";
		if (!dest.replace("res://", "").begins_with(".assets")) {
			String prefix = ".assets";
			if (dest.begins_with("res://")) {
				prefix = "res://.assets";
			}
			dest = prefix.path_join(dest.replace("res://", ""));
		}
		iinfo->set_export_lossless_copy(dest);
		dest_path = output_dir.path_join(dest.replace("res://", ""));
		err = _convert_tex(path, dest_path, false, img_format);
		ERR_FAIL_COND_V(err != OK, report);
	}

	return report;
}

void TextureExporter::get_handled_types(List<String> *out) const {
	out->push_back("Texture");
	out->push_back("Texture2D");
	out->push_back("ImageTexture");
	out->push_back("StreamTexture");
	out->push_back("CompressedTexture2D");
	out->push_back("BitMap");
	out->push_back("AtlasTexture");
	out->push_back("StreamTexture");
	out->push_back("StreamTexture3D");
	out->push_back("StreamTextureArray");
	out->push_back("CompressedTexture2D");
	out->push_back("CompressedTexture3D");
	out->push_back("CompressedTextureLayered");
	out->push_back("CompressedTexture2DArray");
	out->push_back("CompressedCubemap");
	out->push_back("CompressedCubemapArray");
	out->push_back("TextureArray");
}

void TextureExporter::get_handled_importers(List<String> *out) const {
	out->push_back("texture");
	out->push_back("bitmap");
	out->push_back("image");
	out->push_back("texture_atlas");
	out->push_back("texture_array");
	out->push_back("cubemap_texture");
	out->push_back("2d_array_texture");
	out->push_back("cubemap_array_texture");
	out->push_back("texture_3d");
	out->push_back("3d_texture");
}
