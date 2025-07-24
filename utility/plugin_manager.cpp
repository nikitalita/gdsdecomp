#include "plugin_manager.h"
#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "modules/zip/zip_reader.h"
#include "utility/common.h"
#include "utility/gdre_settings.h"
#include "utility/glob.h"
#include "utility/plugin_source.h"
#include "utility/task_manager.h"

Ref<PluginSource> PluginManager::sources[MAX_SOURCES];
int PluginManager::source_count = 0;
bool PluginManager::prepopping = false;
HashMap<String, PluginVersion> PluginManager::plugin_version_cache;
Mutex PluginManager::plugin_version_cache_mutex;
// known bad plugin versions and their replacements; these had better be prepopped.
HashMap<String, Pair<String, String>> PluginManager::known_bad_plugin_versions = {
	{ "godotsteam", Pair<String, String>("v4.4.1-gde", "v4.4.2-gde") },
};

String PluginManager::get_plugin_cache_path() {
	// check if OS has the environment variable "GDRE_PLUGIN_CACHE_DIR" set
	// if it is set, use that as the cache folder
	// This is a hack to help prepopulate the cache for releases
	if (OS::get_singleton()->has_environment(PLUGIN_CACHE_ENV_VAR)) {
		return OS::get_singleton()->get_environment(PLUGIN_CACHE_ENV_VAR);
	}
	return GDRESettings::get_singleton()->get_gdre_user_path().path_join("plugin_cache").path_join("v" + itos(CACHE_VERSION));
}

Ref<PluginSource> PluginManager::get_source(const String &plugin_name) {
	// Check if we already have a source for this plugin
	Ref<PluginSource> default_source;
	for (int i = 0; i < source_count; ++i) {
		if (sources[i]->handles_plugin(plugin_name)) {
			if (sources[i]->is_default()) {
				default_source = sources[i];
				continue;
			}
			return sources[i];
		}
	}
	return default_source;
}

void PluginManager::register_source(Ref<PluginSource> source, bool at_front) {
	if (source_count < MAX_SOURCES) {
		if (at_front) {
			for (int i = source_count; i > 0; --i) {
				sources[i] = sources[i - 1];
			}
			sources[0] = source;
		} else {
			sources[source_count] = source;
		}
		source_count++;
	}
}

void PluginManager::unregister_source(Ref<PluginSource> source) {
	// Find source
	int i = 0;
	for (; i < source_count; ++i) {
		if (sources[i] == source) {
			break;
		}
	}
	ERR_FAIL_COND(i >= source_count); // Not found
	for (int j = i; j < source_count - 1; ++j) {
		sources[j] = sources[j + 1];
	}
	sources[source_count - 1].unref();
	--source_count;
}

PluginVersion PluginManager::get_plugin_version_for_key(const String &plugin_name, const String &version) {
	Ref<PluginSource> source = get_source(plugin_name);
	ERR_FAIL_COND_V_MSG(source.is_null(), PluginVersion::invalid(), "No source found for plugin: " + plugin_name);

	// Get ReleaseInfo from the source
	ReleaseInfo release_info = source->get_release_info(plugin_name, version);
	if (release_info.plugin_source.is_empty()) {
		return PluginVersion::invalid(); // No release info available
	}

	// Generate cache key
	String cache_key = get_cache_key(release_info.plugin_source, release_info.primary_id, release_info.secondary_id);

	// Check if we have a cached PluginVersion
	PluginVersion cached_version = get_cached_plugin_version(cache_key);
	if (cached_version.cache_version == CACHE_VERSION) {
		return cached_version;
	}

	// Populate PluginVersion from ReleaseInfo
	PluginVersion plugin_version = populate_plugin_version_from_release(release_info);
	if (!plugin_version.is_valid()) {
		return PluginVersion::invalid(); // Return empty version on error
	}

	// Cache the result
	cache_plugin_version(cache_key, plugin_version);

	return plugin_version;
}

