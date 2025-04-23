#include "asset_library_source.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "utility/common.h"
#include "utility/gdre_settings.h"
#include "utility/glob.h"
#include "utility/plugin_manager.h"

HashMap<String, String> AssetLibrarySource::GODOT_VERSION_RELEASE_DATES = {
	{ "2.0", "2016-02-23" },
	{ "2.1", "2016-09-08" },
	{ "3.0", "2018-01-29" },
	{ "3.1", "2019-03-13" },
	{ "3.2", "2020-01-29" },
	{ "3.3", "2021-04-22" },
	{ "3.4", "2021-11-06" },
	{ "3.5", "2022-08-05" },
	{ "3.6", "2024-09-09" },
	{ "4.0", "2023-03-01" },
	{ "4.1", "2023-07-06" },
	{ "4.2", "2023-11-30" },
	{ "4.3", "2024-08-15" },
	{ "4.4", "2025-01-22" },
};

Vector<Dictionary> AssetLibrarySource::search_for_assets(const String &plugin_name, int ver_major) {
	static const Vector<String> _GODOT_VERSIONS = { "2.99", "3.99", "4.99" };
	Vector<String> godot_versions;
	if (ver_major == 0) {
		godot_versions = _GODOT_VERSIONS;
	} else {
		godot_versions.push_back(_GODOT_VERSIONS[ver_major - 2]);
	}

	static const String URL_TEMPLATE = "https://godotengine.org/asset-library/api/asset?type=addon&filter={0}&godot_version={1}&max_results={2}&page={3}";
	Array assets;

	for (const auto &godot_version : godot_versions) {
		int page = 0;
		int pages = 1000;
		for (page = 0; page < pages; page++) {
			String request_url = URL_TEMPLATE.replace("{0}", plugin_name)
										 .replace("{1}", godot_version)
										 .replace("{2}", "500")
										 .replace("{3}", itos(page));
			Vector<uint8_t> response;
			Error err = gdre::wget_sync(request_url, response);
			if (err) {
				break;
			}
			String response_str;
			response_str.append_utf8((const char *)response.ptr(), response.size());
			Dictionary dict = JSON::parse_string(response_str);
			if (dict.is_empty()) {
				break;
			}
			pages = dict.get("pages", {});
			Array page_assets = dict.get("result", {});
			if (page_assets.is_empty()) {
				break;
			}
			assets.append_array(page_assets);
		}
	}
	return gdre::array_to_vector<Dictionary>(assets);
}

Vector<int> AssetLibrarySource::search_for_asset_ids(const String &plugin_name, int ver_major) {
	auto assets = search_for_assets(plugin_name, ver_major);
	Vector<int> asset_ids;
	for (int i = 0; i < assets.size(); i++) {
		Dictionary asset = assets[i];
		if (!asset.has("asset_id")) {
			continue;
		}
		asset_ids.push_back(asset.get("asset_id", 0));
	}
	asset_ids.sort();
	return asset_ids;
}

Vector<Dictionary> AssetLibrarySource::get_list_of_edits(int asset_id) {
	int page = 0;
	int pages = 1000;
	double now = OS::get_singleton()->get_unix_time();
	Vector<Dictionary> edits_vec;

	{
		MutexLock lock(cache_mutex);
		if (edit_list_cache.has(asset_id)) {
			auto &cache = edit_list_cache[asset_id];
			if (!is_cache_expired(cache.retrieved_time)) {
				return cache.edit_list;
			}
		}
	}

	for (page = 0; page < pages; page++) {
		String URL = "https://godotengine.org/asset-library/api/asset/edit?asset={0}&status=accepted&page={1}";
		URL = URL.replace("{0}", itos(asset_id)).replace("{1}", itos(page));

		Vector<uint8_t> response;
		Error err = gdre::wget_sync(URL, response);
		if (err) {
			break;
		}
		String response_str;
		response_str.append_utf8((const char *)response.ptr(), response.size());
		Dictionary response_obj = JSON::parse_string(response_str);
		if (response_obj.is_empty()) {
			break;
		}
		Array edits = response_obj.get("result", {});
		pages = response_obj.get("pages", {});
		if (edits.is_empty()) {
			break;
		}
		for (int i = 0; i < edits.size(); i++) {
			edits_vec.push_back(edits[i]);
		}
	}

	{
		MutexLock lock(cache_mutex);
		edit_list_cache[asset_id] = { now, edits_vec };
	}

	return edits_vec;
}

