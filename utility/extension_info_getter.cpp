#include "extension_info_getter.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/dir_access.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"
#include "import_info.h"
#include "modules/zip/zip_reader.h"
#include "utility/common.h"
#include "utility/gdre_settings.h"
#include "utility/glob.h"
#include <cstdint>

namespace {
HashMap<String, String> init_map(const char *arr[][2], int count) {
	HashMap<String, String> map;
	for (int i = 0; i < count; i++) {
		map[arr[i][0]] = arr[i][1];
	}
	return map;
}
bool is_empty_or_null(const String &str) {
	return str.is_empty() || str == "<null>";
}
} //namespace

HashMap<uint64_t, HashMap<String, PluginVersion>> AssetLibInfoGetter::asset_lib_cache = {};
HashMap<uint64_t, EditListCache> AssetLibInfoGetter::temp_edit_list_cache = {};
HashMap<String, GHReleaseListCache> AssetLibInfoGetter::temp_gh_release_list_cache = {};
HashMap<String, HashMap<uint64_t, HashMap<uint64_t, PluginVersion>>> AssetLibInfoGetter::non_asset_lib_cache = {};

Mutex AssetLibInfoGetter::cache_mutex = {};
static String github_release_api_url = "https://api.github.com/repos/{0}/{1}/releases?per_page=100&page={2}";
static const char *non_asset_lib_plugin_repos[][2] = {
	{ "godotsteam", "https://github.com/GodotSteam/GodotSteam" },
};
static constexpr size_t non_asset_lib_plugin_repos_count = sizeof(non_asset_lib_plugin_repos) / sizeof(non_asset_lib_plugin_repos[0]);
static const char *_GODOT_VERSION_RELEASE_DATES[][2] = {
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
};
static constexpr int GODOT_VERSION_RELEASE_DATES_COUNT = sizeof(_GODOT_VERSION_RELEASE_DATES) / sizeof(_GODOT_VERSION_RELEASE_DATES[0]);
HashMap<String, String> AssetLibInfoGetter::non_asset_lib_plugins = init_map(non_asset_lib_plugin_repos, non_asset_lib_plugin_repos_count);
HashMap<String, String> AssetLibInfoGetter::GODOT_VERSION_RELEASE_DATES = init_map(_GODOT_VERSION_RELEASE_DATES, GODOT_VERSION_RELEASE_DATES_COUNT);

Array AssetLibInfoGetter::search_for_assets(const String &plugin_name, int ver_major) {
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
			String request_url = URL_TEMPLATE.replace("{0}", plugin_name).replace("{1}", godot_version).replace("{2}", "500").replace("{3}", itos(page));
			Vector<uint8_t> response;
			Error err = gdre::wget_sync(request_url, response);
			if (err) {
				break;
			}
			String response_str;
			response_str.parse_utf8((const char *)response.ptr(), response.size());
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
	return assets;
}

