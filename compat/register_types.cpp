#include "register_types.h"

#include "core/io/image_loader.h"
#include "core/object/class_db.h"

#include "compat/config_file_compat.h"
#include "compat/fake_csharp_script.h"
#include "compat/fake_gdscript.h"
#include "compat/fake_mesh.h"
#include "compat/fake_script.h"
#include "compat/font_loader_compat.h"
#include "compat/ico_loader.h"
#include "compat/input_event_parser_v2.h"
#include "compat/oggstr_loader_compat.h"
#include "compat/optimized_translation_extractor.h"
#include "compat/resource_compat_binary.h"
#include "compat/resource_compat_obdb.h"
#include "compat/resource_compat_text.h"
#include "compat/resource_loader_compat.h"
#include "compat/sample_loader_compat.h"
#include "compat/script_loader.h"
#include "compat/texture_loader_compat.h"
#include "compat/video_stream_compat.h"
#include "compat/visual_shader_compat.h"

//loaders
static Ref<ResourceFormatLoaderCompatText> text_loader = nullptr;
static Ref<ResourceFormatLoaderCompatBinary> binary_loader = nullptr;
static Ref<ResourceFormatLoaderCompatOBDB> obdb_loader = nullptr;
static Ref<ResourceFormatLoaderCompatTexture2D> texture_loader = nullptr;
static Ref<ResourceFormatLoaderCompatTexture3D> texture3d_loader = nullptr;
static Ref<ResourceFormatLoaderCompatTextureLayered> texture_layered_loader = nullptr;
static Ref<ResourceFormatLoaderImageTextureCompat> image_texture_loader = nullptr;
static Ref<ResourceFormatLoaderCompatFont> font_loader = nullptr;
static Ref<ResourceFormatGDScriptLoader> script_loader = nullptr;
static Ref<ResourceFormatLoaderCompatImage> image_loader = nullptr;
static Ref<ResourceFormatLoaderCompatVideo> video_loader = nullptr;
static Ref<ImageLoaderICO> ico_loader = nullptr;

//converters
static Ref<SampleConverterCompat> sample_converter = nullptr;
static Ref<ResourceConverterTexture2D> texture_converter = nullptr;
static Ref<ImageConverterCompat> image_converter = nullptr;
static Ref<ImageTextureConverterCompat> image_texture_converter = nullptr;
static Ref<OggStreamConverterCompat> ogg_converter = nullptr;
static Ref<LargeTextureConverterCompat> large_texture_converter = nullptr;
static Ref<FakeScriptConverterCompat> fake_script_converter = nullptr;
static Ref<TranslationConverterCompat> translation_converter = nullptr;
static Ref<InputEventConverterCompat> input_event_converter = nullptr;
static Ref<VisualShaderConverterCompat> visual_shader_converter = nullptr;
static Ref<VisualShaderNodeCustomConverterCompat> visual_shader_node_custom_converter = nullptr;

void init_loaders() {
	text_loader = memnew(ResourceFormatLoaderCompatText);
	binary_loader = memnew(ResourceFormatLoaderCompatBinary);
	obdb_loader = memnew(ResourceFormatLoaderCompatOBDB);
	texture_loader = memnew(ResourceFormatLoaderCompatTexture2D);
	texture3d_loader = memnew(ResourceFormatLoaderCompatTexture3D);
	texture_layered_loader = memnew(ResourceFormatLoaderCompatTextureLayered);
	image_texture_loader = memnew(ResourceFormatLoaderImageTextureCompat);
	font_loader = memnew(ResourceFormatLoaderCompatFont);
	script_loader = memnew(ResourceFormatGDScriptLoader);
	image_loader = memnew(ResourceFormatLoaderCompatImage);
	video_loader = memnew(ResourceFormatLoaderCompatVideo);
	sample_converter = memnew(SampleConverterCompat);
	texture_converter = memnew(ResourceConverterTexture2D);
	image_converter = memnew(ImageConverterCompat);
	image_texture_converter = memnew(ImageTextureConverterCompat);
	ogg_converter = memnew(OggStreamConverterCompat);
	large_texture_converter = memnew(LargeTextureConverterCompat);
	fake_script_converter = memnew(FakeScriptConverterCompat);
	translation_converter = memnew(TranslationConverterCompat);
	input_event_converter = memnew(InputEventConverterCompat);
	visual_shader_converter = memnew(VisualShaderConverterCompat);
	visual_shader_node_custom_converter = memnew(VisualShaderNodeCustomConverterCompat);
	ResourceCompatLoader::add_resource_format_loader(binary_loader, true);
	ResourceCompatLoader::add_resource_format_loader(obdb_loader, true);
	ResourceCompatLoader::add_resource_format_loader(text_loader, true);
	ResourceCompatLoader::add_resource_format_loader(image_texture_loader, true);
	ResourceCompatLoader::add_resource_format_loader(texture_loader, true);
	ResourceCompatLoader::add_resource_format_loader(texture3d_loader, true);
	ResourceCompatLoader::add_resource_format_loader(texture_layered_loader, true);
	ResourceCompatLoader::add_resource_format_loader(font_loader, true);
	ResourceCompatLoader::add_resource_format_loader(script_loader, true);
	ResourceCompatLoader::add_resource_format_loader(image_loader, true);
	ResourceCompatLoader::add_resource_format_loader(video_loader, true);
	ResourceCompatLoader::add_resource_object_converter(sample_converter, true);
	ResourceCompatLoader::add_resource_object_converter(texture_converter, true);
	ResourceCompatLoader::add_resource_object_converter(image_converter, true);
	ResourceCompatLoader::add_resource_object_converter(image_texture_converter, true);
	ResourceCompatLoader::add_resource_object_converter(ogg_converter, true);
	ResourceCompatLoader::add_resource_object_converter(large_texture_converter, true);
	ResourceCompatLoader::add_resource_object_converter(fake_script_converter, true);
	ResourceCompatLoader::add_resource_object_converter(translation_converter, true);
	ResourceCompatLoader::add_resource_object_converter(input_event_converter, true);
	ResourceCompatLoader::add_resource_object_converter(visual_shader_converter, true);
	ResourceCompatLoader::add_resource_object_converter(visual_shader_node_custom_converter, true);
}

