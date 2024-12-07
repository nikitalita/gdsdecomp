#include "core/io/json.h"
#include "core/os/shared_object.h"
#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "utility/godotver.h"

static constexpr int CACHE_VERSION = 0;
struct PluginBin {
	String name;
	String md5;
	Vector<String> tags;

	Dictionary to_json() const {
		Dictionary d;
		d["name"] = name;
		d["md5"] = md5;
		d["platform"] = tags;
		return d;
	}

	static PluginBin from_json(Dictionary d) {
		PluginBin bin;
		bin.name = d.get("name", "");
		bin.md5 = d.get("md5", "");
		bin.tags = d.get("tags", Vector<String>());
		return bin;
	}
};

struct GDExtInfo {
	String relative_path;
	String min_godot_version;
	String max_godot_version;
	Vector<PluginBin> bins;
	Vector<PluginBin> dependencies;

	Dictionary to_json() const {
		Dictionary d;
		d["relative_path"] = relative_path;
		d["min_godot_version"] = min_godot_version;
		d["max_godot_version"] = max_godot_version;
		Array bins_arr;
		for (const auto &bin : bins) {
			bins_arr.push_back(bin.to_json());
		}
		d["bins"] = bins_arr;
		Array deps_arr;
		for (const auto &dep : dependencies) {
			deps_arr.push_back(dep.to_json());
		}
		d["dependencies"] = deps_arr;
		return d;
	}

	static GDExtInfo from_json(Dictionary d) {
		GDExtInfo info;
		info.relative_path = d.get("relative_path", "");
		info.min_godot_version = d.get("min_godot_version", "");
		info.max_godot_version = d.get("max_godot_version", "");
		Array bins_arr = d.get("bins", {});
		for (int i = 0; i < bins_arr.size(); i++) {
			info.bins.push_back(PluginBin::from_json(bins_arr[i]));
		}
		Array deps_arr = d.get("dependencies", {});
		for (int i = 0; i < deps_arr.size(); i++) {
			info.dependencies.push_back(PluginBin::from_json(deps_arr[i]));
		}
		return info;
	}
};

struct PluginVersion {
	int asset_id = 0;
	int cache_version = CACHE_VERSION;
	String plugin_name;
	String version;
	String min_godot_version;
	String max_godot_version;
	String release_date;
	String download_url;
	String base_folder;
	Vector<GDExtInfo> gdexts;
	bool is_compatible(const Ref<GodotVer> &ver) const {
		auto min_ver = GodotVer::parse(min_godot_version);
		if (ver->get_major() != min_ver->get_major()) {
			return false;
		}
		if (!max_godot_version.is_empty()) {
			return (ver->get_minor() >= min_ver->get_minor() && ver->get_minor() <= GodotVer::parse(max_godot_version)->get_minor());
		}
		return ver->get_minor() >= min_ver->get_minor();
	}

	bool is_compatible(const String &godot_version) const {
		auto ver = GodotVer::parse(godot_version);
		return is_compatible(ver);
	}

	Dictionary to_json() const {
		Dictionary d;
		d["asset_id"] = asset_id;
		d["cache_version"] = cache_version;
		d["plugin_name"] = plugin_name;
		d["version"] = version;
		d["min_godot_version"] = min_godot_version;
		d["max_godot_version"] = max_godot_version;
		d["release_date"] = release_date;
		d["download_url"] = download_url;
		Array gdexts_arr;
		for (const auto &gdext : gdexts) {
			gdexts_arr.push_back(gdext.to_json());
		}
		d["gdexts"] = gdexts_arr;
		return d;
	}

	static PluginVersion from_json(Dictionary d) {
		PluginVersion version;
		version.asset_id = d.get("asset_id", 0);
		version.cache_version = d.get("cache_version", 0);
		version.plugin_name = d.get("plugin_name", "");
		version.version = d.get("version", "");
		version.min_godot_version = d.get("min_godot_version", "");
		version.max_godot_version = d.get("max_godot_version", "");
		version.release_date = d.get("release_date", "");
		version.download_url = d.get("download_url", "");
		Array gdexts_arr = d.get("gdexts", {});
		for (int i = 0; i < gdexts_arr.size(); i++) {
			version.gdexts.push_back(GDExtInfo::from_json(gdexts_arr[i]));
		}
		return version;
	}
};

struct PluginInfo {
	String name;
	Vector<PluginVersion> versions;
};

struct EditListCache {
	double retrieved_time = 0;
	Vector<Dictionary> edit_list;
};

class AssetLibInfoGetter {
	static HashMap<int, HashMap<String, PluginVersion>> cache; // asset_id -> version -> PluginVersion
	static HashMap<int, EditListCache> temp_edit_list_cache;
	static Mutex cache_mutex;

	static PluginBin get_plugin_bin(const String &oath, const SharedObject &obj);
	static bool init_plugin_version_from_edit(Dictionary edit_list_entry, PluginVersion &version);
	static Error populate_plugin_version_hashes(PluginVersion &version);
	static PluginVersion get_plugin_version(int asset_id, const String &version);

public:
	static String get_asset_lib_cache_folder();
	static Array search_for_assets(const String &plugin_name, int ver_major = 0);
	static Vector<int> search_for_asset_ids(const String &plugin_name, int ver_major = 0);
	static Vector<Dictionary> get_assets_for_plugin(const String &plugin_name);
	static Vector<Dictionary> get_list_of_edits(int asset_id);
	static Dictionary get_edit(int edit_id);
	static Vector<PluginVersion> get_all_plugin_versions_slow(const String &plugin_name, int asset_id);
	static Vector<String> get_plugin_version_numbers(const String &plugin_name, int asset_id);
	static String get_plugin_download_url(const String &plugin_name, const Vector<String> hashes);
	static void load_cache();
	static void save_cache();
};