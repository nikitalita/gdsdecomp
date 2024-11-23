#include "extension_info_getter.h"
#include "utility/common.h"
#include <cstdint>

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
		auto edits = get_plugin_versions(plugin_name, asset_id);
		new_assets.append(asset);
	}
	return new_assets;
}

struct EditsSorter {
	bool operator()(const Dictionary &a, const Dictionary &b) const {
		return int(a.get("edit_id", 0)) < int(b.get("edit_id", 0));
	}
};

Vector<Dictionary> AssetLibInfoGetter::get_list_of_edits(int asset_id) {
	int page = 0;
	int pages = 1000;
	Vector<Dictionary> edits_vec;
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
	edits_vec.sort_custom<EditsSorter>();
	return edits_vec;
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

Vector<PluginInfo::PluginVersion> AssetLibInfoGetter::get_plugin_versions(const String &plugin_name, int asset_id) {
	auto edits = get_list_of_edits(asset_id);
	Vector<PluginInfo::PluginVersion> versions;
	if (edits.size() == 0) {
		return versions;
	}
	for (int i = 0; i < edits.size(); i++) {
		Dictionary edit_list_entry = edits[i];
		int edit_id = int(edit_list_entry.get("edit_id", {}));
		Vector<uint8_t> response;
		Dictionary edit = get_edit(edit_id);
		String godot_version = edit_list_entry.get("godot_version", "");
		String submit_date = edit_list_entry.get("submit_date", "");
		if (!is_empty_or_null(submit_date)) {
			submit_date = submit_date.split(" ")[0];
		}
		String version = edit.get("version_string", "");
		String download_commit = edit.get("download_commit", "");
		if (is_empty_or_null(version) || is_empty_or_null(download_commit)) {
			continue;
		}
		if (!download_commit.begins_with("http")) {
			String download_url = edit.get("download_url", "");
			if (is_empty_or_null(download_url) || !download_url.begins_with("http")) {
				print_line("Invalid download url: " + download_url);
				continue;
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
		print_line("plugin " + plugin_name + " version: " + version + ", min engine: " + godot_version + " max_engine: " + current_version + ", download_url: " + download_commit);
		PluginInfo::PluginVersion version_dict;
		version_dict.version = version;
		version_dict.min_godot_version = godot_version;
		version_dict.max_godot_reported_version = current_version;
		version_dict.submit_date = submit_date;
		version_dict.download_url = download_commit;
		versions.push_back(version_dict);
	}
	return versions;
}