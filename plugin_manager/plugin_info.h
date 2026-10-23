#pragma once

#include "core/variant/variant.h"
#include "utility/godotver.h"

static constexpr int CACHE_VERSION = 7;

struct PluginBin {
	String name;
	Vector<String> tags;
	bool exists = false;
	String sha256;
	String verbatim_sha256;
	String bin_ver;

	Dictionary to_json() const;
	static PluginBin from_json(Dictionary d);
};

struct GDExtInfo {
	String relative_path;
	String min_godot_version;
	String max_godot_version;
	Vector<PluginBin> bins;
	Vector<PluginBin> dependencies;

	Dictionary to_json() const;
	static GDExtInfo from_json(Dictionary d);
};

struct ReleaseInfo {
	String plugin_source;
	int64_t primary_id = 0; // assetlib asset id or github release id
	int64_t secondary_id = 0; // assetlib edit_id or github asset id
	String version;
	String release_date;
	String download_url;
	String repository_url;
	String sha256_sum;

	bool is_valid() const;

	Dictionary to_json() const;
	static ReleaseInfo from_json(Dictionary d);

	bool operator==(const ReleaseInfo &other) const;
	bool operator!=(const ReleaseInfo &other) const;

	String get_cache_key() const;
	String get_download_file() const;
};

struct PluginVersion {
	int cache_version = CACHE_VERSION;
	String plugin_name;
	ReleaseInfo release_info;
	String min_godot_version;
	String max_godot_version;
	String base_folder;
	int64_t size = 0;
	String archive_sha256;
	Vector<GDExtInfo> gdexts;
	// non-serialized field
	bool is_static_cached = false;

	static PluginVersion invalid();

	bool is_valid() const;

	bool is_compatible(const Ref<GodotVer> &ver) const;
	bool is_compatible(const String &godot_version) const;

	bool bin_hashes_match(const Vector<String> &hashes, bool deep = false) const;
	bool dep_bin_hashes_match(const Vector<String> &hashes, bool deep = false) const;

	Dictionary to_json() const;
	static PluginVersion from_json(Dictionary d);
};