Dictionary PluginManager::get_plugin_info(const String &plugin_name, const Vector<String> &hashes) {
	Ref<PluginSource> source = get_source(plugin_name);
	ERR_FAIL_COND_V_MSG(source.is_null(), Dictionary(), "No source found for plugin: " + plugin_name);

	// First, check all cached PluginVersions for this plugin
	{
		MutexLock lock(plugin_version_cache_mutex);
		String first_version;
		String replacement_version;
		for (auto &E : plugin_version_cache) {
			const PluginVersion &cached_version = E.value;

			// Check if this cached version is for the requested plugin
			if (cached_version.is_valid() && cached_version.plugin_name == plugin_name) {
				// Check if any of the hashes match
				if (cached_version.bin_hashes_match(hashes)) {
					print_line("Found matching plugin in cache: " + plugin_name + ", version: " + cached_version.release_info.version + ", download url: " + cached_version.release_info.download_url);
					if (known_bad_plugin_versions.has(plugin_name) && known_bad_plugin_versions[plugin_name].first == cached_version.release_info.version) {
						first_version = cached_version.release_info.version;
						replacement_version = known_bad_plugin_versions[plugin_name].second;
						break;
					}
					return cached_version.to_json();
				}
			}
		}
		if (!replacement_version.is_empty()) {
			for (auto &E : plugin_version_cache) {
				const PluginVersion &cached_version = E.value;
				if (cached_version.is_valid() && cached_version.plugin_name == plugin_name && cached_version.release_info.version == replacement_version) {
					print_line("Found known bad plugin version: " + plugin_name + ", version: " + first_version + ", replacing with: " + replacement_version);
					return cached_version.to_json();
				}
			}
			ERR_FAIL_V_MSG(Dictionary(), "!!!!!!!!!\nNO REPLACEMENT FOUND\n!!!!!!!!!!!");
		}
	}

	// If no cached versions match, get all release info and populate PluginVersions
	Vector<String> version_keys = source->get_plugin_version_numbers(plugin_name);
	for (auto &version_key : version_keys) {
		ReleaseInfo release_info = source->get_release_info(plugin_name, version_key);
		if (release_info.plugin_source.is_empty()) {
			continue; // Skip if no release info available
		}
		// Check if we already have a cached version for this key
		String cache_key = get_cache_key(release_info.plugin_source, release_info.primary_id, release_info.secondary_id);
		{
			MutexLock lock(plugin_version_cache_mutex);
			if (plugin_version_cache.has(cache_key)) {
				continue;
			}
		}

		// Populate PluginVersion from ReleaseInfo
		PluginVersion plugin_version = populate_plugin_version_from_release(release_info);
		if (!plugin_version.is_valid()) {
			continue; // Skip if population failed
		}

		// Cache the result
		cache_plugin_version(cache_key, plugin_version);

		// Check if this version matches the hashes
		if (plugin_version.bin_hashes_match(hashes)) {
			print_line("Found matching plugin after population: " + plugin_name + ", version: " + plugin_version.release_info.version + ", download url: " + plugin_version.release_info.download_url);
			return plugin_version.to_json();
		}
	}

	return Dictionary();
}

void PluginManager::load_cache() {
	Dictionary d;
	if (FileAccess::exists(STATIC_PLUGIN_CACHE_PATH)) {
		load_plugin_version_cache_file(STATIC_PLUGIN_CACHE_PATH);
	}
	for (int i = 0; i < source_count; ++i) {
		sources[i]->load_cache();
	}

	// Load PluginVersion cache
	load_plugin_version_cache();
}

void PluginManager::save_cache() {
	for (int i = 0; i < source_count; ++i) {
		sources[i]->save_cache();
	}

	// Save PluginVersion cache
	save_plugin_version_cache();
}

struct PrePopToken {
	String plugin_name;
	Ref<PluginSource> source;
	String version;
};

