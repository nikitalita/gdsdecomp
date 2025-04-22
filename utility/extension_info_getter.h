#include "core/os/shared_object.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "utility/plugin_info.h"

struct EditListCache {
	double retrieved_time = 0;
	Vector<Dictionary> edit_list;
};

// alias HashMap<uint64_t, Pair<Dictionary, HashMap<uint64_t, Dictionary>>> to type AssetMap
struct GHReleaseListCache {
	using AssetMap = HashMap<uint64_t, Pair<Dictionary, HashMap<uint64_t, Dictionary>>>;
	double retrieved_time = 0;
	Vector<Dictionary> edit_list;
	AssetMap assets;
};

struct PrePopTask;

class AssetLibInfoGetter {
	friend struct PrePopTask;
	static HashMap<String, String> non_asset_lib_plugins; // plugin_name -> github_url
	static HashMap<String, Vector<String>> non_asset_lib_tag_masks; // plugin_name -> tag_suffixes
	static HashMap<String, Vector<String>> non_asset_lib_release_file_masks; // plugin_name -> tag_suffixes

	static HashMap<String, HashMap<uint64_t, HashMap<uint64_t, PluginVersion>>> non_asset_lib_cache; // plugin_name -> tag -> asset_id -> PluginVersion
	static HashMap<uint64_t, HashMap<String, PluginVersion>> asset_lib_cache; // asset_id -> version -> PluginVersion
	static HashMap<uint64_t, EditListCache> temp_edit_list_cache;
	static HashMap<String, GHReleaseListCache> temp_gh_release_list_cache;
	static HashMap<String, String> GODOT_VERSION_RELEASE_DATES;
	static constexpr const char *PLUGIN_CACHE_ENV_VAR = "GDRE_PLUGIN_CACHE_DIR";
	static Mutex cache_mutex;
	static bool is_prepopping;

	static PluginBin get_plugin_bin(const String &oath, const SharedObject &obj);
	static bool init_plugin_version_from_edit(Dictionary edit_list_entry, PluginVersion &version);
	static Error populate_plugin_version_hashes(PluginVersion &version);

	static Dictionary get_gh_release_dict(const String &plugin_name, uint64_t release_id);
	static Dictionary get_gh_asset_dict(const String &plugin_name, uint64_t release_id, uint64_t asset_id);
	static bool recache_gh_release_list(const String &plugin_name);
	static bool recache_gl_release_list(const String &plugin_name);
	static bool init_plugin_version_from_gh_release_asset(Dictionary release_entry, uint64_t gh_asset_id, PluginVersion &version);
	static String get_plugin_download_url_non_asset_lib(const String &plugin_name, const Vector<String> hashes);
	static Vector<Pair<uint64_t, uint64_t>> get_gh_asset_pairs(const String &plugin_name);
	static bool should_skip_tag(const String &plugin_name, const String &tag);
	static Vector<Dictionary> get_assets_for_plugin(const String &plugin_name);
	static Vector<Dictionary> get_list_of_edits(int asset_id);
	static Array search_for_assets(const String &plugin_name, int ver_major = 0);
	static Vector<int> search_for_asset_ids(const String &plugin_name, int ver_major = 0);
	static Dictionary get_edit(int edit_id);
	static String get_non_asset_lib_cache_folder();
	static String get_main_cache_folder();
	static HashMap<uint64_t, Vector<uint64_t>> get_gh_asset_ids(const String &plugin_name);
	static Vector<Dictionary> get_list_of_gh_releases(const String &plugin_name);
	static void load_non_asset_lib_cache();
	static void save_non_asset_lib_cache();
	static String get_asset_lib_cache_folder();
	static Vector<String> get_plugin_version_numbers(const String &plugin_name, int asset_id);
	static PluginVersion get_plugin_version_gh(const String &plugin_name, uint64_t release_id, uint64_t asset_id);

public:
	static PluginVersion get_plugin_version(int asset_id, const String &version);
	static void prepop_plugin_cache(const Vector<String> &plugin_names, bool multithread = false);
	static String get_plugin_download_url(const String &plugin_name, const Vector<String> hashes);
	static void load_cache();
	static void save_cache();
};