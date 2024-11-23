#include "gdextension_exporter.h"
#include "core/os/os.h"
#include "core/os/shared_object.h"
#include "exporters/export_report.h"
#include "modules/zip/zip_reader.h"
#include "utility/common.h"
#include "utility/extension_info_getter.h"
#include "utility/gdre_settings.h"
#include "utility/glob.h"
#include "utility/godotsteam_versions.h"
#include "utility/import_info.h"
Error GDExtensionExporter::export_file(const String &p_dest_path, const String &p_src_path) {
	auto report = export_resource(p_dest_path.get_base_dir(), ImportInfo::load_from_file(p_src_path));
	return report->get_error();
}

#define GDExt_ERR_FAIL_COND_V_MSG(err, ret, msg) \
	{                                            \
		if (err) {                               \
			ret->set_error(err);                 \
			ret->set_message(msg);               \
			ERR_FAIL_V_MSG(ret, msg);            \
		}                                        \
	}

Error find_libs(const Vector<SharedObject> &libs, HashMap<String, SharedObject> &lib_paths) {
	if (libs.size() == 0) {
		return OK;
	}
	HashSet<String> libs_to_find;
	String parent_dir = GDRESettings::get_singleton()->get_pack_path().get_base_dir();
	Error err;
	for (const SharedObject &so : libs) {
		libs_to_find.insert(so.path.get_file());
	}
	{
		Ref<DirAccess> da = DirAccess::open(parent_dir, &err);
		ERR_FAIL_COND_V_MSG(err, ERR_FILE_CANT_OPEN, "Failed to open directory " + parent_dir);
		da->list_dir_begin();
		String f = da->get_next();
		while (!f.is_empty()) {
			if (f != "." && f != "..") {
				for (const SharedObject &so : libs) {
					if (so.path.get_file().to_lower() == f.to_lower()) {
						lib_paths[parent_dir.path_join(f)] = so;
					}
				}
			}
			f = da->get_next();
		}
	}
	if (lib_paths.size() == 0) {
		// if we're on MacOS, try one path up
		if (parent_dir.get_file() == "Resources") {
			parent_dir = parent_dir.get_base_dir();
			Vector<String> globs;
			for (String lib : libs_to_find) {
				globs.push_back(parent_dir.path_join("**").path_join(lib));
			}
			auto paths = Glob::rglob_list(globs, true);
			for (String path : paths) {
				for (const SharedObject &so : libs) {
					if (so.path.get_file().to_lower() == path.get_file().to_lower()) {
						lib_paths[path] = so;
					}
				}
			}
		}
	}
	if (lib_paths.size() == 0) {
		return ERR_BUG;
	}

	return OK;
}

Error copy_libs(const String &output_dir, const HashMap<String, SharedObject> &lib_paths) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	for (const auto &E : lib_paths) {
		const auto &so = E.value;
		String dest_path = output_dir.path_join(so.path.replace("res://", ""));
		Error err = gdre::ensure_dir(dest_path.get_base_dir());
		ERR_FAIL_COND_V_MSG(err, ERR_FILE_CANT_WRITE, "Failed to create directory for library " + dest_path);
		if (da->file_exists(E.key)) {
			da->copy(E.key, dest_path);
		} else {
			da->copy_dir(E.key, dest_path);
		}
	}
	return OK;
}

