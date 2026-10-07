#include "func_godot_exporter.h"
#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "scene/resources/texture.h"
#include "utility/common.h"

String FuncGodotLmpExporter::get_name() const {
	return EXPORTER_NAME;
}

Error FuncGodotLmpExporter::export_file(const String &out_path, const String &res_path) {
	auto res = ResourceCompatLoader::fake_load(res_path);
	ERR_FAIL_COND_V_MSG(res.is_null(), ERR_FILE_CANT_OPEN, "Failed to load resource: " + res_path);
	bool is_valid = false;
	PackedColorArray colors = res->get("colors", &is_valid);
	ERR_FAIL_COND_V_MSG(!is_valid || colors.is_empty(), ERR_INVALID_DATA, "Invalid palette data: " + res_path);

	PackedByteArray data;
	for (auto &color : colors) {
		data.push_back(CLAMP(color.r * 255, 0, 255));
		data.push_back(CLAMP(color.g * 255, 0, 255));
		data.push_back(CLAMP(color.b * 255, 0, 255));
	}
	gdre::ensure_dir(out_path.get_base_dir());
	Ref<FileAccess> fa = FileAccess::open(out_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(fa.is_null(), ERR_FILE_CANT_OPEN, "Failed to open file: " + out_path);
	if (!fa->store_buffer(data)) {
		return ERR_FILE_CANT_WRITE;
	}
	return OK;
}

Ref<ExportReport> FuncGodotLmpExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	Ref<ExportReport> report = Ref<ExportReport>(memnew(ExportReport(import_infos)));
	String dest = output_dir.path_join(import_infos->get_export_dest().trim_prefix("res://"));
	report->set_error(export_file(dest, import_infos->get_path()));
	report->set_resources_used({ import_infos->get_path() });
	if (report->get_error() == OK) {
		report->set_saved_path(dest);
	}
	// no options
	return report;
}

void FuncGodotLmpExporter::get_handled_types(List<String> *out) const {
}

void FuncGodotLmpExporter::get_handled_importers(List<String> *out) const {
	out->push_back("func_godot.palette");
	out->push_back("qodot.palette");
}

String FuncGodotLmpExporter::get_default_export_extension(const String &res_path) const {
	return "lmp";
}

Vector<String> FuncGodotLmpExporter::get_export_extensions(const String &res_path) const {
	Vector<String> extensions;
	extensions.push_back("lmp");
	return extensions;
}

String FuncGodotMapExporter::get_name() const {
	return EXPORTER_NAME;
}

Error FuncGodotMapExporter::export_file(const String &out_path, const String &res_path) {
	auto res = ResourceCompatLoader::fake_load(res_path);
	ERR_FAIL_COND_V_MSG(res.is_null(), ERR_FILE_CANT_OPEN, "Failed to load resource: " + res_path);
	bool is_valid = false;
	String data = res->get("map_data", &is_valid);
	// this is a fairly common occurrance, so we keep it quiet.
	if (!is_valid || data.is_empty()) {
		return ERR_PRINTER_ON_FIRE;
	}

	gdre::ensure_dir(out_path.get_base_dir());
	Ref<FileAccess> fa = FileAccess::open(out_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(fa.is_null(), ERR_FILE_CANT_OPEN, "Failed to open file for writing: " + out_path);
	if (!fa->store_string(data)) {
		ERR_FAIL_V_MSG(ERR_FILE_CANT_WRITE, "Failed to write map data to file: " + out_path);
	}
	return OK;
}

Ref<ExportReport> FuncGodotMapExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	Ref<ExportReport> report = Ref<ExportReport>(memnew(ExportReport(import_infos)));
	String dest = output_dir.path_join(import_infos->get_export_dest().trim_prefix("res://"));
	report->set_error(export_file(dest, import_infos->get_path()));
	if (report->get_error() == ERR_PRINTER_ON_FIRE) {
		report->set_message("Map data is empty, skipping export");
		report->set_error(ERR_PRINTER_ON_FIRE);
	}
	report->set_resources_used({ import_infos->get_path() });
	if (report->get_error() == OK) {
		report->set_saved_path(dest);
	}
	// no options
	return report;
}

void FuncGodotMapExporter::get_handled_types(List<String> *out) const {
}

void FuncGodotMapExporter::get_handled_importers(List<String> *out) const {
	out->push_back("func_godot.map");
	out->push_back("qodot.map");
}

