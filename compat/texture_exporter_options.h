#pragma once
#include "core/io/resource_importer.h"

namespace v3Preset {
enum v3Preset {
	PRESET_DETECT,
	PRESET_2D,
	PRESET_2D_PIXEL,
	PRESET_3D,
};
}

namespace v4Preset {
enum v4Preset {
	PRESET_DETECT,
	PRESET_2D,
	PRESET_3D,
};
}

namespace v2Flags {
enum v2Flags {

	IMAGE_FLAG_STREAM_FORMAT = 1,
	IMAGE_FLAG_FIX_BORDER_ALPHA = 2,
	IMAGE_FLAG_ALPHA_BIT = 4, //hint for compressions that use a bit for alpha
	IMAGE_FLAG_COMPRESS_EXTRA = 8, // used for pvrtc2
	IMAGE_FLAG_NO_MIPMAPS = 16, //normal for 2D games
	IMAGE_FLAG_REPEAT = 32, //usually disabled in 2D
	IMAGE_FLAG_FILTER = 64, //almost always enabled
	IMAGE_FLAG_PREMULT_ALPHA = 128, //almost always enabled
	IMAGE_FLAG_CONVERT_TO_LINEAR = 256, //convert image to linear
	IMAGE_FLAG_CONVERT_NORMAL_TO_XY = 512, //convert image to linear
	IMAGE_FLAG_USE_ANISOTROPY = 1024, //convert image to linear

};
}

void get_import_options_v4(List<ResourceImporter::ImportOption> *r_options, int p_preset) {
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "compress/mode", PROPERTY_HINT_ENUM, "Lossless,Lossy,VRAM Compressed,VRAM Uncompressed,Basis Universal", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), p_preset == v4Preset::PRESET_3D ? 2 : 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "compress/high_quality"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "compress/lossy_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.7));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "compress/hdr_compression", PROPERTY_HINT_ENUM, "Disabled,Opaque Only,Always"), 1));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "compress/normal_map", PROPERTY_HINT_ENUM, "Detect,Enable,Disabled"), 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "compress/channel_pack", PROPERTY_HINT_ENUM, "sRGB Friendly,Optimized"), 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "mipmaps/generate"), (p_preset == v4Preset::PRESET_3D ? true : false)));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "mipmaps/limit", PROPERTY_HINT_RANGE, "-1,256"), -1));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "roughness/mode", PROPERTY_HINT_ENUM, "Detect,Disabled,Red,Green,Blue,Alpha,Gray"), 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::STRING, "roughness/src_normal", PROPERTY_HINT_FILE, "*.bmp,*.dds,*.exr,*.jpeg,*.jpg,*.hdr,*.png,*.svg,*.tga,*.webp"), ""));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/fix_alpha_border"), p_preset != v4Preset::PRESET_3D));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/premult_alpha"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/normal_map_invert_y"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/hdr_as_srgb"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/hdr_clamp_exposure"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "process/size_limit", PROPERTY_HINT_RANGE, "0,4096,1"), 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "detect_3d/compress_to", PROPERTY_HINT_ENUM, "Disabled,VRAM Compressed,Basis Universal"), (p_preset == v4Preset::PRESET_DETECT) ? 1 : 0));
	// svg only, we ignore the path
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "svg/scale", PROPERTY_HINT_RANGE, "0.001,100,0.001"), 1.0));
}

// 3.x options
void get_import_options_v3(List<ResourceImporter::ImportOption> *r_options, int p_preset) {
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "compress/mode", PROPERTY_HINT_ENUM, "Lossless,Lossy,Video RAM,Uncompressed", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), p_preset == v3Preset::PRESET_3D ? 2 : 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "compress/lossy_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.7));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "compress/hdr_mode", PROPERTY_HINT_ENUM, "Enabled,Force RGBE"), 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "compress/bptc_ldr", PROPERTY_HINT_ENUM, "Enabled,RGBA Only"), 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "compress/normal_map", PROPERTY_HINT_ENUM, "Detect,Enable,Disabled"), 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "flags/repeat", PROPERTY_HINT_ENUM, "Disabled,Enabled,Mirrored"), p_preset == v3Preset::PRESET_3D ? 1 : 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "flags/filter"), p_preset != v3Preset::PRESET_2D_PIXEL));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "flags/mipmaps"), p_preset == v3Preset::PRESET_3D));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "flags/anisotropic"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "flags/srgb", PROPERTY_HINT_ENUM, "Disable,Enable,Detect"), 2));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/fix_alpha_border"), p_preset != v3Preset::PRESET_3D));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/premult_alpha"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/HDR_as_SRGB"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/invert_color"), false)); // removed in v4
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "process/normal_map_invert_y"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "stream"), false)); // removed in v4
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "size_limit", PROPERTY_HINT_RANGE, "0,4096,1"), 0));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "detect_3d"), p_preset == v3Preset::PRESET_DETECT));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "svg/scale", PROPERTY_HINT_RANGE, "0.001,100,0.001"), 1.0));
}

// BOOL , "atlas"
// INT  , "format" // V2ImportEnums::TextureFormat
// INT  , "flags" //v2Flags::v2Flags
// FLOAT, "quality" // lossy quality
// FLOAT, "shrink"
// BOOL , "crop"
// ARRAY, "rects" // Array of Rect2

void get_import_options_v2(List<ResourceImporter::ImportOption> *r_options, int p_preset) {
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "filter"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "gen_mipmaps"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "repeat"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "anisotropic"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "tolinear"), false));
	r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "mirroredrepeat"), false));
}

void get_import_options_v2_translated(List<ResourceImporter::ImportOption> *r_options, int p_preset) {
	// flags & IMAGE_FLAG_STREAM_FORMAT == stream
	// flags & IMAGE_FLAG_FILTER == flags/filter
	// etc...
}