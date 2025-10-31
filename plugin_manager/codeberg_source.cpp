#include "plugin_manager/codeberg_source.h"

const String CodebergSource::codeberg_release_api_url = _codeberg_release_api_url;
namespace {
static const HashMap<String, Vector<String>> tag_masks = {
	{ "godotsteam", { "*gdn*", "*gde*" } },
	{ "godotsteam_server", { "*gdn*", "*gde*" } },
};

static const HashMap<String, Vector<String>> release_file_masks = {};

static const HashMap<String, Vector<String>> release_file_exclude_masks = {};

static const HashMap<String, String> plugin_map = {
	{ "godotsteam", "https://codeberg.org/GodotSteam/GodotSteam" },
	{ "godotsteam_server", "https://codeberg.org/GodotSteam/GodotSteam-Server" },
};

// A hack because the godotsteam developers are reckless and broke the release urls when they moved to codeberg for certain releases.
// TODO: remove this when godotsteam fixes their releases.
static const HashMap<String, HashMap<String, String>> bad_release_urls = {
	{ "godotsteam",
			{
					// replace it with the codeberg url
					{ "https://github.com/GodotSteam/GodotSteam/releases/download/v4.14-gde/godotsteam-4.14-gdextension-plugin-4.4.zip",
							"https://codeberg.org/godotsteam/godotsteam/releases/download/v4.14-gde/godotsteam-4.14-gdextension-plugin-4.4.zip" },
					{ "https://github.com/GodotSteam/GodotSteam/releases/download/v4.15-gde/godotsteam-4.15-gdextension-plugin-4.4.zip",
							"https://codeberg.org/godotsteam/godotsteam/archive/81b02cb50e7096e24aa3be863558250b2760e6d4.zip" },
			} },
};

} // namespace

CodebergSource::CodebergSource() {
	// Initialize any necessary resources
}

CodebergSource::~CodebergSource() {
	// Clean up any resources
}

const HashMap<String, String> &CodebergSource::get_plugin_repo_map() {
	return plugin_map;
}

const HashMap<String, Vector<String>> &CodebergSource::get_plugin_tag_masks() {
	return tag_masks;
}

const HashMap<String, Vector<String>> &CodebergSource::get_plugin_release_file_masks() {
	return release_file_masks;
}

const HashMap<String, Vector<String>> &CodebergSource::get_plugin_release_file_exclude_masks() {
	return release_file_exclude_masks;
}

String CodebergSource::get_plugin_name() {
	return "codeberg";
}

// Codeberg API behaves like the GitHub API, so all we have to do is return the correct URL and the page limit
const String &CodebergSource::get_release_api_url() {
	return codeberg_release_api_url;
}

int CodebergSource::get_release_page_limit() {
	return 30;
}

ReleaseInfo CodebergSource::get_release_info(const String &plugin_name, int64_t primary_id, int64_t secondary_id) {
	auto rel = GitHubSource::get_release_info(plugin_name, primary_id, secondary_id);
	if (rel.is_valid() && bad_release_urls.has(plugin_name)) {
		if (bad_release_urls[plugin_name].has(rel.download_url)) {
			rel.download_url = bad_release_urls[plugin_name][rel.download_url];
		}
	}
	return rel;
}