String FuncGodotMapExporter::get_default_export_extension(const String &res_path) const {
	return "map";
}

Vector<String> FuncGodotMapExporter::get_export_extensions(const String &res_path) const {
	Vector<String> extensions;
	extensions.push_back("map");
	return extensions;
}

namespace {
constexpr int TEXTURE_NAME_LENGTH = 16;
constexpr int MAX_MIP_LEVELS = 4;
constexpr int PALETTE_SIZE = 256;
constexpr uint8_t QUAKE_TYPE_MIPS_TEXTURE = 0x44;
constexpr int QUAKE_TRANSPARENT_INDEX = 255;
constexpr int ALPHA_THRESHOLD = 128;
constexpr int MIPTEX_HEADER_SIZE = TEXTURE_NAME_LENGTH + sizeof(uint32_t) * 2 + sizeof(uint32_t) * MAX_MIP_LEVELS; // 40
constexpr int HEADER_SIZE = 4 + sizeof(uint32_t) * 2; // 12
constexpr int MOD = 16;
constexpr int MIN_DIM = 16;
constexpr const char *DEFAULT_PALETTE_PATH = "res://addons/func_godot/palette.lmp";
constexpr const char *DEFAULT_QODOT_PALETTE_PATH = "res://addons/qodot/palette.lmp";

Error _load_palette(const Ref<ImportInfo> &import_infos, PackedColorArray &r_palette) {
	String palette_path = DEFAULT_PALETTE_PATH;
	if (import_infos.is_valid() && import_infos->has_param("palette_file")) {
		String pal_var = import_infos->get_param("palette_file");
		if (!pal_var.is_empty()) {
			palette_path = pal_var;
		}
	} else if (import_infos.is_valid()) {
		if (import_infos->get_importer().begins_with("qodot")) {
			palette_path = DEFAULT_QODOT_PALETTE_PATH;
		}
		import_infos->set_param("palette_file", palette_path);
	}
	auto pal_res = ResourceCompatLoader::custom_load(palette_path, "", ResourceInfo::LoadType::FAKE_LOAD);
	ERR_FAIL_COND_V_MSG(pal_res.is_null(), ERR_FILE_CANT_OPEN, "Cannot load Quake palette: " + palette_path);
	bool valid = false;
	PackedColorArray colors = pal_res->get("colors", &valid);
	ERR_FAIL_COND_V_MSG(!valid || colors.size() != PALETTE_SIZE, ERR_INVALID_DATA,
			vformat("Quake palette '%s' is invalid (expected %d colors, got %d)", palette_path, PALETTE_SIZE, colors.size()));
	r_palette = colors;
	return OK;
}

int _round_up_mod16(int v) {
	if (v < MIN_DIM) {
		return MIN_DIM;
	}
	return (v + MOD - 1) & ~(MOD - 1);
}

Ref<Image> _resize_to_mod16(const Ref<Image> &p_img) {
	int w = p_img->get_width();
	int h = p_img->get_height();
	int new_w = _round_up_mod16(w);
	int new_h = _round_up_mod16(h);
	if (new_w == w && new_h == h) {
		return p_img;
	}
	Ref<Image> resized = p_img->duplicate();
	resized->resize(new_w, new_h, Image::INTERPOLATE_BILINEAR);
	return resized;
}

uint8_t _quantize_pixel(const Color &p_color, const PackedColorArray &p_palette) {
	if (p_color.a * 255.0f < ALPHA_THRESHOLD) {
		return QUAKE_TRANSPARENT_INDEX;
	}
	const float pr = p_color.r * 255.0f;
	const float pg = p_color.g * 255.0f;
	const float pb = p_color.b * 255.0f;
	int best_index = 0;
	float best_dist = Math::INF;
	const Color *palette_ptr = p_palette.ptr();
	// Restrict to indices 0..254; index 255 is reserved for transparency in Quake.
	for (int i = 0; i < QUAKE_TRANSPARENT_INDEX; i++) {
		const Color &c = palette_ptr[i];
		float dr = pr - c.r * 255.0f;
		float dg = pg - c.g * 255.0f;
		float db = pb - c.b * 255.0f;
		float dist = dr * dr + dg * dg + db * db;
		if (dist < best_dist) {
			best_dist = dist;
			best_index = i;
			if (dist == 0.0f) {
				break;
			}
		}
	}
	return (uint8_t)best_index;
}

PackedByteArray _quantize_image(const Ref<Image> &p_img, const PackedColorArray &p_palette) {
	int w = p_img->get_width();
	int h = p_img->get_height();
	PackedByteArray indices;
	indices.resize(w * h);
	uint8_t *out = indices.ptrw();
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			out[y * w + x] = _quantize_pixel(p_img->get_pixel(x, y), p_palette);
		}
	}
	return indices;
}

