//_PRELUDE_
// clang-format off
#pragma once

#include "bytecode/bytecode_base.h"

//_BYTECODE_HEADERS_

void register_decomp_versions();
GDScriptDecomp *create_decomp_for_commit(uint64_t p_commit_hash);
Vector<Ref<GDScriptDecomp>> get_decomps_for_bytecode_ver(int bytecode_version, bool include_dev = false);
struct GDScriptDecompVersion {
	static Vector<GDScriptDecompVersion> decomp_versions;
	uint64_t commit;
	String name;
	int bytecode_version;
	bool is_dev;
	String min_version;
	String max_version;
	int parent;
	Dictionary custom;

	bool is_custom() const {
		return custom.size() > 0;
	}

	Ref<GodotVer> get_min_version() const {
		return GodotVer::parse(min_version);
	}
	Ref<GodotVer> get_max_version() const {
		return GodotVer::parse(max_version);
	}
};
Vector<GDScriptDecompVersion> get_decomp_versions(bool include_dev = true, int ver_major = 0);

static constexpr int LATEST_GDSCRIPT_COMMIT = /*_BYTECODE_DECOMP_LATEST_VERSION_*/;