Dictionary AssetLibrarySource::get_edit(int edit_id) {
	String URL = "https://godotengine.org/asset-library/api/asset/edit/{0}";
	URL = URL.replace("{0}", itos(edit_id));
	Vector<uint8_t> response;
	Error err = gdre::wget_sync(URL, response);
	if (err) {
		return Dictionary();
	}
	String response_str;
	response_str.append_utf8((const char *)response.ptr(), response.size());
	Dictionary response_obj = JSON::parse_string(response_str);
	return response_obj;
}

namespace {
bool is_empty_or_null(const String &str) {
	return str.is_empty() || str == "<null>";
}
} //namespace

bool AssetLibrarySource::init_plugin_version_from_edit(Dictionary edit_list_entry, PluginVersion &version_dict) {
	int edit_id = int(edit_list_entry.get("edit_id", {}));
	int asset_id = int(edit_list_entry.get("asset_id", {}));
	Vector<uint8_t> response;
	Dictionary edit = get_edit(edit_id);
	if (edit.is_empty()) {
		return false;
	}
	String godot_version = edit_list_entry.get("godot_version", "");
	String submit_date = edit_list_entry.get("submit_date", "");
	String plugin_name = edit_list_entry.get("title", "");
	if (!is_empty_or_null(submit_date)) {
		submit_date = submit_date.split(" ")[0];
	}
	String version = edit.get("version_string", "");
	String download_commit = edit.get("download_commit", "");
	if (is_empty_or_null(version) || is_empty_or_null(download_commit)) {
		return false;
	}
	if (!download_commit.begins_with("http")) {
		String download_url = edit.get("download_url", "");
		if (is_empty_or_null(download_url) || !download_url.begins_with("http")) {
			return false;
		}
		download_commit = download_url;
	}
	if (edit.has("godot_version")) {
		String _ver = edit.get("godot_version", "");
		if (!is_empty_or_null(_ver)) {
			godot_version = _ver;
		}
	}
	if (is_empty_or_null(godot_version)) {
		WARN_PRINT("Godot version not found for plugin " + plugin_name + " version: " + version);
	}
	String release_date = GODOT_VERSION_RELEASE_DATES.has(godot_version) ? GODOT_VERSION_RELEASE_DATES.get(godot_version) : "";
	String current_version = godot_version;
	if (is_empty_or_null(release_date)) {
		print_line("Godot version " + godot_version + " not found in release dates");
	} else {
		if (submit_date < release_date) {
			for (auto &E : GODOT_VERSION_RELEASE_DATES) {
				const String &engine_version = E.key;
				const String &date = E.value;
				if (current_version[0] == engine_version[0] && date <= submit_date && current_version >= engine_version) {
					godot_version = engine_version;
				}
			}
		}
	}
	print_line("Got version info for " + plugin_name + " version: " + version + ", min engine: " + godot_version + ", download_url: " + download_commit);
	version_dict.release_id = edit_id;
	version_dict.from_asset_lib = true;
	version_dict.asset_id = asset_id;
	version_dict.version = version;
	version_dict.min_godot_version = godot_version;
	version_dict.release_date = submit_date;
	version_dict.download_url = download_commit;
	return true;
}

PluginVersion AssetLibrarySource::get_plugin_asset_version(int asset_id, const String &version) {
	{
		MutexLock lock(cache_mutex);
		if (asset_lib_cache.has(asset_id) && asset_lib_cache[asset_id].has(version)) {
			auto &out_version = asset_lib_cache[asset_id][version];
			if (out_version.cache_version == CACHE_VERSION) {
				return out_version;
			}
		}
	}
	auto edits = get_list_of_edits(asset_id);
	Dictionary edit;
	PluginVersion plugin_version;

	for (int i = 0; i < edits.size(); i++) {
		edit = edits[i];
		if (edit.get("version_string", "") == version) {
			if (init_plugin_version_from_edit(edit, plugin_version)) {
				break;
			}
		}
	}
	if (plugin_version.asset_id == 0) {
		return PluginVersion();
	}
	if (populate_plugin_version_hashes(plugin_version) != OK) {
		return PluginVersion();
	}
	{
		MutexLock lock(cache_mutex);
		if (!asset_lib_cache.has(asset_id)) {
			asset_lib_cache[asset_id] = {};
		}
		asset_lib_cache[asset_id][version] = plugin_version;
	}

	return plugin_version;
}

PluginVersion AssetLibrarySource::get_plugin_version(const String &plugin_name, const String &version) {
	auto parts = version.split("-", true, 1);
	if (parts.size() != 2) {
		return PluginVersion();
	}
	auto asset_id = parts[0].to_int();
	return get_plugin_asset_version(asset_id, parts[1]);
}