void deinit_loaders() {
	if (text_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(text_loader);
	}
	if (binary_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(binary_loader);
	}
	if (obdb_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(obdb_loader);
	}
	if (texture_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(texture_loader);
	}
	if (texture3d_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(texture3d_loader);
	}
	if (texture_layered_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(texture_layered_loader);
	}
	if (image_texture_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(image_texture_loader);
	}
	if (font_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(font_loader);
	}
	if (script_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(script_loader);
	}
	if (image_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(image_loader);
	}
	if (video_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(video_loader);
	}
	if (sample_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(sample_converter);
	}
	if (texture_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(texture_converter);
	}
	if (image_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(image_converter);
	}
	if (image_texture_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(image_texture_converter);
	}
	if (ogg_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(ogg_converter);
	}
	if (large_texture_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(large_texture_converter);
	}
	if (fake_script_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(fake_script_converter);
	}
	if (translation_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(translation_converter);
	}
	if (input_event_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(input_event_converter);
	}
	if (visual_shader_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(visual_shader_converter);
	}
	if (visual_shader_node_custom_converter.is_valid()) {
		ResourceCompatLoader::remove_resource_object_converter(visual_shader_node_custom_converter);
	}
	text_loader = nullptr;
	binary_loader = nullptr;
	obdb_loader = nullptr;
	texture_loader = nullptr;
	texture3d_loader = nullptr;
	texture_layered_loader = nullptr;
	image_texture_loader = nullptr;
	font_loader = nullptr;
	script_loader = nullptr;
	image_loader = nullptr;
	video_loader = nullptr;
	sample_converter = nullptr;
	texture_converter = nullptr;
	image_converter = nullptr;
	image_texture_converter = nullptr;
	ogg_converter = nullptr;
	large_texture_converter = nullptr;
	fake_script_converter = nullptr;
	translation_converter = nullptr;
	input_event_converter = nullptr;
	visual_shader_converter = nullptr;
	visual_shader_node_custom_converter = nullptr;
}
void initialize_gdsdecomp_compat_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<ConfigFileCompat>();
	ClassDB::register_class<CoreBind::ResourceCompatLoader>();
	ClassDB::register_class<CompatFormatLoader>();
	ClassDB::register_class<ResourceFormatLoaderCompatText>();
	ClassDB::register_class<ResourceFormatLoaderCompatBinary>();
	ClassDB::register_class<ResourceFormatLoaderCompatOBDB>();
	ClassDB::register_class<ResourceFormatLoaderCompatTexture2D>();
	ClassDB::register_class<ResourceFormatLoaderCompatTexture3D>();
	ClassDB::register_class<ResourceFormatLoaderCompatTextureLayered>();
	ClassDB::register_class<ResourceFormatLoaderImageTextureCompat>();
	ClassDB::register_class<ResourceFormatLoaderCompatFont>();
	ClassDB::register_class<ResourceFormatGDScriptLoader>();
	ClassDB::register_class<ResourceFormatLoaderCompatImage>();
	ClassDB::register_class<ResourceFormatLoaderCompatVideo>();
	ClassDB::register_class<LargeTextureConverterCompat>();
	// TODO: make ResourceCompatConverter non-abstract
	ClassDB::register_abstract_class<ResourceCompatConverter>();
	ClassDB::register_class<SampleConverterCompat>();
	ClassDB::register_class<ResourceConverterTexture2D>();
	ClassDB::register_class<ImageConverterCompat>();
	ClassDB::register_class<ImageTextureConverterCompat>();
	ClassDB::register_class<OggStreamConverterCompat>();
	ClassDB::register_class<FakeScriptConverterCompat>();
	ClassDB::register_class<TranslationConverterCompat>();
	ClassDB::register_class<InputEventConverterCompat>();
	ClassDB::register_class<OptimizedTranslationExtractor>();

	ClassDB::register_class<FakeScript>();
	ClassDB::register_class<FakeGDScript>();
	ClassDB::register_class<FakeCSharpScript>();
	ClassDB::register_class<FakeMesh>();
	init_loaders();

	// Register ICO image loader
	ico_loader.instantiate();
	ImageLoader::add_image_format_loader(ico_loader);
}

void uninitialize_gdsdecomp_compat_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	deinit_loaders();
	if (ico_loader.is_valid()) {
		ImageLoader::remove_image_format_loader(ico_loader);
		ico_loader.unref();
	}
	ico_loader = nullptr;
}
