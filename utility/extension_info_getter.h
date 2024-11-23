#include "core/io/json.h"
#include "core/string/ustring.h"
#include "utility/godotver.h"

struct PluginInfo {
	struct PluginVersion {
		String version;
		String min_godot_version;
		String max_godot_reported_version;
		String max_version;
		String submit_date;
		String download_url;
		bool is_compatible(const Ref<GodotVer> &ver) const {
			auto min_ver = GodotVer::parse(min_godot_version);
			if (ver->get_major() != min_ver->get_major()) {
				return false;
			}
			if (!max_version.is_empty()) {
				return (ver->get_minor() >= min_ver->get_minor() && ver->get_minor() <= GodotVer::parse(max_version)->get_minor());
			}
			return ver->get_minor() >= min_ver->get_minor();
		}

		bool is_compatible(const String &godot_version) const {
			auto ver = GodotVer::parse(godot_version);
			return is_compatible(ver);
		}
	};
	String name;
	Vector<PluginVersion> versions;
};

class AssetLibInfoGetter {
public:
	static Array search_for_assets(const String &plugin_name, int ver_major = 0);
	static Vector<int> search_for_asset_ids(const String &plugin_name, int ver_major = 0);
	static Vector<Dictionary> get_assets_for_plugin(const String &plugin_name);
	static Vector<Dictionary> get_list_of_edits(int asset_id);
	static Dictionary get_edit(int edit_id);
	static Vector<PluginInfo::PluginVersion> get_plugin_versions(const String &plugin_name, int asset_id);
};