String find_godotsteam_url_for_dll(const String &path) {
	// hash the file/dir
	auto da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String md5_hash;
	if (da->file_exists(path)) {
		md5_hash = FileAccess::get_md5(path);
	} else {
		md5_hash = gdre::get_md5_for_dir(path, true);
	}
	for (int i = 0; i < godotsteam_versions_count; i++) {
		for (int j = 0; j < godotsteam_versions[i].bins.size(); j++) {
			if (godotsteam_versions[i].bins[j].md5 == md5_hash) {
				return godotsteam_versions[i].url;
			}
		}
	}
	return "";
}
String find_compatible_url_for_dll(const String &path) {
	// Failed to find exact hash; find one that matches the major/minor version and has the same steamworks dll
	auto parent_dir = path.get_base_dir();
	auto steam_dlls = Glob::rglob(parent_dir.path_join("**/*steam_api*"), true);
	Vector<String> steam_dll_md5s;
	for (auto &dll : steam_dlls) {
		steam_dll_md5s.push_back(FileAccess::get_md5(dll));
	}
	auto engine_version = GDRESettings::get_singleton()->get_version_string();
	auto godot_ver = GodotVer::parse(engine_version);
	String candidate_url;
	for (int i = 0; i < godotsteam_versions_count; i++) {
		auto min_ver = GodotVer::parse(godotsteam_versions[i].min_godot_version);
		auto max_ver = GodotVer::parse(godotsteam_versions[i].max_godot_version);
		if (godot_ver->patch_compatible(min_ver) || godot_ver->patch_compatible(max_ver) ||
				(godot_ver->get_major() == min_ver->get_major() &&
						(godot_ver->get_minor() >= min_ver->get_minor() && godot_ver->get_minor() <= max_ver->get_minor()))) {
			for (int j = 0; j < godotsteam_versions[i].steam_dlls.size(); j++) {
				if (steam_dll_md5s.has(godotsteam_versions[i].steam_dlls[j].md5)) {
					candidate_url = godotsteam_versions[i].url;
					break;
				}
			}
		}
	}
	return candidate_url;
}

String get_plugin_name(const Ref<ImportInfo> import_infos) {
	String rel_path = import_infos->get_import_md_path().simplify_path().replace("res://", "");
	String plugin_name;
	if (rel_path.begins_with("addons/")) {
		plugin_name = rel_path.replace("addons/", "").split("/")[0];
	} else {
		plugin_name = rel_path.get_file().get_basename();
	}
	return plugin_name;
}

Error GDExtensionExporter::unzip_and_copy_dir(const String &zip_path, const String &output_dir) {
	//append a random string
	String tmp_dir = output_dir.path_join(".tmp").path_join(String::num_uint64(OS::get_singleton()->get_unix_time() + rand()));
	ERR_FAIL_COND_V_MSG(gdre::unzip_file_to_dir(zip_path, tmp_dir) != OK, ERR_FILE_CANT_WRITE, "Failed to unzip plugin zip to " + tmp_dir);
	// copy the files to the output_dir
	Vector<String> addons = Glob::rglob(tmp_dir.path_join("**/addons"));
	if (addons.size() > 0) {
		tmp_dir = addons[0];
		if (addons.size() > 1) {
			WARN_PRINT("Found multiple addons directories in addon zip, using the first one.");
		}
	}
	auto da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	Error err = da->copy_dir(tmp_dir, output_dir);
	ERR_FAIL_COND_V_MSG(err != OK, ERR_FILE_CANT_WRITE, "Failed to copy GodotSteam files to " + output_dir);
	return OK;
}

Error handle_godotsteam(const String &dll_path, const String &output_dir, bool exact_match = true) {
	auto url = find_godotsteam_url_for_dll(dll_path);
	auto dll_file = dll_path.get_file();
	bool compat = false;
	if (url.is_empty()) {
		if (!exact_match) {
			url = find_compatible_url_for_dll(dll_path);
		}
		if (url.is_empty()) {
			WARN_PRINT("Failed to find a GodotSteam version for " + dll_file);
			return ERR_FILE_NOT_FOUND;
		}
		WARN_PRINT("Failed to find an exact GodotSteam version for " + dll_file + ", using a compatible version @ " + url);
		compat = true;
	} else {
		print_line("Found GodotSteam version for " + dll_file + " @ " + url);
	}

	// download it to the .tmp directory in the output_dir
	String tmp_dir = output_dir.path_join(".tmp");
	String zip_path = tmp_dir.path_join("godotsteam.zip");
	ERR_FAIL_COND_V_MSG(gdre::ensure_dir(tmp_dir) != OK, ERR_FILE_CANT_WRITE, "Failed to create temporary directory for GodotSteam download");
	print_line("Downloading GodotSteam from " + url);
	ERR_FAIL_COND_V_MSG(gdre::download_file_sync(url, zip_path) != OK, ERR_FILE_CANT_READ, "Failed to download GodotSteam from " + url);
	ERR_FAIL_COND_V_MSG(gdre::unzip_file_to_dir(zip_path, tmp_dir) != OK, ERR_FILE_CANT_WRITE, "Failed to unzip GodotSteam to " + tmp_dir);
	// copy the files to the output_dir
	auto da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	Error err = da->copy_dir(tmp_dir, output_dir);
	ERR_FAIL_COND_V_MSG(err != OK, ERR_FILE_CANT_WRITE, "Failed to copy GodotSteam files to " + output_dir);
	return compat ? ERR_PRINTER_ON_FIRE : OK;
}

