#include "plugin_source.h"
#include "common.h"
#include "core/crypto/crypto_core.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "modules/zip/zip_reader.h"
#include "plugin_info.h"
#include "utility/gdre_settings.h"
#include "utility/glob.h"
#include "utility/plugin_manager.h"
#include "utility/task_manager.h"

PluginBin PluginSource::get_plugin_bin(const String &path, const SharedObject &obj) {
	PluginBin bin;
	bin.name = obj.path;
	bin.md5 = gdre::get_md5(path, true);
	bin.tags = obj.tags;
	return bin;
}

Error PluginSource::populate_plugin_version_hashes(PluginVersion &plugin_version) {
	auto temp_folder = GDRESettings::get_singleton()->get_gdre_user_path().path_join(".tmp").path_join(itos(plugin_version.asset_id));
	String url = plugin_version.download_url;
	String new_temp_foldr = temp_folder.path_join(itos(plugin_version.release_id));
	String zip_path = new_temp_foldr.path_join("plugin.zip");

	Error err = OK;
	if (!PluginManager::is_prepopping()) {
		auto task_id = TaskManager::get_singleton()->add_download_task(url, zip_path);
		err = TaskManager::get_singleton()->wait_for_download_task_completion(task_id);
	} else {
		err = gdre::download_file_sync(url, zip_path);
	}

	if (err) {
		if (err == ERR_FILE_NOT_FOUND) {
			ERR_FAIL_V_MSG(err, "Failed plugin download (404): " + url);
		} else if (err == ERR_UNAUTHORIZED) {
			ERR_FAIL_V_MSG(err, "Failed plugin download (401): " + url);
		}
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
		gdext_str.append_utf8((const char *)data.ptr(), data.size());
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
	gdre::rimraf(unzupped_path);
	return OK;
}

void PluginSource::prepop_cache(const Vector<String> &plugin_names, bool multithread) {
	ERR_FAIL_MSG("Not implemented");
}

bool PluginSource::handles_plugin(const String &plugin_name) {
	ERR_FAIL_V_MSG(false, "Not implemented");
}

bool PluginSource::is_default() {
	return false;
}

void PluginSource::load_cache_internal() {
	ERR_FAIL_MSG("Not implemented");
}

void PluginSource::save_cache() {
	ERR_FAIL_MSG("Not implemented");
}

PluginVersion PluginSource::get_plugin_version(const String &plugin_name, const String &version) {
	ERR_FAIL_V_MSG(PluginVersion(), "Not implemented");
}

String PluginSource::get_plugin_download_url(const String &plugin_name, const Vector<String> &hashes) {
	ERR_FAIL_V_MSG(String(), "Not implemented");
}

Vector<String> PluginSource::get_plugin_version_numbers(const String &plugin_name) {
	ERR_FAIL_V_MSG(Vector<String>(), "Not implemented");
}

void PluginSource::load_static_precache(const Dictionary &d) {
	Dictionary data = d.get(get_plugin_name(), Dictionary());
	for (auto &pair : data) {
		String plugin_name = pair.key;
		Dictionary d = pair.value;
		load_cache_data(plugin_name, d);
	}
}

void PluginSource::load_cache() {
	load_cache_internal();
}

void PluginSource::load_cache_data(const String &plugin_name, const Dictionary &data) {
	ERR_FAIL_MSG("Not implemented");
}

String PluginSource::get_plugin_name() {
	ERR_FAIL_V_MSG(String(), "Not implemented");
}

void PluginSource::_bind_methods() {
	// ClassDB::bind_method(D_METHOD("get_plugin_version", "plugin_name", "version"), &PluginSource::get_plugin_version);
	ClassDB::bind_method(D_METHOD("get_plugin_download_url", "plugin_name", "hashes"), &PluginSource::get_plugin_download_url);
	ClassDB::bind_method(D_METHOD("get_plugin_version_numbers", "plugin_name"), &PluginSource::get_plugin_version_numbers);
	ClassDB::bind_method(D_METHOD("load_cache"), &PluginSource::load_cache);
	ClassDB::bind_method(D_METHOD("save_cache"), &PluginSource::save_cache);
	ClassDB::bind_method(D_METHOD("prepop_cache", "plugin_names", "multithread"), &PluginSource::prepop_cache, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("handles_plugin", "plugin_name"), &PluginSource::handles_plugin);
	ClassDB::bind_method(D_METHOD("is_default"), &PluginSource::is_default);
}
