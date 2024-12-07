#include "extension_info_getter.h"
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

HashMap<int, HashMap<String, PluginVersion>> AssetLibInfoGetter::cache = {};
HashMap<int, EditListCache> AssetLibInfoGetter::temp_edit_list_cache = {};
Mutex AssetLibInfoGetter::cache_mutex = {};

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
		auto edits = get_all_plugin_versions_slow(plugin_name, asset_id);
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
		if (cache.has(asset_id) && cache[asset_id].has(version)) {
			auto &out_version = cache[asset_id][version];
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
		if (!cache.has(asset_id)) {
			cache[asset_id] = {};
		}
		cache[asset_id][version] = plugin_version;
	}

	return plugin_version;
}

Error AssetLibInfoGetter::populate_plugin_version_hashes(PluginVersion &plugin_version) {
	auto temp_folder = GDRESettings::get_singleton()->get_gdre_user_path().path_join(".tmp").path_join(plugin_version.plugin_name);
	String url = plugin_version.download_url;
	String new_temp_foldr = temp_folder.path_join(plugin_version.version);
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
		if (ext == "gdextension" || ext == "gdnative") {
			// get the path relative to the "addons" folder
			auto idx = ipath.to_lower().find("addons");
			if (idx == -1) {
				// just get one path up from the file
				// e.g. 3523231532315/bin/something.gdnative would be bin/something.gdnative
				idx = ipath.get_base_dir().rfind("/") + 1;
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
		return err;
	}
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
	da->remove(unzupped_path);
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
namespace {
HashMap<String, String> init_rel_map() {
	HashMap<String, String> map;
	for (int i = 0; i < GODOT_VERSION_RELEASE_DATES_COUNT; i++) {
		map[_GODOT_VERSION_RELEASE_DATES[i][0]] = _GODOT_VERSION_RELEASE_DATES[i][1];
	}
	return map;
}
bool is_empty_or_null(const String &str) {
	return str.is_empty() || str == "<null>";
}
} //namespace
static HashMap<String, String> GODOT_VERSION_RELEASE_DATES = init_rel_map();

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
	print_line(plugin_name + " version: " + version + ", min engine: " + godot_version + " max_engine: " + current_version + ", download_url: " + download_commit);
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

Vector<PluginVersion> AssetLibInfoGetter::get_all_plugin_versions_slow(const String &plugin_name, int asset_id) {
	auto versions = get_plugin_version_numbers(plugin_name, asset_id);
	Vector<PluginVersion> vers;
	for (auto &E : versions) {
		PluginVersion version_dict = get_plugin_version(asset_id, E);
		if (version_dict.asset_id != 0) {
			vers.push_back(version_dict);
		}
	}
	return vers;
}

String AssetLibInfoGetter::get_plugin_download_url(const String &plugin_name, const Vector<String> hashes) {
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
	MutexLock lock(cache_mutex);
	for (auto &E : cache) {
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

void AssetLibInfoGetter::load_cache() {
	// load the cache
	auto cache_folder = get_asset_lib_cache_folder();
	auto files = Glob::rglob(cache_folder.path_join("**/*.json"), true);
	MutexLock lock(cache_mutex);
	for (auto &file : files) {
		auto fa = FileAccess::open(file, FileAccess::READ);
		ERR_CONTINUE_MSG(fa.is_null(), "Failed to open file for reading: " + file);
		if (fa.is_valid()) {
			String json = fa->get_as_text();
			Dictionary d = JSON::parse_string(json);
			auto version = PluginVersion::from_json(d);
			if (version.cache_version != CACHE_VERSION) {
				continue;
			}
			if (!cache.has(version.asset_id)) {
				cache[version.asset_id] = {};
			}
			cache[version.asset_id][version.version] = version;
		}
	}
}
