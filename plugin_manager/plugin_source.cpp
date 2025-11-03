#include "plugin_source.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "plugin_info.h"
#include "utility/common.h"

PluginBin PluginSource::get_plugin_bin(const String &path, const SharedObject &obj) {
	PluginBin bin;
	bin.name = obj.path;
	bin.md5 = gdre::get_md5(path, true);
	bin.tags = obj.tags;
	return bin;
}

bool PluginSource::handles_plugin(const String &plugin_name) {
	ERR_FAIL_V_MSG(false, "Not implemented");
}

bool PluginSource::is_default() {
	return false;
}

ReleaseInfo PluginSource::get_release_info(const String &plugin_name, int64_t primary_id, int64_t secondary_id, Error &r_connection_error) {
	r_connection_error = ERR_UNAVAILABLE;
	return ReleaseInfo();
}

void PluginSource::load_cache_internal() {
	ERR_FAIL_MSG("Not implemented");
}

void PluginSource::save_cache() {
	ERR_FAIL_MSG("Not implemented");
}

Vector<Pair<int64_t, int64_t>> PluginSource::get_plugin_version_numbers(const String &plugin_name, Error &r_connection_error) {
	r_connection_error = ERR_UNAVAILABLE;
	ERR_FAIL_V_MSG({}, "Not implemented");
}

void PluginSource::load_cache() {
	load_cache_internal();
}

String PluginSource::get_plugin_name() {
	ERR_FAIL_V_MSG(String(), "Not implemented");
}

Dictionary PluginSource::_get_plugin_version_numbers(const String &plugin_name) {
	Dictionary d;
	Error err = OK;
	auto pairs = get_plugin_version_numbers(plugin_name, err);
	ERR_FAIL_COND_V_MSG(err != OK, Dictionary(), "Failed to get plugin version numbers for plugin " + plugin_name);
	for (auto &E : pairs) {
		d[E.first] = E.second;
	}
	return d;
}

Dictionary PluginSource::_get_release_info(const String &plugin_name, int64_t primary_id, int64_t secondary_id) {
	Error err = OK;
	auto rel = get_release_info(plugin_name, primary_id, secondary_id, err);
	if (!rel.is_valid() || err) {
		return Dictionary();
	}
	return rel.to_json();
}

Vector<ReleaseInfo> PluginSource::find_release_infos_by_tag(const String &plugin_name, const String &tag, Error &r_error) {
	r_error = ERR_UNAVAILABLE;
	ERR_FAIL_V_MSG({}, "Not implemented");
}

void PluginSource::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_release_info", "plugin_name", "primary_id", "secondary_id"), &PluginSource::_get_release_info);
	ClassDB::bind_method(D_METHOD("get_plugin_version_numbers", "plugin_name"), &PluginSource::_get_plugin_version_numbers);
	ClassDB::bind_method(D_METHOD("load_cache"), &PluginSource::load_cache);
	ClassDB::bind_method(D_METHOD("save_cache"), &PluginSource::save_cache);
	ClassDB::bind_method(D_METHOD("handles_plugin", "plugin_name"), &PluginSource::handles_plugin);
	ClassDB::bind_method(D_METHOD("is_default"), &PluginSource::is_default);
}
