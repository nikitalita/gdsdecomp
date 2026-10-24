#include "register_types.h"
#include "core/object/class_db.h"
#include "plugin_manager/asset_library_source.h"
#include "plugin_manager/codeberg_source.h"
#include "plugin_manager/github_source.h"
#include "plugin_manager/gitlab_source.h"
#include "plugin_manager/plugin_manager.h"
#include "plugin_manager/plugin_source.h"

static PluginManager *plugin_manager = nullptr;
//plugin manager sources
static Ref<CodebergSource> codeberg_source = nullptr;
static Ref<GitHubSource> github_source = nullptr;
static Ref<AssetLibrarySource> asset_library_source = nullptr;
static Ref<GitLabSource> gitlab_source = nullptr;

void init_plugin_manager_sources() {
	codeberg_source = memnew(CodebergSource);
	github_source = memnew(GitHubSource);
	gitlab_source = memnew(GitLabSource);
	asset_library_source = memnew(AssetLibrarySource);

	PluginManager::get_singleton()->register_source(codeberg_source, false);
	PluginManager::get_singleton()->register_source(github_source, false);
	PluginManager::get_singleton()->register_source(gitlab_source, false);
	PluginManager::get_singleton()->register_source(asset_library_source, false);
}

void deinit_plugin_manager_sources() {
	if (codeberg_source.is_valid()) {
		PluginManager::get_singleton()->unregister_source(codeberg_source, false);
	}
	if (github_source.is_valid()) {
		PluginManager::get_singleton()->unregister_source(github_source, false);
	}
	if (gitlab_source.is_valid()) {
		PluginManager::get_singleton()->unregister_source(gitlab_source, false);
	}
	if (asset_library_source.is_valid()) {
		PluginManager::get_singleton()->unregister_source(asset_library_source, false);
	}
	codeberg_source = nullptr;
	github_source = nullptr;
	gitlab_source = nullptr;
	asset_library_source = nullptr;
}

void initialize_gdsdecomp_plugin_manager_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_class<PluginManager>();
	ClassDB::register_class<PluginSource>();
	ClassDB::register_class<GitHubSource>();
	ClassDB::register_class<AssetLibrarySource>();
	ClassDB::register_class<GitLabSource>();

	plugin_manager = memnew(PluginManager);
	Engine::get_singleton()->add_singleton(Engine::Singleton("PluginManager", PluginManager::get_singleton()));
	init_plugin_manager_sources();
}

void uninitialize_gdsdecomp_plugin_manager_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	deinit_plugin_manager_sources();
	if (plugin_manager) {
		memdelete(plugin_manager);
	}
	plugin_manager = nullptr;
}
