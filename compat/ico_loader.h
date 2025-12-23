/**************************************************************************/
/*  ico_loader.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/io/image_loader.h"

class ImageLoaderICO : public ImageFormatLoader {
	struct IconEntry {
		uint8_t width = 0;
		uint8_t height = 0;
		uint8_t colors = 0;
		uint8_t reserved = 0;
		uint16_t planes = 0;
		uint16_t bits_per_pixel = 0;
		uint32_t image_size = 0;
		uint32_t image_offset = 0;
	};

	static Error load_embedded_image(const IconEntry &p_icon_entry, Ref<Image> p_image, const Vector<uint8_t> &p_data, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale);

public:
	virtual Error load_image(Ref<Image> p_image, Ref<FileAccess> p_fileaccess, BitField<ImageFormatLoader::LoaderFlags> p_flags = FLAG_NONE, float p_scale = 1.0) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;

	ImageLoaderICO();
};