struct PrePopTask {
	void do_task(uint32_t index, const PrePopToken *tokens) {
		auto &token = tokens[index];
		// Use the new workflow: get ReleaseInfo and populate PluginVersion
		ReleaseInfo release_info = token.source->get_release_info(token.plugin_name, token.version);
		if (!release_info.plugin_source.is_empty()) {
			String cache_key = PluginManager::get_cache_key(release_info.plugin_source, release_info.primary_id, release_info.secondary_id);
			PluginVersion cached_version = PluginManager::get_cached_plugin_version(cache_key);
			if (!cached_version.is_valid()) {
				PluginVersion plugin_version = PluginManager::populate_plugin_version_from_release(release_info);
				if (plugin_version.is_valid()) {
					PluginManager::cache_plugin_version(cache_key, plugin_version);
				}
			}
		}
	}
};

void PluginManager::prepop_cache(const Vector<String> &plugin_names, bool multithread) {
	prepopping = true;
	String plugin_names_str = String(", ").join(plugin_names);
	print_line("Prepopulating cache for " + plugin_names_str);
	print_line("Plugin dir: " + get_plugin_cache_path());
	Vector<PrePopToken> tokens;
	for (int i = 0; i < plugin_names.size(); i++) {
		auto &plugin_name = plugin_names[i];
		auto source = get_source(plugin_name);
		if (source.is_null()) {
			continue;
		}
		auto versions = source->get_plugin_version_numbers(plugin_name);
		for (auto &version : versions) {
			PrePopToken token;
			token.plugin_name = plugin_name;
			token.source = source;
			token.version = version;
			tokens.push_back(token);
		}
	}
	PrePopTask task;
	if (multithread) {
		gdre::shuffle_vector(tokens);
		auto group_id = WorkerThreadPool::get_singleton()->add_template_group_task(
				&task,
				&PrePopTask::do_task,
				tokens.ptr(),
				tokens.size(), -1, true, SNAME("GDRESettings::prepop_plugin_cache_gh"));
		WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_id);
	} else {
		for (int i = 0; i < tokens.size(); i++) {
			task.do_task(i, tokens.ptr());
		}
	}

	prepopping = false;
}

bool PluginManager::is_prepopping() {
	return prepopping;
}

String PluginManager::get_cache_key(const String &plugin_source, uint64_t primary_id, uint64_t secondary_id) {
	return plugin_source + "-" + itos(primary_id) + "-" + itos(secondary_id);
}

PluginVersion PluginManager::get_cached_plugin_version(const String &cache_key) {
	MutexLock lock(plugin_version_cache_mutex);
	if (plugin_version_cache.has(cache_key)) {
		return plugin_version_cache[cache_key];
	}
	return PluginVersion::invalid();
}

void PluginManager::cache_plugin_version(const String &cache_key, const PluginVersion &version) {
	MutexLock lock(plugin_version_cache_mutex);
	plugin_version_cache[cache_key] = version;
}

PluginVersion PluginManager::populate_plugin_version_from_release(const ReleaseInfo &release_info) {
	PluginVersion version;
	version.release_info = release_info;
	version.plugin_name = ""; // Will be populated during analysis
	version.min_godot_version = "";
	version.max_godot_version = "";
	version.base_folder = "";

	// Download and analyze the plugin using the new method
	Error err = populate_plugin_version_hashes(version);
	if (err != OK) {
		return PluginVersion::invalid(); // Return empty version on error
	}

	return version;
}

Error PluginManager::populate_plugin_version_hashes(PluginVersion &plugin_version) {
	auto temp_folder = GDRESettings::get_singleton()->get_gdre_tmp_path();
	String url = plugin_version.release_info.download_url;
	String new_temp_foldr = temp_folder.path_join(itos(plugin_version.release_info.primary_id) + "_" + itos(plugin_version.release_info.secondary_id));
	String zip_path = new_temp_foldr.path_join("plugin.zip");

	Error err = OK;
	if (!is_prepopping()) {
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

	print_line("Populating plugin version hashes for " + plugin_version.plugin_name + " version: " + plugin_version.release_info.version);
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
				auto plugin_bin = PluginSource::get_plugin_bin(real_path, E);
				plugin_bins.push_back(plugin_bin);
			}
			return plugin_bins;
		};

		gdext_info.bins = parse_bins(cf->get_libaries(false));
		gdext_info.dependencies = parse_bins(cf->get_dependencies(false));
		plugin_version.gdexts.push_back(gdext_info);
	}

	close_and_remove_zip();
	gdre::rimraf(new_temp_foldr);
	return OK;
}