Vector<uint8_t> _build_miptex_entry(const String &p_name, const Ref<Image> &p_src_image, const PackedColorArray &p_palette) {
	Vector<uint8_t> blob;
	ERR_FAIL_COND_V(p_src_image.is_null(), blob);

	Ref<Image> img = p_src_image;
	if (img->get_format() != Image::FORMAT_RGBA8 || img->has_mipmaps()) {
		img = img->duplicate();
		if (img->has_mipmaps()) {
			img->clear_mipmaps();
		}
		if (img->get_format() != Image::FORMAT_RGBA8) {
			img->convert(Image::FORMAT_RGBA8);
		}
	}
	img = _resize_to_mod16(img);

	const int w = img->get_width();
	const int h = img->get_height();

	// Build the four mip levels independently so each is a clean RGBA resample,
	// then quantize each to 8-bit indices against the shared palette.
	PackedByteArray mip_pixels[MAX_MIP_LEVELS];
	mip_pixels[0] = _quantize_image(img, p_palette);
	Ref<Image> mip = img;
	for (int i = 1; i < MAX_MIP_LEVELS; i++) {
		int mw = MAX(w >> i, 1);
		int mh = MAX(h >> i, 1);
		mip = mip->duplicate();
		mip->resize(mw, mh, Image::INTERPOLATE_BILINEAR);
		mip_pixels[i] = _quantize_image(mip, p_palette);
	}

	uint32_t mip_offsets[MAX_MIP_LEVELS];
	uint32_t cursor = MIPTEX_HEADER_SIZE;
	for (int i = 0; i < MAX_MIP_LEVELS; i++) {
		mip_offsets[i] = cursor;
		cursor += (uint32_t)mip_pixels[i].size();
	}
	const uint32_t total_size = cursor;

	blob.resize(total_size);
	uint8_t *bw = blob.ptrw();
	memset(bw, 0, total_size);

	CharString name_utf8 = p_name.utf8();
	int name_len = MIN(name_utf8.length(), TEXTURE_NAME_LENGTH - 1);
	memcpy(bw, name_utf8.get_data(), name_len);

	auto write_le32 = [](uint8_t *dst, uint32_t v) {
		dst[0] = (uint8_t)(v & 0xff);
		dst[1] = (uint8_t)((v >> 8) & 0xff);
		dst[2] = (uint8_t)((v >> 16) & 0xff);
		dst[3] = (uint8_t)((v >> 24) & 0xff);
	};
	write_le32(bw + 16, (uint32_t)w);
	write_le32(bw + 20, (uint32_t)h);
	for (int i = 0; i < MAX_MIP_LEVELS; i++) {
		write_le32(bw + 24 + i * 4, mip_offsets[i]);
	}

	for (int i = 0; i < MAX_MIP_LEVELS; i++) {
		memcpy(bw + mip_offsets[i], mip_pixels[i].ptr(), mip_pixels[i].size());
	}

	return blob;
}

