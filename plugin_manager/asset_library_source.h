#ifndef ASSET_LIBRARY_SOURCE_H
#define ASSET_LIBRARY_SOURCE_H

#include "core/variant/dictionary.h"
#include "plugin_source.h"

struct EditListCache {
	double retrieved_time = 0;
	int64_t asset_id = 0;
	Vector<Dictionary> edit_list;
	Dictionary to_json() const;
	static EditListCache from_json(const Dictionary &json);
};

struct EditCache {
	double retrieved_time = 0;
	int64_t edit_id = 0;
	Dictionary edit;
	Dictionary to_json() const;
	static EditCache from_json(const Dictionary &json);
};

class AssetLibrarySource : public PluginSource {
	GDCLASS(AssetLibrarySource, PluginSource)

private:
	static HashMap<String, String> GODOT_VERSION_RELEASE_DATES;

	HashMap<int64_t, EditListCache> edit_list_cache;
	HashMap<int64_t, EditCache> edit_cache;

	// Helper methods
	Error get_edit_list(int64_t asset_id, Vector<Dictionary> &r_edits);
	Error get_edit(int64_t edit_id, Dictionary &r_edit);
	Error search_for_assets(const String &plugin_name, Vector<Dictionary> &r_assets, int ver_major = 0);
	Error search_for_asset_ids(const String &plugin_name, Vector<int64_t> &r_asset_ids, int ver_major = 0);
	Error get_assets_for_plugin(const String &plugin_name, Vector<Dictionary> &r_assets);
	Error get_valid_edit_ids_for_plugin(int64_t asset_id, Vector<int64_t> &r_versions);
	Error load_edit_list_cache();
	void load_edit_cache();
	void save_edit_list_cache();
	void save_edit_cache();

public:
	// Implementation of PluginSource interface
	Vector<Pair<int64_t, int64_t>> get_plugin_version_numbers(const String &plugin_name, Error &r_connection_error) override;
	ReleaseInfo get_release_info(const String &plugin_name, int64_t primary_id, int64_t secondary_id, Error &r_connection_error) override;
	void load_cache_internal() override;
	void save_cache() override;
	bool handles_plugin(const String &plugin_name) override;
	bool is_default() override { return true; }
	String get_plugin_name() override;
	Vector<ReleaseInfo> find_release_infos_by_tag(const String &plugin_name, const String &tag, Error &r_error) override;
};

#endif // ASSET_LIBRARY_SOURCE_H
