/**************************************************************************/
/*  file_access_encrypted.cpp                                             */
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

#include "file_access_encrypted_custom.h"

#include "core/error/error_list.h"
#include "core/io/file_access_encrypted.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"

Error FileAccessEncryptedCustom::open_and_parse(Ref<FileAccess> p_base, const Vector<uint8_t> &p_key, Mode p_mode, bool p_with_magic, const Vector<uint8_t> &p_iv) {
	ERR_FAIL_NULL_V_MSG(decryptor, ERR_UNCONFIGURED, "Decryptor is not set.");
	ERR_FAIL_COND_V_MSG(file.is_valid(), ERR_ALREADY_IN_USE, vformat("Can't open file while another file from path '%s' is open.", file->get_path_absolute()));
	ERR_FAIL_COND_V(p_key.size() != decryptor->get_required_key_size_in_bytes(), ERR_INVALID_PARAMETER);

	pos = 0;
	eofed = false;
	use_magic = p_with_magic;

	if (p_mode == MODE_WRITE) {
		ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Writing to a custom encrypted file is not supported.");
	} else if (p_mode == MODE_READ) {
		writing = false;
		key = p_key;
		Dictionary result = decryptor->parse_and_decrypt(p_base, key, use_magic);
		ERR_FAIL_COND_V_MSG(result.is_empty(), ERR_BUG, "Decryptor did not return a result.");
		ERR_FAIL_COND_V_MSG(!result.has("error") || !result.has("length") || !result.has("data"), ERR_BUG, "Decryptor result is missing required keys;\nEnsure that the decryptor returns a Dictionary with the following keys: 'error', 'length', and 'data'.");
		Error err = result["error"];
		ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to decrypt data.");
		length = result["length"];
		data = result["data"];
		ERR_FAIL_COND_V_MSG(length != data.size(), ERR_UNAUTHORIZED, "Decrypted data size mismatch; 'length' does not match data.size()");
		file = p_base;
	}

	return OK;
}

Error FileAccessEncryptedCustom::open_and_parse_password(Ref<FileAccess> p_base, const String &p_key, Mode p_mode) {
	String cs = p_key.md5_text();
	ERR_FAIL_COND_V(cs.length() != 32, ERR_INVALID_PARAMETER);
	Vector<uint8_t> key_md5;
	key_md5.resize(32);
	for (int i = 0; i < 32; i++) {
		key_md5.write[i] = cs[i];
	}

	return open_and_parse(p_base, key_md5, p_mode);
}

Error FileAccessEncryptedCustom::open_internal(const String &p_path, int p_mode_flags) {
	return OK;
}

void FileAccessEncryptedCustom::_close() {
	if (file.is_null()) {
		return;
	}

	file.unref();
}

bool FileAccessEncryptedCustom::is_open() const {
	return file.is_valid();
}

String FileAccessEncryptedCustom::get_path() const {
	if (file.is_valid()) {
		return file->get_path();
	} else {
		return "";
	}
}

String FileAccessEncryptedCustom::get_path_absolute() const {
	if (file.is_valid()) {
		return file->get_path_absolute();
	} else {
		return "";
	}
}

void FileAccessEncryptedCustom::seek(uint64_t p_position) {
	if (p_position > get_length()) {
		p_position = get_length();
	}

	pos = p_position;
	eofed = false;
}

void FileAccessEncryptedCustom::seek_end(int64_t p_position) {
	seek(get_length() + p_position);
}

uint64_t FileAccessEncryptedCustom::get_position() const {
	return pos;
}

uint64_t FileAccessEncryptedCustom::get_length() const {
	return data.size();
}

bool FileAccessEncryptedCustom::eof_reached() const {
	return eofed;
}

uint64_t FileAccessEncryptedCustom::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V_MSG(writing, -1, "File has not been opened in read mode.");

	if (!p_length) {
		return 0;
	}

	ERR_FAIL_NULL_V(p_dst, -1);

	uint64_t to_copy = MIN(p_length, get_length() - pos);

	memcpy(p_dst, data.ptr() + pos, to_copy);
	pos += to_copy;

	if (to_copy < p_length) {
		eofed = true;
	}

	return to_copy;
}

Error FileAccessEncryptedCustom::get_error() const {
	return eofed ? ERR_FILE_EOF : OK;
}

bool FileAccessEncryptedCustom::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_V_MSG(false, "Writing to a custom encrypted file is not supported.");
}

void FileAccessEncryptedCustom::flush() {
	ERR_FAIL_MSG("Writing to a custom encrypted file is not supported.");

	// encrypted files keep data in memory till close()
}