Error _export_wad_file(const String &out_path, const String &res_path, const Ref<ImportInfo> &import_infos) {
	auto res = ResourceCompatLoader::custom_load(res_path, "", ResourceInfo::LoadType::GLTF_LOAD, nullptr, false, ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP);
	ERR_FAIL_COND_V_MSG(res.is_null(), ERR_FILE_CANT_OPEN, "Failed to load resource: " + res_path);
	bool valid = false;
	Dictionary textures = res->get("textures", &valid);
	ERR_FAIL_COND_V_MSG(!valid || textures.is_empty(), ERR_INVALID_DATA, "QuakeWadFile has no textures: " + res_path);

	PackedColorArray palette;
	Error err = _load_palette(import_infos, palette);
	if (err != OK) {
		return err;
	}

	LocalVector<Variant> keys = textures.get_key_list();
	Vector<Vector<uint8_t>> blobs;
	Vector<String> names;
	blobs.reserve(keys.size());
	names.reserve(keys.size());

	bool has_mipmaps = false;
	for (const Variant &key : keys) {
		String name = key;
		Ref<Texture2D> tex = textures[key];
		if (tex.is_null()) {
			WARN_PRINT("Skipping non-Texture2D entry in WAD: " + name);
			continue;
		}
		Ref<Image> img = tex->get_image();
		if (img.is_null() || img->is_empty()) {
			WARN_PRINT("Skipping texture with no image data: " + name);
			continue;
		}
		has_mipmaps = has_mipmaps || img->has_mipmaps();
		Vector<uint8_t> blob = _build_miptex_entry(name, img, palette);
		if (blob.is_empty()) {
			WARN_PRINT("Failed to build miptex entry: " + name);
			continue;
		}
		blobs.push_back(blob);
		names.push_back(name);
	}
	if (import_infos.is_valid()) {
		import_infos->set_param("generate_mipmaps", has_mipmaps);
	}

	ERR_FAIL_COND_V_MSG(blobs.is_empty(), ERR_INVALID_DATA, "No exportable textures in WAD: " + res_path);

	uint32_t total_data_size = 0;
	Vector<uint32_t> entry_offsets;
	entry_offsets.resize(blobs.size());
	for (int i = 0; i < blobs.size(); i++) {
		entry_offsets.write[i] = HEADER_SIZE + total_data_size;
		total_data_size += (uint32_t)blobs[i].size();
	}
	uint32_t dir_offset = HEADER_SIZE + total_data_size;

	gdre::ensure_dir(out_path.get_base_dir());
	Ref<FileAccess> fa = FileAccess::open(out_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(fa.is_null(), ERR_FILE_CANT_OPEN, "Failed to open file for writing: " + out_path);

	fa->store_buffer((const uint8_t *)"WAD2", 4);
	fa->store_32((uint32_t)blobs.size());
	fa->store_32(dir_offset);

	for (int i = 0; i < blobs.size(); i++) {
		if (!fa->store_buffer(blobs[i])) {
			return ERR_FILE_CANT_WRITE;
		}
	}

	for (int i = 0; i < blobs.size(); i++) {
		uint32_t entry_size = (uint32_t)blobs[i].size();
		fa->store_32(entry_offsets[i]);
		fa->store_32(entry_size);
		fa->store_32(entry_size);
		fa->store_8(QUAKE_TYPE_MIPS_TEXTURE);
		fa->store_8(0); // no compression
		fa->store_16(0); // dummy

		uint8_t name_buf[TEXTURE_NAME_LENGTH] = { 0 };
		CharString name_utf8 = names[i].utf8();
		int name_len = MIN(name_utf8.length(), TEXTURE_NAME_LENGTH - 1);
		memcpy(name_buf, name_utf8.get_data(), name_len);
		fa->store_buffer(name_buf, TEXTURE_NAME_LENGTH);
	}

	if (fa->get_error() != OK && fa->get_error() != ERR_FILE_EOF) {
		return ERR_FILE_CANT_WRITE;
	}
	return OK;
}
} // namespace

String FuncGodotWADExporter::get_name() const {
	return EXPORTER_NAME;
}

Error FuncGodotWADExporter::export_file(const String &out_path, const String &res_path) {
	return _export_wad_file(out_path, res_path, Ref<ImportInfo>());
}

Ref<ExportReport> FuncGodotWADExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	Ref<ExportReport> report = Ref<ExportReport>(memnew(ExportReport(import_infos)));
	String dest = output_dir.path_join(import_infos->get_export_dest().trim_prefix("res://"));
	report->set_error(_export_wad_file(dest, import_infos->get_path(), import_infos));
	report->set_resources_used({ import_infos->get_path() });
	if (report->get_error() == OK) {
		report->set_saved_path(dest);
	}
	return report;
}

void FuncGodotWADExporter::get_handled_types(List<String> *out) const {
}

void FuncGodotWADExporter::get_handled_importers(List<String> *out) const {
	out->push_back("func_godot.wad");
	out->push_back("qodot.wad");
}

String FuncGodotWADExporter::get_default_export_extension(const String &res_path) const {
	return "wad";
}

Vector<String> FuncGodotWADExporter::get_export_extensions(const String &res_path) const {
	Vector<String> extensions;
	extensions.push_back("wad");
	return extensions;
}
