#pragma once

#include "core/io/image.h"

class ImageSaver : public Object {
	GDCLASS(ImageSaver, Object);
	static Error _save_images_as_animated_gif(const String &p_path, const TypedArray<Image> &p_images, const Vector<float> &frame_durations_s, int quality = 100);

	static const Vector<String> supported_extensions;

public:
	static Error decompress_image(const Ref<Image> &p_image);
	static Error save_image(const String &p_path, const Ref<Image> &p_image, bool p_lossy, float p_quality = 1.0);
	static Error save_image_as_tga(const String &p_path, const Ref<Image> &p_image);
	static Error save_image_as_svg(const String &p_path, const Ref<Image> &p_image);
	static Error save_image_as_bmp(const String &p_path, const Ref<Image> &p_image);
	static Error save_image_as_gif(const String &p_path, const Ref<Image> &p_image);
	static Error save_images_as_animated_gif(const String &p_path, const Vector<Ref<Image>> &p_images, const Vector<float> &frame_durations_s, int quality = 100);
	static bool dest_format_supports_mipmaps(const String &p_ext);
	static Error save_image_as_hdr(const String &p_path, const Ref<Image> &p_image);
	static Vector<String> get_supported_extensions();
	static bool is_supported_extension(const String &p_ext);

protected:
	static void _bind_methods();
};

#define GDRE_ERR_DECOMPRESS_OR_FAIL(img)                                      \
	{                                                                         \
		Error _err = ImageSaver::decompress_image(img);                       \
		if (_err == ERR_UNAVAILABLE) {                                        \
			return ERR_UNAVAILABLE;                                           \
		}                                                                     \
		ERR_FAIL_COND_V_MSG(_err != OK, _err, "Failed to decompress image."); \
	}