String AssetLibrarySource::get_plugin_download_url(const String &plugin_name, const Vector<String> &hashes) {
	auto asset_ids = search_for_asset_ids(plugin_name);
	for (auto asset_id : asset_ids) {
		auto versions = get_version_strings_for_asset(asset_id);
		for (auto &version : versions) {
			auto plugin_version = get_plugin_asset_version(asset_id, version);
			if (plugin_version.asset_id == 0) {
				continue;
			}
			for (auto &hash : hashes) {
				for (auto &gdext : plugin_version.gdexts) {
					for (auto &bin : gdext.bins) {
						if (bin.md5 == hash) {
							print_line("Detected plugin " + plugin_name + ", version: " + plugin_version.version + ", download url: " + plugin_version.download_url);
							return plugin_version.download_url;
						}
					}
				}
			}
		}
	}
	return "";
}

Vector<String> AssetLibrarySource::get_version_strings_for_asset(int asset_id) {
	Vector<String> versions;
	auto edits = get_list_of_edits(asset_id);
	for (int i = 0; i < edits.size(); i++) {
		String version = edits[i].get("version_string", "");
		if (version.is_empty() || version == "<null>" || versions.has(version)) {
			continue;
		}
		versions.push_back(version);
	}
	return versions;
}

Vector<String> AssetLibrarySource::get_plugin_version_numbers(const String &plugin_name) {
	auto asset_ids = search_for_asset_ids(plugin_name);
	Vector<String> versions;
	for (auto asset_id : asset_ids) {
		auto new_versions = get_version_strings_for_asset(asset_id);
		for (auto &version : new_versions) {
			versions.append(itos(asset_id) + "-" + version);
		}
	}
	return versions;
}

void AssetLibrarySource::load_cache_internal() {
	auto cache_folder = PluginManager::get_plugin_cache_path().path_join("asset_lib");
	auto files = Glob::rglob(cache_folder.path_join("**/*.json"), true);

	MutexLock lock(cache_mutex);
	for (auto &file : files) {
		auto fa = FileAccess::open(file, FileAccess::READ);
		ERR_CONTINUE_MSG(fa.is_null(), "Failed to open file for reading: " + file);
		String json = fa->get_as_text();
		Dictionary d = JSON::parse_string(json);
		load_cache_data(file.get_file(), d);
	}
}

void AssetLibrarySource::save_cache() {
	auto cache_folder = PluginManager::get_plugin_cache_path().path_join("asset_lib");
	ERR_FAIL_COND_MSG(gdre::ensure_dir(cache_folder), "Failed to create cache directory: " + cache_folder);

	MutexLock lock(cache_mutex);
	for (auto &E : asset_lib_cache) {
		auto &asset_id = E.key;
		auto &versions = E.value;
		auto asset_folder = cache_folder.path_join(itos(asset_id));
		ERR_FAIL_COND_MSG(gdre::ensure_dir(asset_folder), "Failed to create directory for asset id: " + itos(asset_id));

		for (auto &V : versions) {
			auto &version = V.key;
			auto &plugin_version = V.value;
			if (plugin_version.cache_version != CACHE_VERSION) {
				continue;
			}
			auto version_file = asset_folder.path_join(version + ".json");
			Dictionary d = plugin_version.to_json();
			String json = JSON::stringify(d, " ", false, true);
			auto fa = FileAccess::open(version_file, FileAccess::WRITE);
			ERR_FAIL_COND_MSG(fa.is_null(), "Failed to open file for writing: " + version_file);
			if (fa.is_valid()) {
				fa->store_string(json);
				fa->close();
			}
		}
	}
}

void AssetLibrarySource::prepop_cache(const Vector<String> &plugin_names, bool multithread) {
	for (const String &plugin_name : plugin_names) {
		auto asset_ids = search_for_asset_ids(plugin_name);
		for (auto asset_id : asset_ids) {
			auto versions = get_version_strings_for_asset(asset_id);
			for (auto &version : versions) {
				get_plugin_asset_version(asset_id, version);
			}
		}
	}
}

bool AssetLibrarySource::handles_plugin(const String &plugin_name) {
	return true;
}

String AssetLibrarySource::get_plugin_name() {
	return "asset_lib";
}

void AssetLibrarySource::load_cache_data(const String &plugin_name, const Dictionary &data) {
	ERR_FAIL_COND_MSG(data.is_empty(), "Failed to parse json string for plugin: " + plugin_name);

	auto version = PluginVersion::from_json(data);
	if (version.cache_version != CACHE_VERSION) {
		return;
	}
	if (!asset_lib_cache.has(version.asset_id)) {
		asset_lib_cache[version.asset_id] = {};
	}
	asset_lib_cache[version.asset_id][version.version] = version;
}