void PluginManager::load_plugin_version_cache() {
	String cache_dir = get_plugin_cache_path().path_join("plugin_versions");
	if (!DirAccess::exists(cache_dir)) {
		return;
	}
	auto files = Glob::rglob(cache_dir.path_join("**/*.json"), true);
	for (auto &file : files) {
		load_plugin_version_cache_file(file);
	}
}

void PluginManager::load_plugin_version_cache_file(const String &cache_file) {
	// this works with both the static and dynamic cache files
	auto file = FileAccess::open(cache_file, FileAccess::READ);
	if (file.is_null()) {
		ERR_PRINT("Failed to open plugin version cache file!");
		return;
	}

	String json = file->get_as_text();
	Dictionary data = JSON::parse_string(json);

	MutexLock lock(plugin_version_cache_mutex);
	for (auto &E : data.keys()) {
		Dictionary version_data = data[E];
		PluginVersion version = PluginVersion::from_json(version_data);
		if (version.is_valid()) {
			String cache_key = get_cache_key(version.release_info.plugin_source, version.release_info.primary_id, version.release_info.secondary_id);
			plugin_version_cache[cache_key] = version;
		}
	}
}

void PluginManager::save_plugin_version_cache() {
	String cache_dir = get_plugin_cache_path().path_join("plugin_versions");
	gdre::ensure_dir(cache_dir);
	HashMap<String, HashMap<String, Dictionary>> data;
	{
		MutexLock lock(plugin_version_cache_mutex);
		for (auto &E : plugin_version_cache) {
			String cache_key = E.key;
			PluginVersion version = E.value;
			if (version.is_valid()) {
				Dictionary version_json = version.to_json();
				String source = version.release_info.plugin_source;
				String primary_id = itos(version.release_info.primary_id);
				String secondary_id = itos(version.release_info.secondary_id);
				if (!data.has(source)) {
					data[source] = HashMap<String, Dictionary>();
				}
				if (!data[source].has(primary_id)) {
					data[source][primary_id] = Dictionary();
				}
				data[source][primary_id][secondary_id] = version_json;
			}
		}
	}

	for (auto &source : data) {
		for (auto &primary_id : source.value) {
			// source/primary_id.json - with secondary_id as the key
			String file_name = source.key + "/" + primary_id.key + ".json";
			String path = cache_dir.path_join(file_name);
			gdre::ensure_dir(path.get_base_dir());
			auto file = FileAccess::open(path, FileAccess::WRITE);
			if (file.is_null()) {
				ERR_PRINT("Failed to open plugin version cache file for writing: " + file_name);
				continue;
			}
			Dictionary d = primary_id.value;
			String json = JSON::stringify(d, " ", false, true);
			file->store_string(json);
			file->flush();
			file->close();
			// if we don't clear the dictionary here, it will sometimes cause a crash when the hashmap is cleared
			primary_id.value = Dictionary();
		}
		source.value.clear();
	}
	data.clear();
}

void PluginManager::_bind_methods() {
	// ClassDB::bind_method(D_METHOD("get_plugin_version", "plugin_name", "version"), &PluginManager::get_plugin_version);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("get_plugin_info", "plugin_name", "hashes"), &PluginManager::get_plugin_info);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("load_cache"), &PluginManager::load_cache);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("save_cache"), &PluginManager::save_cache);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("prepop_cache", "plugin_names", "multithread"), &PluginManager::prepop_cache, DEFVAL(true));
	ClassDB::bind_static_method(get_class_static(), D_METHOD("register_source", "name", "source"), &PluginManager::register_source);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("unregister_source", "name"), &PluginManager::unregister_source);
}