bool FileAccessEncryptedCustom::file_exists(const String &p_name) {
	Ref<FileAccess> fa = FileAccess::open(p_name, FileAccess::READ);
	if (fa.is_null()) {
		return false;
	}
	return true;
}

uint64_t FileAccessEncryptedCustom::_get_modified_time(const String &p_file) {
	if (file.is_valid()) {
		return file->get_modified_time(p_file);
	} else {
		return 0;
	}
}

uint64_t FileAccessEncryptedCustom::_get_access_time(const String &p_file) {
	if (file.is_valid()) {
		return file->get_access_time(p_file);
	} else {
		return 0;
	}
}

int64_t FileAccessEncryptedCustom::_get_size(const String &p_file) {
	if (file.is_valid()) {
		return file->get_size(p_file);
	} else {
		return -1;
	}
}

BitField<FileAccess::UnixPermissionFlags> FileAccessEncryptedCustom::_get_unix_permissions(const String &p_file) {
	if (file.is_valid()) {
		return file->_get_unix_permissions(p_file);
	}
	return 0;
}

Error FileAccessEncryptedCustom::_set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) {
	if (file.is_valid()) {
		return file->_set_unix_permissions(p_file, p_permissions);
	}
	return FAILED;
}

bool FileAccessEncryptedCustom::_get_hidden_attribute(const String &p_file) {
	if (file.is_valid()) {
		return file->_get_hidden_attribute(p_file);
	}
	return false;
}

Error FileAccessEncryptedCustom::_set_hidden_attribute(const String &p_file, bool p_hidden) {
	if (file.is_valid()) {
		return file->_set_hidden_attribute(p_file, p_hidden);
	}
	return FAILED;
}

bool FileAccessEncryptedCustom::_get_read_only_attribute(const String &p_file) {
	if (file.is_valid()) {
		return file->_get_read_only_attribute(p_file);
	}
	return false;
}

Error FileAccessEncryptedCustom::_set_read_only_attribute(const String &p_file, bool p_ro) {
	if (file.is_valid()) {
		return file->_set_read_only_attribute(p_file, p_ro);
	}
	return FAILED;
}

void FileAccessEncryptedCustom::set_decryptor(Ref<CustomDecryptor> p_decryptor) {
	decryptor = p_decryptor;
}

void FileAccessEncryptedCustom::close() {
	_close();
}

Ref<FileAccessEncryptedCustom> FileAccessEncryptedCustom::create(Ref<CustomDecryptor> p_decryptor) {
	Ref<FileAccessEncryptedCustom> fae;
	fae.instantiate();
	fae->set_decryptor(p_decryptor);
	return fae;
}

FileAccessEncryptedCustom::~FileAccessEncryptedCustom() {
	_close();
}

Ref<FileAccess> FileAccessEncryptedCustom::_create_and_parse_custom(Ref<CustomDecryptor> p_decryptor, Ref<FileAccess> p_base, const Vector<uint8_t> &p_key, Mode p_mode, bool p_with_magic, const Vector<uint8_t> &p_iv) {
	Ref<FileAccessEncryptedCustom> fae = create(p_decryptor);
	Error err = fae->open_and_parse(p_base, p_key, p_mode, p_with_magic, p_iv);
	if (err != OK) {
		return nullptr;
	}
	return fae;
}

Ref<FileAccess> FileAccessEncryptedCustom::_create_and_parse_non_custom(Ref<FileAccess> p_base, const Vector<uint8_t> &p_key, Mode p_mode, bool p_with_magic, const Vector<uint8_t> &p_iv) {
	Ref<FileAccessEncrypted> fae = memnew(FileAccessEncrypted);
	Error err = fae->open_and_parse(p_base, p_key, FileAccessEncrypted::Mode(p_mode), p_with_magic, p_iv);
	if (err != OK) {
		return nullptr;
	}
	return fae;
}

void FileAccessEncryptedCustom::_bind_methods() {
	BIND_ENUM_CONSTANT(MODE_READ);
	BIND_ENUM_CONSTANT(MODE_WRITE);
	BIND_ENUM_CONSTANT(MODE_MAX);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("create_and_parse_custom", "custom_decryptor", "base", "key", "mode", "with_magic", "iv"), &FileAccessEncryptedCustom::_create_and_parse_custom, DEFVAL(true), DEFVAL(Vector<uint8_t>()));
	ClassDB::bind_static_method(get_class_static(), D_METHOD("create_and_parse_non_custom", "base", "key", "mode", "with_magic", "iv"), &FileAccessEncryptedCustom::_create_and_parse_non_custom, DEFVAL(true), DEFVAL(Vector<uint8_t>()));
}
