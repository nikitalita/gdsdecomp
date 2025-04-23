#include "plugin_manager.h"
#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "utility/common.h"
#include "utility/gdre_settings.h"
#include "utility/plugin_source.h"

Ref<PluginSource> PluginManager::sources[MAX_SOURCES];
int PluginManager::source_count = 0;
bool PluginManager::prepopping = false;

String PluginManager::get_plugin_cache_path() {
	// check if OS has the environment variable "GDRE_PLUGIN_CACHE_DIR" set
	// if it is set, use that as the cache folder
	// This is a hack to help prepopulate the cache for releases
	if (OS::get_singleton()->has_environment(PLUGIN_CACHE_ENV_VAR)) {
		return OS::get_singleton()->get_environment(PLUGIN_CACHE_ENV_VAR);
	}
	return GDRESettings::get_singleton()->get_gdre_user_path().path_join("plugin_cache");
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

PluginVersion PluginManager::get_plugin_version(const String &plugin_name, const String &version) {
	Ref<PluginSource> source = get_source(plugin_name);
	ERR_FAIL_COND_V_MSG(source.is_null(), PluginVersion(), "No source found for plugin: " + plugin_name);
	return source->get_plugin_version(plugin_name, version);
}

String PluginManager::get_plugin_download_url(const String &plugin_name, const Vector<String> &hashes) {
	Ref<PluginSource> source = get_source(plugin_name);
	ERR_FAIL_COND_V_MSG(source.is_null(), String(), "No source found for plugin: " + plugin_name);
	return source->get_plugin_download_url(plugin_name, hashes);
}

void PluginManager::load_cache() {
	Dictionary d;
	if (FileAccess::exists(STATIC_PLUGIN_CACHE_PATH)) {
		auto file = FileAccess::open(STATIC_PLUGIN_CACHE_PATH, FileAccess::READ);
		if (file.is_null()) {
			ERR_PRINT("Failed to open static plugin cache file!");
		}
		d = JSON::parse_string(file->get_as_text());
	}
	for (int i = 0; i < source_count; ++i) {
		sources[i]->load_static_precache(d);
		sources[i]->load_cache();
	}
}

void PluginManager::save_cache() {
	for (int i = 0; i < source_count; ++i) {
		sources[i]->save_cache();
	}
}

struct PrePopToken {
	String plugin_name;
	Ref<PluginSource> source;
	String version;
};

struct PrePopTask {
	void do_task(uint32_t index, const PrePopToken *tokens) {
		auto &token = tokens[index];
		auto version = token.source->get_plugin_version(token.plugin_name, token.version);
	}
};

void PluginManager::prepop_cache(const Vector<String> &plugin_names, bool multithread) {
	prepopping = true;
	String plugin_names_str = String(", ").join(plugin_names);
	print_line("Prepopulating GitHub cache for " + plugin_names_str);
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

void PluginManager::_bind_methods() {
	// ClassDB::bind_method(D_METHOD("get_plugin_version", "plugin_name", "version"), &PluginManager::get_plugin_version);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("get_plugin_download_url", "plugin_name", "hashes"), &PluginManager::get_plugin_download_url);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("load_cache"), &PluginManager::load_cache);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("save_cache"), &PluginManager::save_cache);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("prepop_cache", "plugin_names", "multithread"), &PluginManager::prepop_cache, DEFVAL(true));
	ClassDB::bind_static_method(get_class_static(), D_METHOD("register_source", "name", "source"), &PluginManager::register_source);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("unregister_source", "name"), &PluginManager::unregister_source);
}
