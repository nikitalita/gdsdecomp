#pragma once

#include "core/io/file_access_pack.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"

static constexpr uint8_t MD5_EMPTY[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
class PackedFileInfo : public RefCounted {
	GDCLASS(PackedFileInfo, RefCounted);
	friend class GDRESettings;
	friend class PckDumper;
	friend class GDREPackedSource;
	friend class APKArchive;
	friend class GDREFolderSource;

	String path;
	String raw_path;
	PackedData::PackedFile pf;
	bool malformed_path = false;
	bool md5_passed = false;
	bool dummy_path = false;

	void set_md5_match(bool pass) { md5_passed = pass; }

public:
	void init(const String &p_path, const PackedData::PackedFile *pfstruct, bool p_is_dummy = false) {
		pf = *pfstruct;
		raw_path = p_path;
		malformed_path = false;
		dummy_path = p_is_dummy;
		fix_path();
	}
	String get_pack() const { return pf.pack; }
	String get_path() const { return path; }
	String get_raw_path() const { return raw_path; }
	uint64_t get_offset() const { return pf.offset; }
	uint64_t get_size() const { return pf.size; }
	Vector<uint8_t> get_md5() const {
		Vector<uint8_t> ret;
		ret.resize(16);
		memcpy(ret.ptrw(), pf.md5, 16);
		return ret;
	}
	bool has_md5() const {
		// check if all 16 bytes are 0
		return memcmp(pf.md5, MD5_EMPTY, 16) != 0;
	}

	bool is_malformed() const {
		return malformed_path;
	}
	bool is_encrypted() const {
		return pf.encrypted;
	}
	bool is_checksum_validated() const {
		return md5_passed;
	}
	bool is_dummy() const {
		return dummy_path;
	}

protected:
	static void _bind_methods();

private:
	void fix_path();
};
