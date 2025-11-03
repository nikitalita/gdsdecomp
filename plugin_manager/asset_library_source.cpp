#include "asset_library_source.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "plugin_manager.h"
#include "utility/common.h"

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
	{ "4.5", "2025-09-15" },
};

Error AssetLibrarySource::search_for_assets(const String &plugin_name, Vector<Dictionary> &r_assets, int ver_major) {
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
			ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to get assets ids for plugin " + plugin_name + " godot version " + godot_version);
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
	r_assets = gdre::array_to_vector<Dictionary>(assets);
	return OK;
}

Error AssetLibrarySource::search_for_asset_ids(const String &plugin_name, Vector<int64_t> &r_asset_ids, int ver_major) {
	Vector<Dictionary> assets;
	Error err = search_for_assets(plugin_name, assets, ver_major);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to search for assets for plugin " + plugin_name);
	r_asset_ids.clear();
	for (int i = 0; i < assets.size(); i++) {
		Dictionary asset = assets[i];
		if (!asset.has("asset_id")) {
			continue;
		}
		r_asset_ids.push_back(asset.get("asset_id", 0));
	}
	r_asset_ids.sort();
	return OK;
}

Error AssetLibrarySource::get_assets_for_plugin(const String &plugin_name, Vector<Dictionary> &r_assets) {
	return search_for_assets(plugin_name, r_assets, 0);
}

Error AssetLibrarySource::get_edit_list(int64_t asset_id, Vector<Dictionary> &r_edits) {
	int page = 0;
	int pages = 1000;
	double now = OS::get_singleton()->get_unix_time();
	Vector<Dictionary> edits_vec;

	{
		MutexLock lock(cache_mutex);
		if (edit_list_cache.has(asset_id)) {
			auto &cache = edit_list_cache[asset_id];
			if (!is_cache_expired(cache.retrieved_time)) {
				r_edits = cache.edit_list;
				return OK;
			}
		}
	}

	static const String URL_TEMPLATE = "https://godotengine.org/asset-library/api/asset/edit?asset={0}&status=accepted&page={1}";
	for (page = 0; page < pages; page++) {
		String URL = URL_TEMPLATE.replace("{0}", itos(asset_id)).replace("{1}", itos(page));

		Vector<uint8_t> response;
		Error err = gdre::wget_sync(URL, response);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to get edit list for asset " + itos(asset_id));
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
		edit_list_cache[asset_id] = { now, (int64_t)asset_id, edits_vec };
	}

	r_edits = edits_vec;
	return OK;
}

Error AssetLibrarySource::get_edit(int64_t edit_id, Dictionary &r_edit) {
	{
		constexpr time_t EDIT_EXPIRY_TIME = 24 * 3600; // 1 day in seconds
		MutexLock lock(cache_mutex);
		if (edit_cache.has(edit_id)) {
			auto &cache = edit_cache[edit_id];
			if (!cache.retrieved_time + EDIT_EXPIRY_TIME <= OS::get_singleton()->get_unix_time()) {
				r_edit = cache.edit;
				return OK;
			}
		}
	}

	String URL = "https://godotengine.org/asset-library/api/asset/edit/{0}";
	URL = URL.replace("{0}", itos(edit_id));
	Vector<uint8_t> response;
	Error err = gdre::wget_sync(URL, response);
	if (err || response.size() == 0) {
		return err;
	}
	String response_str;
	response_str.append_utf8((const char *)response.ptr(), response.size());
	Dictionary response_obj = JSON::parse_string(response_str);
	if (response_obj.is_empty()) {
		return ERR_PARSE_ERROR;
	}

	r_edit = response_obj;
	{
		MutexLock lock(cache_mutex);
		edit_cache[edit_id] = {
			OS::get_singleton()->get_unix_time(),
			(int64_t)edit_id,
			r_edit
		};
	}

	return OK;
}

namespace {
bool is_empty_or_null(const String &str) {
	return str.is_empty() || str == "<null>";
}
} //namespace

