/**************************************************************************/
/*  file_access_buffer.cpp                                                */
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

#include "file_access_buffer.h"

#include "core/config/project_settings.h"

#include "core/os/memory.h"
#include "core/templates/vector.h"

static HashMap<String, Vector<uint8_t>> *files = nullptr;

void FileAccessBuffer::register_file(const String &p_name, const Vector<uint8_t> &p_data) {
	if (!files) {
		files = memnew((HashMap<String, Vector<uint8_t>>));
	}

	String name;
	if (ProjectSettings::get_singleton()) {
		name = ProjectSettings::get_singleton()->globalize_path(p_name);
	} else {
		name = p_name;
	}
	//name = DirAccess::normalize_path(name);

	(*files)[name] = p_data;
}

void FileAccessBuffer::cleanup() {
	if (!files) {
		return;
	}

	memdelete(files);
}

Ref<FileAccess> FileAccessBuffer::create() {
	return memnew(FileAccessBuffer);
}

bool FileAccessBuffer::file_exists(const String &p_name) {
	String name = fix_path(p_name);
	//name = DirAccess::normalize_path(name);

	return files && (files->find(name) != nullptr);
}

Error FileAccessBuffer::open_new() {
	data.clear();
	pos = 0;
	open = true;
	return OK;
}

Error FileAccessBuffer::open_custom(const Vector<uint8_t> &p_data) {
	data = p_data;
	pos = 0;
	open = true;
	return OK;
}

Error FileAccessBuffer::open_internal(const String &p_path, int p_mode_flags) {
	ERR_FAIL_NULL_V(files, ERR_FILE_NOT_FOUND);

	String name = fix_path(p_path);
	//name = DirAccess::normalize_path(name);

	HashMap<String, Vector<uint8_t>>::Iterator E = files->find(name);
	ERR_FAIL_COND_V_MSG(!E, ERR_FILE_NOT_FOUND, vformat("Can't find file '%s'.", p_path));

	data = E->value;
	pos = 0;

	return OK;
}

bool FileAccessBuffer::is_open() const {
	return open;
}

void FileAccessBuffer::seek(uint64_t p_position) {
	ERR_FAIL_COND(!open);
	pos = p_position;
}

void FileAccessBuffer::seek_end(int64_t p_position) {
	ERR_FAIL_COND(!open);
	pos = data.size() + p_position;
}

uint64_t FileAccessBuffer::get_position() const {
	ERR_FAIL_COND_V(!open, 0);
	return pos;
}

uint64_t FileAccessBuffer::get_length() const {
	ERR_FAIL_COND_V(!open, 0);
	return data.size();
}

bool FileAccessBuffer::eof_reached() const {
	ERR_FAIL_COND_V(!open, true);
	return pos >= data.size();
}

uint64_t FileAccessBuffer::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	if (!p_length) {
		return 0;
	}

	ERR_FAIL_COND_V(!open, -1);
	ERR_FAIL_NULL_V(p_dst, -1);

	uint64_t left = data.size() - pos;
	uint64_t read = MIN(p_length, left);

	if (read < p_length) {
		WARN_PRINT("Reading less data than requested");
	}

	memcpy(p_dst, &data[pos], read);
	pos += read;

	return read;
}

Error FileAccessBuffer::get_error() const {
	return pos >= data.size() ? ERR_FILE_EOF : OK;
}

Error FileAccessBuffer::resize(int64_t p_length) {
	ERR_FAIL_COND_V(!open, ERR_UNAVAILABLE);
	data.resize(p_length);
	return OK;
}

void FileAccessBuffer::flush() {
	ERR_FAIL_COND(!open);
}

bool FileAccessBuffer::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	if (!p_length) {
		return true;
	}
	ERR_FAIL_COND_V(!open, false);
	ERR_FAIL_NULL_V(p_src, false);


	// check if data is large enough
	if (pos + p_length > data.size()) {
		data.resize((pos + p_length + (MIN(p_length, 16 * 1024))));
	}

	memcpy(data.ptrw() + pos, p_src, p_length);
	pos += p_length;

	return true;
}

String FileAccessBuffer::get_as_utf8_string(bool p_skip_cr) const {
	ERR_FAIL_COND_V(!open, "");
	uint64_t len = data.size() - pos;
	String s;
	s.append_utf8((const char *)data.ptr() + pos, len, p_skip_cr);
	return s;
}


String FileAccessBuffer::whole_file_as_utf8_string(bool p_skip_cr) const {
	ERR_FAIL_COND_V(!open, "");
	uint64_t len = data.size();
	String s;
	s.append_utf8((const char *)data.ptr(), len, p_skip_cr);
	return s;
}

