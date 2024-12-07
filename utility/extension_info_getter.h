#include "core/io/json.h"
#include "core/os/shared_object.h"
#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "utility/godotver.h"
#include "utility/plugin_info.h"

struct EditListCache {
	double retrieved_time = 0;
	Vector<Dictionary> edit_list;
};

class AssetLibInfoGetter {
	static HashMap<int, HashMap<String, PluginVersion>> cache; // asset_id -> version -> PluginVersion
	static HashMap<int, EditListCache> temp_edit_list_cache;
	static Mutex cache_mutex;

	static PluginBin get_plugin_bin(const String &oath, const SharedObject &obj);
	static bool init_plugin_version_from_edit(Dictionary edit_list_entry, PluginVersion &version);
	static Error populate_plugin_version_hashes(PluginVersion &version);
	static PluginVersion get_plugin_version(int asset_id, const String &version);

public:
	static String get_asset_lib_cache_folder();
	static Array search_for_assets(const String &plugin_name, int ver_major = 0);
	static Vector<int> search_for_asset_ids(const String &plugin_name, int ver_major = 0);
	static Vector<Dictionary> get_assets_for_plugin(const String &plugin_name);
	static Vector<Dictionary> get_list_of_edits(int asset_id);
	static Dictionary get_edit(int edit_id);
	static Vector<PluginVersion> get_all_plugin_versions_slow(const String &plugin_name, int asset_id);
	static Vector<String> get_plugin_version_numbers(const String &plugin_name, int asset_id);
	static String get_plugin_download_url(const String &plugin_name, const Vector<String> hashes);
	static void load_cache();
	static void save_cache();
};