ReleaseInfo AssetLibrarySource::get_release_info(const String &plugin_name, int64_t primary_id, int64_t secondary_id) {
	auto asset_id = primary_id;
	auto edit_id = secondary_id;
	if (asset_id <= 0 || edit_id <= 0) {
		return ReleaseInfo();
	}

	Vector<Dictionary> edit_list;
	Error err = get_edit_list(asset_id, edit_list);
	ERR_FAIL_COND_V_MSG(err != OK, ReleaseInfo(), "Failed to get edit list for asset " + itos(asset_id));
	for (const Dictionary &edit_list_entry : edit_list) {
		if (int64_t(edit_list_entry.get("edit_id", {})) == edit_id) {
			Dictionary edit_data;
			err = get_edit(edit_id, edit_data);
			ERR_CONTINUE_MSG(err != OK, vformat("Failed to get edit %d for asset %d", edit_id, asset_id));
			if (edit_data.is_empty()) {
				continue;
			}

			String godot_version = edit_list_entry.get("godot_version", "");
			String submit_date = edit_list_entry.get("submit_date", "");
			String plugin_name_from_edit = edit_list_entry.get("title", "");
			if (!is_empty_or_null(submit_date)) {
				submit_date = submit_date.split(" ")[0];
			}
			String version = edit_data.get("version_string", "");
			String download_commit = edit_data.get("download_commit", "");
			if (is_empty_or_null(version) || is_empty_or_null(download_commit)) {
				break;
			}
			if (!download_commit.begins_with("http")) {
				String download_url = edit_data.get("download_url", "");
				if (is_empty_or_null(download_url) || !download_url.begins_with("http")) {
					break;
				}
				download_commit = download_url;
			}
			if (edit_data.has("godot_version")) {
				String _ver = edit_data.get("godot_version", "");
				if (!is_empty_or_null(_ver)) {
					godot_version = _ver;
				}
			}
			if (is_empty_or_null(godot_version)) {
				WARN_PRINT("Godot version not found for plugin " + plugin_name_from_edit + " version: " + version);
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

			ReleaseInfo release_info;
			release_info.plugin_source = get_plugin_name();
			release_info.primary_id = asset_id;
			release_info.secondary_id = edit_id;
			release_info.version = version;
			release_info.release_date = submit_date;
			release_info.download_url = download_commit;
			release_info.repository_url = "https://godotengine.org/asset-library/asset/" + itos(asset_id);

			return release_info;
		}
	}

	return ReleaseInfo();
}

Error AssetLibrarySource::get_valid_edit_ids_for_plugin(int64_t asset_id, Vector<int64_t> &r_versions) {
	Vector<Dictionary> edits;
	Error err = get_edit_list(asset_id, edits);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to get edit list for asset " + itos(asset_id));
	r_versions.clear();
	HashSet<int64_t> seen_ids;
	for (int i = 0; i < edits.size(); i++) {
		String version = edits[i].get("version_string", "");
		if (version.is_empty() || version == "<null>") {
			continue;
		}
		int64_t edit_id = int64_t(edits[i].get("edit_id", {}));
		if (seen_ids.has(edit_id) || edit_id == 0) {
			continue;
		}
		seen_ids.insert(edit_id);
		r_versions.push_back(edit_id);
	}
	return OK;
}

Vector<Pair<int64_t, int64_t>> AssetLibrarySource::get_plugin_version_numbers(const String &plugin_name) {
	Vector<int64_t> asset_ids;
	Error err = search_for_asset_ids(plugin_name, asset_ids);
	ERR_FAIL_COND_V_MSG(err != OK, {}, "Failed to search for asset IDs for plugin " + plugin_name);
	Vector<Pair<int64_t, int64_t>> versions;
	for (auto asset_id : asset_ids) {
		Vector<int64_t> edit_ids;
		err = get_valid_edit_ids_for_plugin(asset_id, edit_ids);
		ERR_FAIL_COND_V_MSG(err != OK, {}, "Failed to get valid edit IDs for asset " + itos(asset_id));
		for (auto &edit_id : edit_ids) {
			versions.push_back({ asset_id, edit_id });
		}
	}
	return versions;
}

Error AssetLibrarySource::load_edit_list_cache() {
	MutexLock lock(cache_mutex);
	String edit_list_cache_file = PluginManager::get_plugin_cache_path().path_join("asset_lib_edit_list_release_cache.json");
	if (!FileAccess::exists(edit_list_cache_file)) {
		return OK;
	}
	auto file = FileAccess::open(edit_list_cache_file, FileAccess::READ);
	if (file.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}
	Dictionary json = JSON::parse_string(file->get_as_text());
	for (auto &E : json) {
		edit_list_cache[E.key] = EditListCache::from_json(E.value);
	}
	file->close();
	return OK;
}

void AssetLibrarySource::load_edit_cache() {
	MutexLock lock(cache_mutex);
	String edit_cache_file = PluginManager::get_plugin_cache_path().path_join("asset_lib_edits_release_cache.json");
	if (!FileAccess::exists(edit_cache_file)) {
		return;
	}
	auto file = FileAccess::open(edit_cache_file, FileAccess::READ);
	if (file.is_null()) {
		return;
	}
	Dictionary json = JSON::parse_string(file->get_as_text());
	for (auto &E : json) {
		edit_cache[E.key] = EditCache::from_json(E.value);
	}
	file->close();
}

void AssetLibrarySource::load_cache_internal() {
	Error err = load_edit_list_cache();
	if (err != OK) {
		ERR_PRINT("Failed to load edit list cache");
		return;
	}
	load_edit_cache();
}

void AssetLibrarySource::save_edit_list_cache() {
	MutexLock lock(cache_mutex);
	String edit_list_cache_file = PluginManager::get_plugin_cache_path().path_join("asset_lib_edit_list_release_cache.json");
	gdre::ensure_dir(edit_list_cache_file.get_base_dir());
	auto file = FileAccess::open(edit_list_cache_file, FileAccess::WRITE);
	if (file.is_null()) {
		return;
	}
	Dictionary json;
	for (auto &E : edit_list_cache) {
		json[E.key] = E.value.to_json();
	}
	file->store_string(JSON::stringify(json));
	file->close();
	json.clear();
}

void AssetLibrarySource::save_edit_cache() {
	MutexLock lock(cache_mutex);
	String edit_cache_file = PluginManager::get_plugin_cache_path().path_join("asset_lib_edits_release_cache.json");
	gdre::ensure_dir(edit_cache_file.get_base_dir());
	auto file = FileAccess::open(edit_cache_file, FileAccess::WRITE);
	if (file.is_null()) {
		return;
	}

	Dictionary json;
	for (auto &E : edit_cache) {
		json[E.key] = E.value.to_json();
	}
	file->store_string(JSON::stringify(json));
	file->close();
	json.clear();
}

void AssetLibrarySource::save_cache() {
	save_edit_list_cache();
	save_edit_cache();
}

bool AssetLibrarySource::handles_plugin(const String &plugin_name) {
	return true;
}

String AssetLibrarySource::get_plugin_name() {
	return "asset_lib";
}

EditListCache EditListCache::from_json(const Dictionary &json) {
	EditListCache cache;
	cache.retrieved_time = json.get("retrieved_time", 0);
	cache.asset_id = json.get("asset_id", 0);
	cache.edit_list = gdre::array_to_vector<Dictionary>(json.get("edit_list", Array()));
	return cache;
}

Dictionary EditListCache::to_json() const {
	return {
		{ "retrieved_time", retrieved_time },
		{ "asset_id", asset_id },
		{ "edit_list", gdre::vector_to_array(edit_list) }
	};
}

EditCache EditCache::from_json(const Dictionary &json) {
	EditCache cache;
	cache.retrieved_time = json.get("retrieved_time", 0);
	cache.edit_id = json.get("edit_id", 0);
	cache.edit = json.get("edit", Dictionary());
	return cache;
}

Dictionary EditCache::to_json() const {
	return {
		{ "retrieved_time", retrieved_time },
		{ "edit_id", edit_id },
		{ "edit", edit }
	};
}

Vector<ReleaseInfo> AssetLibrarySource::find_release_infos_by_tag(const String &plugin_name, const String &tag) {
	Vector<int64_t> asset_ids;
	Error err = search_for_asset_ids(plugin_name, asset_ids);
	ERR_FAIL_COND_V_MSG(err != OK, Vector<ReleaseInfo>(), "Failed to search for asset IDs for plugin " + plugin_name);
	Vector<ReleaseInfo> release_infos;
	Vector<int64_t> edit_ids;
	for (auto asset_id : asset_ids) {
		Vector<Dictionary> edits;
		err = get_edit_list(asset_id, edits);
		ERR_CONTINUE_MSG(err != OK, vformat("Failed to get edit list for asset %d", asset_id));
		for (int i = 0; i < edits.size(); i++) {
			String version = edits[i].get("version_string", "");
			if (version == tag) {
				int64_t edit_id = int64_t(edits[i].get("edit_id", 0));
				if (edit_ids.has(edit_id) || edit_id == 0) {
					continue;
				}
				edit_ids.push_back(edit_id);
				auto rel_info = get_release_info(plugin_name, asset_id, edit_id);
				if (rel_info.is_valid()) {
					release_infos.push_back(rel_info);
				}
			}
		}
	}
	return release_infos;
}