Vector<int> AssetLibInfoGetter::search_for_asset_ids(const String &plugin_name, int ver_major) {
	Array assets = search_for_assets(plugin_name, ver_major);
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

Vector<Dictionary> AssetLibInfoGetter::get_assets_for_plugin(const String &plugin_name) {
	Array assets = search_for_assets(plugin_name);
	Vector<int> asset_ids;
	for (int i = 0; i < assets.size(); i++) {
		Dictionary asset = assets[i];
		asset_ids.push_back(asset.get("asset_id", {}));
	}
	asset_ids.sort();
	Vector<Dictionary> new_assets;
	for (int i = 0; i < asset_ids.size(); i++) {
		int asset_id = asset_ids[i];
		String request_url = "https://godotengine.org/asset-library/api/asset/" + itos(asset_id);
		Vector<uint8_t> response;
		Error err = gdre::wget_sync(request_url, response);
		if (err) {
			continue;
		}
		String response_str;
		response_str.parse_utf8((const char *)response.ptr(), response.size());
		Dictionary asset = JSON::parse_string(response_str);
		new_assets.append(asset);
	}
	return new_assets;
}

struct EditsSorter {
	bool operator()(const Dictionary &a, const Dictionary &b) const {
		return int(a.get("edit_id", 0)) < int(b.get("edit_id", 0));
	}
};
struct EditsSorterReverse {
	bool operator()(const Dictionary &a, const Dictionary &b) const {
		return int(a.get("edit_id", 0)) > int(b.get("edit_id", 0));
	}
};

// 1 hour in seconds
static constexpr time_t EXPIRY_TIME = 3600;
Vector<Dictionary> AssetLibInfoGetter::get_list_of_edits(int asset_id) {
	int page = 0;
	int pages = 1000;
	// get the current time
	double now = OS::get_singleton()->get_unix_time();
	Vector<Dictionary> edits_vec;
	{
		MutexLock lock(cache_mutex);
		if (temp_edit_list_cache.has(asset_id)) {
			auto &cache = temp_edit_list_cache[asset_id];
			if (cache.retrieved_time + EXPIRY_TIME > now) {
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
		response_str.parse_utf8((const char *)response.ptr(), response.size());
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
	edits_vec.sort_custom<EditsSorterReverse>();
	{
		MutexLock lock(cache_mutex);
		temp_edit_list_cache[asset_id] = { .retrieved_time = now, .edit_list = edits_vec };
	}
	return edits_vec;
}

PluginBin AssetLibInfoGetter::get_plugin_bin(const String &path, const SharedObject &obj) {
	PluginBin bin;
	bin.name = obj.path;
	bin.md5 = gdre::get_md5(path, true);
	bin.tags = obj.tags;
	return bin;
}

PluginVersion AssetLibInfoGetter::get_plugin_version(int asset_id, const String &version) {
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

Error AssetLibInfoGetter::populate_plugin_version_hashes(PluginVersion &plugin_version) {
	auto temp_folder = GDRESettings::get_singleton()->get_gdre_user_path().path_join(".tmp").path_join(itos(plugin_version.asset_id));
	String url = plugin_version.download_url;
	String new_temp_foldr = temp_folder.path_join(itos(plugin_version.release_id));
	String zip_path = new_temp_foldr.path_join("plugin.zip");
	Error err = gdre::download_file_sync(url, zip_path);
	if (err) {
		return err;
	}
	Ref<ZIPReader> zip;
	zip.instantiate();
	err = zip->open(zip_path);
	if (err) {
		return err;
	}
	auto files = zip->get_files();
	String gd_ext_file = "";
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	auto close_and_remove_zip = [&]() {
		zip->close();
		da->remove(zip_path);
	};
	// get all the gdexts
	HashMap<String, GDExtInfo> gdexts;
	for (int i = 0; i < files.size(); i++) {
		auto ipath = files[i];
		auto ext = files[i].get_extension().to_lower();
		if (ext == "gdextension" || ext == "gdnlib") {
			// get the path relative to the "addons" folder
			auto idx = ipath.to_lower().find("addons");
			if (idx == -1) {
				// just get one path up from the file
				// e.g. 3523231532315/bin/something.gdnative would be bin/something.gdnative
				idx = ipath.get_base_dir().rfind("/") + 1;
				if (plugin_version.plugin_name.is_empty()) {
					plugin_version.plugin_name = ipath.get_file().get_basename();
				}
			} else {
				if (plugin_version.plugin_name.is_empty()) {
					plugin_version.plugin_name = ipath.substr(idx).simplify_path().get_slice("/", 1);
				}
			}
			auto rel_path = ipath.substr(idx);
			GDExtInfo gdext_info;
			gdext_info.relative_path = rel_path;
			if (plugin_version.base_folder.is_empty() && idx > 0) {
				plugin_version.base_folder = ipath.substr(0, idx);
			}

			gdexts[files[i]] = gdext_info;
		}
	}
	if (gdexts.size() == 0) {
		close_and_remove_zip();
		return OK;
	}
	print_line("Populating plugin version hashes for " + plugin_version.plugin_name + " version: " + plugin_version.version);
	String unzupped_path = new_temp_foldr.path_join("unzipped");
	err = gdre::unzip_file_to_dir(zip_path, unzupped_path);
	if (err) {
		close_and_remove_zip();
		return err;
	}
	bool first_min = true;
	bool first_max = true;
	for (auto &E : gdexts) {
		GDExtInfo &gdext_info = E.value;
		auto &gdext_path = E.key;
		auto data = zip->read_file(gdext_path, true);
		String gdext_str;
		gdext_str.parse_utf8((const char *)data.ptr(), data.size());
		Ref<ImportInfoGDExt> cf = memnew(ImportInfoGDExt);
		cf->load_from_string("res://" + gdext_info.relative_path, gdext_str);
		if (!cf->get_compatibility_minimum().is_empty()) {
			auto min = cf->get_compatibility_minimum();
			if (first_min || plugin_version.min_godot_version < min) {
				plugin_version.min_godot_version = min;
				first_min = false;
			}
			gdext_info.min_godot_version = min;
		}
		if (!cf->get_compatibility_maximum().is_empty()) {
			auto max = cf->get_compatibility_maximum();
			if (first_max || plugin_version.max_godot_version > max) {
				plugin_version.max_godot_version = max;
				first_max = false;
			}
			gdext_info.max_godot_version = max;
		}
		auto parse_bins = [unzupped_path](Vector<SharedObject> bins) {
			Vector<PluginBin> plugin_bins;
			for (auto &E : bins) {
				auto &lib = E.path;
				// find the path in the unzipped folder
				auto paths = Glob::rglob(unzupped_path.path_join("**").path_join(lib.replace_first("res://", "")), true);
				String real_path;
				for (auto &p : paths) {
					if (p.ends_with(lib.replace_first("res://", ""))) {
						real_path = p;
						break;
					}
				}
				auto plugin_bin = get_plugin_bin(real_path, E);
				plugin_bins.push_back(plugin_bin);
			}
			return plugin_bins;
		};
		gdext_info.bins = parse_bins(cf->get_libaries(false));
		gdext_info.dependencies = parse_bins(cf->get_dependencies(false));
		plugin_version.gdexts.push_back(gdext_info);
	}
	close_and_remove_zip();
	da->change_dir(unzupped_path);
	da->erase_contents_recursive();
	return OK;
}

Dictionary AssetLibInfoGetter::get_edit(int edit_id) {
	String URL = "https://godotengine.org/asset-library/api/asset/edit/{0}";
	URL = URL.replace("{0}", itos(edit_id));
	Vector<uint8_t> response;
	Error err = gdre::wget_sync(URL, response);
	if (err) {
		return Dictionary();
	}
	String response_str;
	response_str.parse_utf8((const char *)response.ptr(), response.size());
	Dictionary response_obj = JSON::parse_string(response_str);
	return response_obj;
}

bool AssetLibInfoGetter::init_plugin_version_from_edit(Dictionary edit_list_entry, PluginVersion &version_dict) {
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

/***
 * edit list api: https://godotengine.org/asset-library/api/asset/edit?asset={ASSET_ID}&status=accepted
 * edit list is like this:
 * ```json
 * [
 *         {
 *             "edit_id": "14471",
 *             "asset_id": "1918",
 *             "user_id": "8098",
 *             "submit_date": "2024-11-03 17:51:55",
 *             "modify_date": "2024-11-04 10:58:07",
 *             "title": "Godot Jolt",
 *             "description": "Godot Jolt is a native extension that allows you to use the Jolt physics engine to power Godot's 3D physics.\r\n\r\nIt functions as a drop-in replacement for Godot Physics, by implementing the same nodes that you would use normally, like RigidBody3D or CharacterBody3D.\r\n\r\nThis version of Godot Jolt only supports Godot 4.3 (including 4.3.x) and only support Windows, Linux, macOS, iOS and Android.\r\n\r\nOnce the extension is extracted in your project folder, you need to go through the following steps to switch physics engine:\r\n\r\n1. Restart Godot\r\n2. Open your project settings\r\n3. Make sure \"Advanced Settings\" is enabled\r\n4. Go to \"Physics\" and then \"3D\"\r\n5. Change \"Physics Engine\" to \"JoltPhysics3D\"\r\n6. Restart Godot\r\n\r\nFor more details visit: github.com\/godot-jolt\/godot-jolt\r\nFor more details about Jolt itself visit: github.com\/jrouwe\/JoltPhysics",
 *             "godot_version": "4.3",
 *             "version_string": "0.14.0",
 *             "cost": "MIT",
 *             "browse_url": "https:\/\/github.com\/godot-jolt\/godot-jolt",
 *             "icon_url": "https:\/\/github.com\/godot-jolt\/godot-asset-library\/releases\/download\/v0.14.0-stable\/godot-jolt_icon.png",
 *             "category": null,
 *             "support_level": "community",
 *             "status": "accepted",
 *             "reason": "",
 *             "author": "mihe"
 *         },
 *         ...
 * ]
 * ```json
 */

Vector<String> AssetLibInfoGetter::get_plugin_version_numbers(const String &plugin_name, int asset_id) {
	auto edits = get_list_of_edits(asset_id);
	Vector<String> versions;
	if (edits.size() == 0) {
		return versions;
	}
	for (int i = 0; i < edits.size(); i++) {
		String version = edits[i].get("version_string", "");
		if (is_empty_or_null(version) || versions.has(version)) {
			continue;
		}
		versions.push_back(version);
	}
	return versions;
}

void AssetLibInfoGetter::prepop_plugin_cache(const String &plugin_name) {
	if (non_asset_lib_plugins.has(plugin_name)) {
		prepop_plugin_cache_gh(plugin_name);
		return;
	}
	auto asset_ids = search_for_asset_ids(plugin_name);
	for (auto &asset_id : asset_ids) {
		auto versions = get_plugin_version_numbers(plugin_name, asset_id);
		for (auto &E : versions) {
			get_plugin_version(asset_id, E);
		}
	}
}

String AssetLibInfoGetter::get_plugin_download_url(const String &plugin_name, const Vector<String> hashes) {
	if (non_asset_lib_plugins.has(plugin_name)) {
		return get_plugin_download_url_non_asset_lib(plugin_name, hashes);
	}

	auto ids = search_for_asset_ids(plugin_name);
	if (ids.size() == 0) {
		return "";
	}
	for (int i = 0; i < ids.size(); i++) {
		int asset_id = ids[i];
		auto versions_keys = get_plugin_version_numbers(plugin_name, asset_id);
		for (auto &version : versions_keys) {
			auto plugin_version = get_plugin_version(asset_id, version);
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

String AssetLibInfoGetter::get_asset_lib_cache_folder() {
	return GDRESettings::get_singleton()->get_gdre_user_path().path_join("plugin_cache").path_join("asset_lib");
}

void AssetLibInfoGetter::save_cache() {
	// cache will be saved to the user data folder.
	// folder will be <USER_DATA>/plugin_cache/asset_lib
	// the files will be saved to <USER_DATA>/plugin_cache/asset_lib/<ASSET_ID>.json
	// the file will be a json file with the plugin version data
	auto cache_folder = get_asset_lib_cache_folder();
	ERR_FAIL_COND_MSG(gdre::ensure_dir(cache_folder), "Failed to create cache directory: " + cache_folder);
	{
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
	save_non_asset_lib_cache();
}

void AssetLibInfoGetter::load_cache() {
	// load the cache
	auto cache_folder = get_asset_lib_cache_folder();
	auto files = Glob::rglob(cache_folder.path_join("**/*.json"), true);
	{
		MutexLock lock(cache_mutex);
		for (auto &file : files) {
			auto fa = FileAccess::open(file, FileAccess::READ);
			ERR_CONTINUE_MSG(fa.is_null(), "Failed to open file for reading: " + file);
			String json = fa->get_as_text();
			Dictionary d = JSON::parse_string(json);
			auto version = PluginVersion::from_json(d);
			if (version.cache_version != CACHE_VERSION) {
				continue;
			}
			if (!asset_lib_cache.has(version.asset_id)) {
				asset_lib_cache[version.asset_id] = {};
			}
			asset_lib_cache[version.asset_id][version.version] = version;
		}
	}
	load_non_asset_lib_cache();
}

String AssetLibInfoGetter::get_non_asset_lib_cache_folder() {
	return GDRESettings::get_singleton()->get_gdre_user_path().path_join("plugin_cache").path_join("non_asset_lib");
}

Dictionary AssetLibInfoGetter::get_gh_release_dict(const String &plugin_name, uint64_t release_id) {
	if (!recache_gh_release_list(plugin_name)) {
		return Dictionary();
	}
	{
		MutexLock lock(cache_mutex);
		if (temp_gh_release_list_cache.has(plugin_name)) {
			auto &cache = temp_gh_release_list_cache[plugin_name];
			if (cache.assets.has(release_id)) {
				return cache.assets[release_id].first;
			}
		}
	}
	return Dictionary();
}

Dictionary AssetLibInfoGetter::get_gh_asset_dict(const String &plugin_name, uint64_t release_id, uint64_t asset_id) {
	if (!recache_gh_release_list(plugin_name)) {
		return Dictionary();
	}
	{
		MutexLock lock(cache_mutex);
		if (temp_gh_release_list_cache.has(plugin_name)) {
			auto &cache = temp_gh_release_list_cache[plugin_name];
			if (cache.assets.has(release_id)) {
				auto &asset_map = cache.assets[release_id].second;
				if (asset_map.has(asset_id)) {
					return asset_map[asset_id];
				}
			}
		}
	}
	return Dictionary();
}

void AssetLibInfoGetter::prepop_plugin_cache_gh(const String &plugin_name) {
	auto thing = get_list_of_gh_releases(plugin_name);
	Vector<PluginVersion> vers;
	for (auto &release : thing) {
		uint64_t release_id = release.get("id", 0);
		Array assets = release.get("assets", {});
		for (auto &asset : assets) {
			uint64_t asset_id = ((Dictionary)asset).get("id", 0);
			get_plugin_version_gh(plugin_name, release_id, asset_id);
		}
	}
}

bool AssetLibInfoGetter::recache_gh_release_list(const String &plugin_name) {
	{
		MutexLock lock(cache_mutex);
		if (temp_gh_release_list_cache.has(plugin_name)) {
			if (temp_gh_release_list_cache[plugin_name].retrieved_time + EXPIRY_TIME > OS::get_singleton()->get_unix_time()) {
				return true;
			}
		}
	}
	Vector<Dictionary> releases;
	String repo_url = non_asset_lib_plugins.get(plugin_name);
	if (repo_url.is_empty() || !repo_url.contains("github.com")) {
		return false;
	}
	double now = OS::get_singleton()->get_unix_time();
	// we need to get the org name and the repo name
	// the url is like this: https://github.com/GodotSteam/GodotSteam
	auto thing = repo_url.replace_first("https://", "");
	// now it's like this: github.com/GodotSteam/GodotSteam
	String org = thing.get_slice("/", 1);
	String repo = thing.get_slice("/", 2);
	int pages = 1000;
	for (int page = 1; page < pages; page++) {
		String request_url = github_release_api_url.replace("{0}", org).replace("{1}", repo).replace("{2}", itos(page));
		Vector<uint8_t> response;
		Error err = gdre::wget_sync(request_url, response);
		if (err) {
			break;
		}
		String response_str;
		response_str.parse_utf8((const char *)response.ptr(), response.size());
		Array response_obj = JSON::parse_string(response_str);
		if (response_obj.is_empty()) {
			break;
		}
		for (int i = 0; i < response_obj.size(); i++) {
			Dictionary release = response_obj[i];
			releases.push_back(release);
		}
	}
	GHReleaseListCache::AssetMap assets;
	for (int i = 0; i < releases.size(); i++) {
		Dictionary release = releases[i];
		uint64_t release_id = uint64_t(release.get("id", 0));
		Array assets_arr = release.get("assets", {});
		HashMap<uint64_t, Dictionary> asset_map;
		for (int j = 0; j < assets_arr.size(); j++) {
			Dictionary asset = assets_arr[j];
			uint64_t asset_id = uint64_t(asset.get("id", 0));
			asset_map[asset_id] = asset;
		}
		assets[release_id] = { release, asset_map };
	}
	{
		MutexLock lock(cache_mutex);
		temp_gh_release_list_cache[plugin_name] = { .retrieved_time = now, .edit_list = releases, .assets = assets };
	}

	return true;
}

Vector<Dictionary> AssetLibInfoGetter::get_list_of_gh_releases(const String &plugin_name) {
	if (!recache_gh_release_list(plugin_name)) {
		return {};
	}
	{
		MutexLock lock(cache_mutex);
		if (temp_gh_release_list_cache.has(plugin_name)) {
			return temp_gh_release_list_cache[plugin_name].edit_list;
		}
	}
	return {};
}

/*
Example of a release entry:
```json
{
	"url": "https://api.github.com/repos/GodotSteam/GodotSteam/releases/83320678",
	"assets_url": "https://api.github.com/repos/GodotSteam/GodotSteam/releases/83320678/assets",
	"upload_url": "https://uploads.github.com/repos/GodotSteam/GodotSteam/releases/83320678/assets{?name,label}",
	"html_url": "https://github.com/GodotSteam/GodotSteam/releases/tag/g4b5-s155-gs415",
	"id": 83320678,
	"author": {
	  [...]
	},
	"node_id": "RE_kwDOA2wODc4E919m",
	"tag_name": "g4b5-s155-gs415",
	"target_commitish": "godot4",
	"name": "Godot 4 Beta 5 - Steamworks 1.55 - GodotSteam 4.1.5",
	"draft": false,
	"prerelease": true,
	"created_at": "2022-11-10T22:31:05Z",
	"published_at": "2022-11-17T00:24:51Z",
	"assets": [
	  {
		"url": "https://api.github.com/repos/GodotSteam/GodotSteam/releases/assets/84896606",
		"id": 84896606,
		"node_id": "RA_kwDOA2wODc4FD2te",
		"name": "linux-g4b5-s155-gs415.zip",
		"label": null,
		"uploader": {
		  [...]
		},
		"content_type": "application/x-zip-compressed",
		"state": "uploaded",
		"size": 126090687,
		"download_count": 11,
		"created_at": "2022-11-17T00:21:34Z",
		"updated_at": "2022-11-17T00:24:31Z",
		"browser_download_url": "https://github.com/GodotSteam/GodotSteam/releases/download/g4b5-s155-gs415/linux-g4b5-s155-gs415.zip"
	  },
	  [...]
	],
	"tarball_url": "https://api.github.com/repos/GodotSteam/GodotSteam/tarball/g4b5-s155-gs415",
	"zipball_url": "https://api.github.com/repos/GodotSteam/GodotSteam/zipball/g4b5-s155-gs415",
	"body": "These pre-compiles are built with the latest Godot Engine 4 beta 5 and linked against Steamworks 1.55. There is nothing new other than these just being built with the latest beta.\r\n\r\nAvailable for Windows and Linux. There are no plans for a Mac build at this time.\r\n\r\nZips now include the relevant Steam API dll or so files as well as a steam_appid.txt with 480 in it, which you can change to your game's app ID or use it as is. The Linux version also include a launcher script to properly link the required libraries if needed.",
}
```
*/

bool AssetLibInfoGetter::init_plugin_version_from_gh_release_asset(Dictionary release_entry, uint64_t gh_asset_id, PluginVersion &version) {
	Array assets = release_entry.get("assets", {});
	if (assets.is_empty()) {
		return false;
	}
	for (int i = 0; i < assets.size(); i++) {
		Dictionary asset = assets[i];
		if (uint64_t(asset.get("id", 0)) != gh_asset_id) {
			continue;
		}
		String name = asset.get("name", "");
		if (is_empty_or_null(name)) {
			break;
		}
		// TODO: other zips?
		if (name.get_extension().to_lower() == "zip") {
			String download_url = asset.get("browser_download_url", "");
			if (is_empty_or_null(download_url)) {
				continue;
			}
			version.download_url = download_url;
			version.asset_id = release_entry.get("id", 0);
			version.release_id = gh_asset_id;
			version.from_asset_lib = false;
			version.version = release_entry.get("tag_name", "");
			version.release_date = asset.get("created_at", "");
			return true;
		}
	}
	return false;
}

PluginVersion AssetLibInfoGetter::get_plugin_version_gh(const String &plugin_name, uint64_t release_id, uint64_t asset_id) {
	{
		MutexLock lock(cache_mutex);
		if (non_asset_lib_cache.has(plugin_name) && non_asset_lib_cache[plugin_name].has(release_id)) {
			auto &release = non_asset_lib_cache[plugin_name][release_id];
			if (release.has(asset_id)) {
				return release[asset_id];
			}
		}
	}
	auto release = get_gh_release_dict(plugin_name, release_id);
	if (release.is_empty()) {
		return PluginVersion();
	}
	Array assets = release.get("assets", {});
	PluginVersion version;
	for (int i = 0; i < assets.size(); i++) {
		Dictionary asset = assets[i];
		if (uint64_t(asset.get("id", 0)) == asset_id) {
			if (init_plugin_version_from_gh_release_asset(release, asset_id, version)) {
				break;
			}
		}
	}
	if (version.asset_id == 0) {
		return PluginVersion();
	}
	if (populate_plugin_version_hashes(version) != OK) {
		return PluginVersion();
	}
	if (plugin_name != version.plugin_name) {
		WARN_PRINT("Plugin name mismatch: " + plugin_name + " != " + version.plugin_name + ", forcing...");
		version.plugin_name = plugin_name;
	}
	{
		MutexLock lock(cache_mutex);
		if (!non_asset_lib_cache.has(plugin_name)) {
			non_asset_lib_cache[plugin_name] = {};
		}
		if (!non_asset_lib_cache[plugin_name].has(release_id)) {
			non_asset_lib_cache[plugin_name][release_id] = {};
		}
		non_asset_lib_cache[plugin_name][release_id][asset_id] = version;
	}
	return version;
}

String AssetLibInfoGetter::get_plugin_download_url_non_asset_lib(const String &plugin_name, const Vector<String> hashes) {
	auto thing = get_list_of_gh_releases(plugin_name);
	for (auto &release : thing) {
		uint64_t release_id = release.get("id", 0);
		Array assets = release.get("assets", {});
		for (auto &asset : assets) {
			uint64_t asset_id = ((Dictionary)asset).get("id", 0);
			auto plugin_version = get_plugin_version_gh(plugin_name, release_id, asset_id);
			if (plugin_version.asset_id == 0) {
				continue;
			}
			for (auto &gdext : plugin_version.gdexts) {
				for (auto &bin : gdext.bins) {
					for (auto &hash : hashes) {
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

HashMap<uint64_t, Vector<uint64_t>> AssetLibInfoGetter::get_gh_asset_ids(const String &plugin_name) {
	auto thing = get_list_of_gh_releases(plugin_name);
	HashMap<uint64_t, Vector<uint64_t>> asset_ids;
	for (auto &E : thing) {
		uint64_t release_id = E.get("id", 0);
		Array assets = E.get("assets", {});
		for (auto &A : assets) {
			if (!asset_ids.has(release_id)) {
				asset_ids[release_id] = {};
			}
			asset_ids[release_id].push_back(A.get("id", 0));
		}
	}
	return asset_ids;
}

void AssetLibInfoGetter::save_non_asset_lib_cache() {
	auto cache_folder = get_non_asset_lib_cache_folder();
	ERR_FAIL_COND_MSG(gdre::ensure_dir(cache_folder), "Failed to create cache directory: " + cache_folder);
	MutexLock lock(cache_mutex);
	for (auto &E : non_asset_lib_cache) {
		auto &plugin_name = E.key;
		auto &releases = E.value;
		Dictionary plugin_dict;
		for (auto &R : releases) {
			auto &release_id = R.key;
			plugin_dict[release_id] = Dictionary();
			auto &assets = R.value;
			for (auto &A : assets) {
				((Dictionary)plugin_dict[release_id])[A.key] = A.value.to_json();
			}
		}
		auto plugin_file = cache_folder.path_join(plugin_name + ".json");
		String json = JSON::stringify(plugin_dict, " ", false, true);
		auto fa = FileAccess::open(plugin_file, FileAccess::WRITE);
		ERR_FAIL_COND_MSG(fa.is_null(), "Failed to open file for writing: " + plugin_file);
		fa->store_string(json);
		fa->close();
	}
}

void AssetLibInfoGetter::load_non_asset_lib_cache() {
	auto cache_folder = get_non_asset_lib_cache_folder();
	auto files = Glob::rglob(cache_folder.path_join("**/*.json"), true);
	MutexLock lock(cache_mutex);
	for (auto &file : files) {
		auto fa = FileAccess::open(file, FileAccess::READ);
		ERR_CONTINUE_MSG(fa.is_null(), "Failed to open file for reading: " + file);
		String json = fa->get_as_text();
		Dictionary d = JSON::parse_string(json);
		String plugin_name = file.get_file().replace(".json", "");
		if (!non_asset_lib_cache.has(plugin_name)) {
			non_asset_lib_cache[plugin_name] = {};
		}
		for (auto &key : d.keys()) {
			uint64_t release_id = key;
			non_asset_lib_cache[plugin_name][release_id] = {};
			Dictionary assets = d[key];
			for (auto &A : assets.keys()) {
				uint64_t asset_id = A;
				Dictionary asset = assets[A];
				PluginVersion version = PluginVersion::from_json(asset);
				if (version.cache_version != CACHE_VERSION) {
					continue;
				}
				non_asset_lib_cache[plugin_name][release_id][asset_id] = version;
			}
		}
	}
}