bool libs_has_platform(const Vector<SharedObject> &libs, const String &platform) {
	for (const SharedObject &so : libs) {
		if (so.tags.has(platform)) {
			return true;
		}
	}
	return false;
}

String GDExtensionExporter::get_plugin_download(const String &output_dir, Ref<ImportInfoGDExt> import_infos) {
	String plugin_name = get_plugin_name(import_infos);
	Ref<GodotVer> godot_ver = GodotVer::parse(GDRESettings::get_singleton()->get_version_string());
	String engine_version = GDRESettings::get_singleton()->get_version_string();
	auto get_compatible_versions = [&](int asset_id) {
		auto versions = AssetLibInfoGetter::get_plugin_versions(plugin_name, asset_id);
		Vector<PluginInfo::PluginVersion> compatible_versions;
		for (auto &version : versions) {
			if (!version.is_compatible(godot_ver)) {
				continue;
			}
			compatible_versions.push_back(version);
		}
		compatible_versions.reverse();
		return compatible_versions;
	};
	auto libs = import_infos->get_libaries();
	HashMap<String, SharedObject> lib_paths;
	Error err = OK;
	err = find_libs(libs, lib_paths);
	HashMap<String, String> hashes;
	auto da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	for (auto &E : lib_paths) {
		String md5;
		if (da->file_exists(E.key)) {
			md5 = FileAccess::get_md5(E.key);
		} else {
			md5 = gdre::get_md5_for_dir(E.key, true);
		}
		if (md5.is_empty()) {
			continue;
		}
		hashes[E.key] = md5;
	}
	ERR_FAIL_COND_V_MSG(err, "", "Failed to find gdextension libraries for plugin " + import_infos->get_import_md_path());

	auto asset_ids = AssetLibInfoGetter::search_for_asset_ids(plugin_name, godot_ver->get_major());

	if (asset_ids.size() == 0) {
		return "";
	}
	const String temp_folder = output_dir.path_join(".tmp").path_join(plugin_name);
	const String relative_gdext_path = import_infos->get_import_md_path().simplify_path().replace("res://", "");
	for (int i = 0; i < asset_ids.size(); i++) {
		int asset_id = asset_ids[i];
		auto compatible_versions = get_compatible_versions(asset_id);
		if (compatible_versions.is_empty()) {
			continue;
		}
		for (auto &version : compatible_versions) {
			String url = version.download_url;
			String new_temp_foldr = temp_folder.path_join(version.version);
			String zip_path = new_temp_foldr.path_join("plugin.zip");
			Error err = gdre::download_file_sync(url, zip_path);
			if (err) {
				continue;
			}
			Ref<ZIPReader> zip;
			zip.instantiate();
			err = zip->open(zip_path);
			if (err) {
				continue;
			}
			auto files = zip->get_files();
			String gd_ext_file = "";
			HashMap<String, String> our_libs_in_zip;

			for (int i = 0; i < files.size(); i++) {
				if (files[i].contains(relative_gdext_path)) {
					gd_ext_file = files[i];
				} else {
					for (auto &E : lib_paths) {
						auto rel_path = E.value.path.replace("res://", "");
						if (files[i].contains(rel_path)) {
							our_libs_in_zip[files[i]] = E.key;
							break;
						}
					}
				}
			}
			auto close_and_remove_zip = [&]() {
				zip->close();
				da->remove(zip_path);
			};
			if (gd_ext_file.is_empty()) {
				// it's unlikely to be this particular asset if it doesn't have the gdext file, continue on to the next asset_id
				close_and_remove_zip();
				break;
			}
			if (our_libs_in_zip.is_empty()) {
				close_and_remove_zip();
				continue;
			}
			// get the gd_ext_file as a string

			auto data = zip->read_file(gd_ext_file, true);
			String gdext_str;
			gdext_str.parse_utf8((const char *)data.ptr(), data.size());
			Ref<ImportInfoGDExt> cf = memnew(ImportInfoGDExt);
			cf->load_from_string(import_infos->get_import_md_path(), gdext_str);
			if (cf->get_compatibility_minimum() != import_infos->get_compatibility_minimum() || cf->get_compatibility_maximum() != import_infos->get_compatibility_maximum()) {
				close_and_remove_zip();
				continue;
			}

			String gdext_path = new_temp_foldr.path_join("unzipped");
			err = gdre::unzip_file_to_dir(zip_path, gdext_path);
			for (auto &E : our_libs_in_zip) {
				auto &lib_path = E.key;
				auto full_path = gdext_path.path_join(lib_path);
				String md5 = FileAccess::get_md5(full_path);
				if (da->file_exists(full_path)) {
					md5 = FileAccess::get_md5(full_path);
				} else {
					md5 = gdre::get_md5_for_dir(full_path, true);
				}
				if (md5 == hashes[E.value]) {
					return zip_path;
				}
			}
			close_and_remove_zip();
		}
	}
	return "";
}

Ref<ExportReport> GDExtensionExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	// get the first name of the plugin after res://addons
	String plugin_name = get_plugin_name(import_infos);
	Ref<ExportReport> report = memnew(ExportReport(import_infos));

	if (plugin_name != "godotsteam") {
		String downloaded_zip = get_plugin_download(output_dir, import_infos);
		if (!downloaded_zip.is_empty()) {
			report->set_saved_path(downloaded_zip);
			return report;
		}
	}
	Ref<ImportInfoGDExt> iinfo = import_infos;
	String platform = OS::get_singleton()->get_name().to_lower();
	String parent_dir = GDRESettings::get_singleton()->get_pack_path().get_base_dir();

	auto libs = iinfo->get_libaries();
	HashMap<String, SharedObject> lib_paths;
	Error err = OK;
	err = find_libs(libs, lib_paths);
	GDExt_ERR_FAIL_COND_V_MSG(err, report, "Failed to find gdextension libraries for plugin " + import_infos->get_import_md_path());
	auto deps = iinfo->get_dependencies();
	HashMap<String, SharedObject> dep_paths;
	bool found_deps = true;
	err = find_libs(iinfo->get_dependencies(), dep_paths);
	if (err) {
		found_deps = false;
		err = OK;
	}
	bool downloaded_plugin = false;
	if (plugin_name == "godotsteam") {
		for (const auto &E : lib_paths) {
			err = handle_godotsteam(E.key, output_dir, false);
			if (!err || err == ERR_PRINTER_ON_FIRE) {
				downloaded_plugin = true;
				break;
			}
		}
	}
	if (!downloaded_plugin) {
		err = copy_libs(output_dir, lib_paths);
		GDExt_ERR_FAIL_COND_V_MSG(err, report, "Failed to copy gdextension libraries for plugin " + import_infos->get_import_md_path());
		err = copy_libs(output_dir, dep_paths);
		GDExt_ERR_FAIL_COND_V_MSG(err, report, "Failed to copy gdextension dependencies for plugin " + import_infos->get_import_md_path());
		bool found_our_platform = false;
		for (const auto &E : lib_paths) {
			const auto &so = E.value;
			if (so.tags.has(platform)) {
				found_our_platform = true;
				break;
			}
		}
		if (!found_our_platform || (!found_deps && libs_has_platform(deps, platform))) {
			report->set_message(plugin_name);
		}
	}
	return report;
}

void GDExtensionExporter::get_handled_types(List<String> *out) const {
}

void GDExtensionExporter::get_handled_importers(List<String> *out) const {
	out->push_back("gdextension");
	out->push_back("gdnative");
}