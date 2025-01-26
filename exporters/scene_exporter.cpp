#include "scene_exporter.h"

#include "compat/resource_loader_compat.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "exporters/export_report.h"
#include "external/tinygltf/tiny_gltf.h"
#include "modules/gltf/gltf_document.h"
#include "utility/common.h"
#include "utility/gdre_logger.h"
#include "utility/gdre_settings.h"

#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "scene/resources/packed_scene.h"

struct dep_info {
	ResourceUID::ID uid = ResourceUID::INVALID_ID;
	String dep;
	String remap;
	String type;
	bool exists = true;
};

#define MAX_DEPTH 256
void get_deps_recursive(const String &p_path, HashMap<String, dep_info> &r_deps, int depth = 0) {
	if (depth > MAX_DEPTH) {
		ERR_PRINT("Dependency recursion depth exceeded.");
		return;
	}
	List<String> deps;
	ResourceCompatLoader::get_dependencies(p_path, &deps, true);
	for (const String &dep : deps) {
		if (!r_deps.has(dep)) {
			r_deps[dep] = dep_info{};
			dep_info &info = r_deps[dep];
			auto splits = dep.split("::");
			if (splits.size() == 3) {
				// If it has a UID, UID is first, followed by type, then fallback path
				info.uid = splits[0].is_empty() ? ResourceUID::INVALID_ID : ResourceUID::get_singleton()->text_to_id(splits[0]);
				info.type = splits[1];
				info.dep = splits[2];
				info.remap = GDRESettings::get_singleton()->get_mapped_path(info.dep);
				auto thingy = GDRESettings::get_singleton()->get_mapped_path(splits[0]);
				if (!FileAccess::exists(info.remap)) {
					if (FileAccess::exists(info.dep)) {
						info.remap = info.dep;
					} else {
						// If the remap doesn't exist, try to find the remap in the UID system
						auto mapped_path = info.uid != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(info.uid) ? ResourceUID::get_singleton()->get_id_path(info.uid) : "";
						if (mapped_path.is_empty() || !FileAccess::exists(mapped_path)) {
							mapped_path = GDRESettings::get_singleton()->get_mapped_path(mapped_path);
							if (mapped_path.is_empty() || !FileAccess::exists(mapped_path)) {
								info.exists = false;
								continue;
							}
						}
						info.remap = mapped_path;
					}
				}

			} else {
				// otherwise, it's path followed by type
				info.dep = splits[0];
				info.type = splits[1];
			}
			if (info.remap.is_empty()) {
				info.remap = GDRESettings::get_singleton()->get_mapped_path(info.dep);
			}
			if (FileAccess::exists(info.remap)) {
				get_deps_recursive(info.remap, r_deps, depth + 1);
			} else {
				info.exists = false;
			}
		}
	}
}

bool SceneExporter::using_threaded_load() const {
	// If the scenes are being exported using the worker task pool, we can't use threaded load
	return !supports_multithread();
}

Error load_model(const String &p_filename, tinygltf::Model &model, String &r_error) {
	tinygltf::TinyGLTF loader;
	std::string filename = p_filename.utf8().get_data();
	std::string error;
	std::string warning;
	bool state = loader.LoadBinaryFromFile(&model, &error, &warning, filename);
	ERR_FAIL_COND_V_MSG(!state, ERR_FILE_CANT_READ, vformat("Failed to load GLTF file: %s", error.c_str()));
	if (error.size() > 0) { // validation errors, ignore for right now
		r_error.parse_utf8(error.c_str());
	}
	return OK;
}

Error save_model(const String &p_filename, const tinygltf::Model &model) {
	tinygltf::TinyGLTF loader;
	std::string filename = p_filename.utf8().get_data();
	bool state = loader.WriteGltfSceneToFile(&model, filename, true, true, false, true);
	ERR_FAIL_COND_V_MSG(!state, ERR_FILE_CANT_WRITE, vformat("Failed to save GLTF file!"));
	return OK;
}

Error SceneExporter::_export_file(const String &p_dest_path, const String &p_src_path, Ref<ExportReport> p_report) {
	String dest_ext = p_dest_path.get_extension().to_lower();
	if (dest_ext == "escn" || dest_ext == "tscn") {
		return ResourceCompatLoader::to_text(p_src_path, p_dest_path);
	} else if (dest_ext != "glb") {
		ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Only .escn, .tscn, and .glb formats are supported for export.");
	}
	Vector<uint64_t> texture_uids;
	Error err = OK;
	bool has_script = false;
	bool has_shader = false;
	bool has_external_animation = false;
	bool has_external_materials = false;
	// bool has_external_textures = false;
	bool has_external_meshes = false;

	bool set_all_externals = false;
	List<String> get_deps;
	// We need to preload any Texture resources that are used by the scene with our own loader
	HashMap<String, dep_info> get_deps_map;
	HashSet<String> need_to_be_updated;
	HashSet<String> external_deps_updated;
	Vector<String> error_messages;
	auto append_error_messages = [&](Error err, const String &err_msg = "") {
		String step;
		switch (err) {
			case ERR_FILE_MISSING_DEPENDENCIES:
				step = "dependency resolution";
				break;
			case ERR_FILE_CANT_OPEN:
				step = "load";
				break;
			case ERR_COMPILATION_FAILED:
				step = "appending to GLTF document";
				break;
			case ERR_FILE_CANT_WRITE:
				step = "serializing GLTF document";
				break;
			case ERR_PRINTER_ON_FIRE:
				step = "GLTF export";
				break;
			case ERR_FILE_CORRUPT:
				step = "GLTF validation";
				break;
			default:
				step = "unknown";
				break;
		}
		String desc;
		if (has_script && has_shader) {
			desc = "scripts and shaders";
		} else if (has_script) {
			desc = "scripts";
		} else if (has_shader) {
			desc = "shaders";
		}
		auto err_message = vformat("Errors during %s", step);
		if (!err_msg.is_empty()) {
			err_message += ":\n  " + err_msg;
		}
		if (!desc.is_empty()) {
			err_message += "\n  Scene had " + desc;
		}

		if (p_report.is_valid()) {
			auto message = err_message + "\n";
			Vector<String> message_detail;
			message_detail.append("Dependencies:");
			for (const auto &E : get_deps_map) {
				message_detail.append(vformat("  %s -> %s, exists: %s", E.key, E.value.remap, E.value.exists ? "yes" : "no"));
			}
			p_report->set_message(message);
			p_report->append_message_detail(message_detail);
			p_report->set_error(err);
			error_messages.append_array(supports_multithread() ? GDRELogger::get_thread_errors() : GDRELogger::get_errors());
			p_report->append_error_messages(error_messages);
		}

		return vformat("Failed to export scene %s:\n  %s", p_src_path, err_message);
	};

#define GDRE_SCN_EXP_FAIL_V_MSG(err, msg)                                                    \
	{                                                                                        \
		ERR_PRINT(append_error_messages(err, msg));                                          \
		supports_multithread() ? GDRELogger::get_thread_errors() : GDRELogger::get_errors(); \
		return err;                                                                          \
	}

#define GDRE_SCN_EXP_FAIL_COND_V_MSG(cond, err, msg) \
	if (unlikely(cond)) {                            \
		GDRE_SCN_EXP_FAIL_V_MSG(err, msg);           \
	}

	{
		get_deps_recursive(p_src_path, get_deps_map);

		Vector<Ref<Resource>> textures;

		for (auto &E : get_deps_map) {
			dep_info &info = E.value;
			if (info.type == "Script") {
				has_script = true;
			} else if (info.dep.get_extension().to_lower().contains("shader")) {
				has_shader = true;
			} else {
				if (info.type.contains("Animation")) {
					has_external_animation = true;
					need_to_be_updated.insert(info.dep);
				} else if (info.type.contains("Material")) {
					has_external_materials = true;
					need_to_be_updated.insert(info.dep);
				} else if (info.type.contains("Texture")) {
					// has_external_textures = true;
					need_to_be_updated.insert(info.dep);
				} else if (info.type.contains("Mesh")) {
					has_external_meshes = true;
					need_to_be_updated.insert(info.dep);
				}
			}
		}
		// Don't need this right now, we just instance shader to a missing resource
		// If GLTF exporter somehow starts making use of them, we'll have to do this
		// bool is_default_gltf_load = ResourceCompatLoader::is_default_gltf_load();
		// if (has_shader) {
		// 	print_line("This scene has shaders, which may not be compatible with the exporter.");
		// 	// if it has a shader, we have to set gltf_load to false and do a real load on the textures, otherwise shaders will not be applied to the textures
		// 	ResourceCompatLoader::set_default_gltf_load(false);
		// }
		ResourceCompatLoader::set_default_gltf_load(false);
		auto set_cache_res = [&](const dep_info &info, Ref<Resource> texture, bool force_replace) {
			if (texture.is_null() || (!force_replace && ResourceCache::get_ref(info.dep).is_valid())) {
				return;
			}
#ifdef TOOLS_ENABLED
			texture->set_import_path(info.remap);
#endif
			// reset the path cache, then set the path so it loads it into cache.
			texture->set_path_cache("");
			texture->set_path(info.dep, true);
			textures.push_back(texture);
		};
		for (auto &E : get_deps_map) {
			dep_info &info = E.value;
			// Never set Script or Shader, they're not used by the GLTF writer and cause errors
			if (info.type == "Script" || info.type == "Shader") {
				auto texture = CompatFormatLoader::create_missing_external_resource(info.dep, info.type, info.uid, "");
				set_cache_res(info, texture, false);
				continue;
			}
			if (!FileAccess::exists(info.remap) && !FileAccess::exists(info.dep)) {
				GDRE_SCN_EXP_FAIL_V_MSG(ERR_FILE_MISSING_DEPENDENCIES,
						vformat("Dependency %s -> %s does not exist.", info.dep, info.remap));
			} else if (info.uid != ResourceUID::INVALID_ID) {
				if (!ResourceUID::get_singleton()->has_id(info.uid)) {
					ResourceUID::get_singleton()->add_id(info.uid, info.remap);
					texture_uids.push_back(info.uid);
				}
				continue;
			}

			if (info.dep != info.remap) {
				auto texture = ResourceCompatLoader::custom_load(
						info.remap, "",
						ResourceCompatLoader::get_default_load_type(),
						&err,
						using_threaded_load(),
						ResourceFormatLoader::CACHE_MODE_REUSE);
				if (err || texture.is_null()) {
					GDRE_SCN_EXP_FAIL_V_MSG(ERR_FILE_MISSING_DEPENDENCIES,
							vformat("Dependency %s:%s failed to load.", info.dep, info.remap));
				}
				set_cache_res(info, texture, false);
			}
		}
		Vector<String> id_to_texture_path;
		Vector<Pair<String, String>> id_to_material_path;
		Vector<Pair<String, String>> id_to_meshes_path;

		auto errors_before = using_threaded_load() ? GDRELogger::get_error_count() : GDRELogger::get_thread_error_count();
		auto _export_scene = [&]() {
			Error err;
			auto mode_type = ResourceCompatLoader::get_default_load_type();
			Ref<PackedScene> scene;
			// For some reason, scenes with meshes fail to load without the load done by ResourceLoader::load, possibly due to notification shenanigans.
			if (ResourceCompatLoader::is_globally_available() && using_threaded_load()) {
				scene = ResourceLoader::load(p_src_path, "PackedScene", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
			} else {
				scene = ResourceCompatLoader::custom_load(p_src_path, "PackedScene", mode_type, &err, using_threaded_load(), ResourceFormatLoader::CACHE_MODE_REUSE);
			}
			ERR_FAIL_COND_V_MSG(err, ERR_FILE_CANT_READ, "Failed to load scene " + p_src_path);
			err = gdre::ensure_dir(p_dest_path.get_base_dir());
			Node *root;
			auto pop_res_path_vec = [](Array arr, Vector<String> &paths) {
				paths.clear();
				paths.resize(arr.size());
				for (int i = 0; i < arr.size(); i++) {
					Ref<Resource> res = arr[i];
					if (res.is_null()) {
						paths.write[i] = "";
					} else {
						paths.write[i] = res->get_path();
					}
				}
			};
			{
				List<String> deps;
				Ref<GLTFDocument> doc;
				doc.instantiate();
				Ref<GLTFState> state;
				state.instantiate();
				int32_t flags = 0;
				flags |= 16; // EditorSceneFormatImporter::IMPORT_USE_NAMED_SKIN_BINDS;
				root = scene->instantiate();
				err = doc->append_from_scene(root, state, flags);
				if (err) {
					memdelete(root);
					ERR_FAIL_COND_V_MSG(err, ERR_COMPILATION_FAILED, "Failed to append scene " + p_src_path + " to glTF document");
				}
				err = doc->write_to_filesystem(state, p_dest_path);

				// Need to get the names and paths for the external resources that are used in the scene
				// 1) we need to rename the resources in the saved GLTF document
				// 2) we need to update the params in the import info so that the GLTF is imported correctly
				pop_res_path_vec(state->get_images(), id_to_texture_path);
				auto meshes = state->get_meshes();
				auto materials = state->get_materials();
				for (int i = 0; i < materials.size(); i++) {
					Ref<Material> mat = materials[i];
					if (mat.is_null()) {
						id_to_material_path.push_back({ "", "" });
						continue;
					}
					auto name = mat->get_name();
					auto path = mat->get_path();
					if (name.is_empty() || path.is_empty()) {
						Dictionary compat = mat->get_meta("compat", Dictionary());
						if (compat.size() > 0) {
							path = compat["original_path"];
							if (name.is_empty()) {
								name = compat["resource_name"];
							}
						}
					}
					// name will probably be empty; we get it down below
					id_to_material_path.push_back({ name, path });
				}

				for (int i = 0; i < meshes.size(); i++) {
					Ref<GLTFMesh> gltf_mesh = meshes[i];
					if (gltf_mesh.is_null()) {
						id_to_meshes_path.push_back({ "", "" });
						continue;
					}
					auto name = gltf_mesh->get_original_name();
					auto mesh = gltf_mesh->get_mesh();
					Dictionary compat = mesh->get_meta("compat", Dictionary());
					String path;
					if (compat.size() > 0) {
						path = compat["original_path"];
						if (name.is_empty()) {
							name = compat["resource_name"];
						}
					}

					id_to_meshes_path.push_back({ name, path });
				}

				// We have to go through each surface in the mesh to find the names for external materials, since they're
				// not saved to the material resources, and the surface names aren't present in the meshes in the gltfdoc either
				// TODO: PR GLTFdocument fix for this
				TypedArray<Node> nodes = root->find_children("*", "MeshInstance3D");
				for (int32_t node_i = 0; node_i < nodes.size(); node_i++) {
					MeshInstance3D *mesh_instance = cast_to<MeshInstance3D>(nodes[node_i]);
					if (mesh_instance == nullptr) {
						continue;
					}
					// only ArrayMeshes have surface names
					Ref<ArrayMesh> mesh = mesh_instance->get_mesh();
					if (mesh.is_null()) {
						continue;
					}
					for (int j = 0; j < mesh->get_surface_count(); j++) {
						auto surf_mat = mesh->surface_get_material(j);
						if (surf_mat.is_null()) {
							continue;
						}
						auto surf_mat_path = surf_mat->get_path();
						if (!surf_mat_path.is_empty()) {
							for (auto &E : id_to_material_path) {
								if (E.second == surf_mat_path) {
									E.first = mesh->surface_get_name(j);
								}
							}
						}
					}
				}
			}
			memdelete(root);
			ERR_FAIL_COND_V_MSG(err, ERR_FILE_CANT_WRITE, "Failed to write glTF document to " + p_dest_path);
			tinygltf::Model model;
			String error_string;
			err = load_model(p_dest_path, model, error_string);
			ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Failed to load glTF document from " + p_dest_path + ": " + error_string);
			// rename the images
			String scene_name;
			auto iinfo = p_report->get_import_info();
			if (p_report.is_valid()) {
				scene_name = iinfo->get_source_file().get_file().get_basename();
			} else {
				scene_name = p_src_path.get_file().get_slice(".", 0);
			}

			// TODO: handle Godot version <= 4.2 image naming scheme?
			auto get_res_name = [scene_name](const String &path) {
				return path.get_file().get_basename().trim_prefix(scene_name + "_");
			};

			for (int i = 0; i < model.images.size(); i++) {
				ERR_FAIL_COND_V(i > id_to_texture_path.size(), ERR_FILE_CORRUPT);
				String img_path = id_to_texture_path[i];
				if (img_path.is_empty() || img_path.get_file().contains("::")) {
					continue;
				}

				String img_name = get_res_name(img_path);
				auto &gltf_image = model.images[i];
				gltf_image.name = img_name.utf8().get_data();
				external_deps_updated.insert(img_path);
			}

			for (int i = 0; i < model.materials.size(); i++) {
				ERR_FAIL_COND_V(i > id_to_material_path.size(), ERR_FILE_CORRUPT);
				String img_name = id_to_material_path[i].first;
				if (img_name.is_empty() || id_to_material_path[i].second.is_empty()) {
					continue;
				}
				auto &gltf_image = model.materials[i];
				gltf_image.name = img_name.utf8().get_data();
			}

			for (int i = 0; i < model.meshes.size(); i++) {
				ERR_FAIL_COND_V(i > id_to_meshes_path.size(), ERR_FILE_CORRUPT);
				String img_name = id_to_meshes_path[i].first;
				if (img_name.is_empty() || id_to_meshes_path[i].second.is_empty()) {
					continue;
				}
				auto &gltf_image = model.meshes[i];
				gltf_image.name = img_name.utf8().get_data();
			}

			// save the model
			return save_model(p_dest_path, model);
		};
		err = _export_scene();
		auto errors_after = using_threaded_load() ? GDRELogger::get_error_count() : GDRELogger::get_thread_error_count();
		if (err == OK && errors_after > errors_before) {
			err = ERR_PRINTER_ON_FIRE;
		}
		auto iinfo = p_report.is_valid() ? p_report->get_import_info() : Ref<ImportInfo>();

		// Godot 4.2 and above blow out the import params, so we need to update them to point to the external resources.
		Dictionary _subresources_dict = Dictionary();
		if (iinfo.is_valid() && iinfo->has_param("_subresources")) {
			_subresources_dict = iinfo->get_param("_subresources");
		}

		if ((has_external_animation || has_external_meshes || has_external_materials) && iinfo.is_valid() && _subresources_dict.is_empty()) {
			if (has_external_animation) {
				if (!_subresources_dict.has("animations")) {
					_subresources_dict["animations"] = Dictionary();
				}
				Dictionary animations_dict = _subresources_dict["animations"];
				for (auto &E : get_deps_map) {
					dep_info &info = E.value;
					if (info.type.contains("Animation")) {
						Error res_load_err;
						auto res = ResourceCompatLoader::fake_load(info.remap, "", &res_load_err);
						if (res.is_null() || res_load_err) {
							set_all_externals = false;
							ERR_CONTINUE(true);
						}
						auto name = res->get_name();
						animations_dict[name] = Dictionary();
						// "save_to_file/enabled": true,
						// "save_to_file/keep_custom_tracks": true,
						// "save_to_file/path": "res://models/Enemies/cultist-shoot-anim.res",
						Dictionary subres = animations_dict[name];
						subres["save_to_file/enabled"] = true;
						subres["save_to_file/keep_custom_tracks"] = true;
						subres["save_to_file/path"] = info.dep;
						external_deps_updated.insert(info.dep);
					}
				}
			}
			if (has_external_meshes) {
				if (!_subresources_dict.has("meshes")) {
					_subresources_dict["meshes"] = Dictionary();
				}

				Dictionary mesh_Dict = _subresources_dict["meshes"];
				for (auto &E : id_to_meshes_path) {
					auto name = E.first;
					auto path = E.second;
					if (name.is_empty() || path.is_empty() || path.get_file().contains("::") || mesh_Dict.has(name)) {
						continue;
					}
					// "save_to_file/enabled": true,
					// "save_to_file/path": "res://models/Enemies/cultist-shoot-anim.res",
					mesh_Dict[name] = Dictionary();
					Dictionary subres = mesh_Dict[name];
					subres["save_to_file/enabled"] = true;
					subres["save_to_file/path"] = path;
					external_deps_updated.insert(path);
				}
			}
			if (has_external_materials) {
				if (!_subresources_dict.has("materials")) {
					_subresources_dict["materials"] = Dictionary();
				}

				Dictionary mat_Dict = _subresources_dict["materials"];
				for (auto &E : id_to_material_path) {
					auto name = E.first;
					auto path = E.second;
					if (name.is_empty() || path.is_empty() || path.get_file().contains("::") || mat_Dict.has(name)) {
						continue;
					}
					mat_Dict[name] = Dictionary();
					Dictionary subres = mat_Dict[name];
					subres["use_external/enabled"] = true;
					subres["use_external/path"] = path;
					external_deps_updated.insert(path);
				}
			}

			iinfo->set_param("_subresources", _subresources_dict);
		}
	}

	// remove the UIDs that we added that didn't exist before
	for (uint64_t id : texture_uids) {
		ResourceUID::get_singleton()->remove_id(id);
	}

	set_all_externals = external_deps_updated.size() == need_to_be_updated.size();
	// GLTF Exporter has issues with custom animations and throws errors;
	// if we've set all the external resources (including custom animations),
	// then this isn't an error.
	if (err == ERR_PRINTER_ON_FIRE && has_external_animation && set_all_externals) {
		err = OK;
		error_messages.append_array(supports_multithread() ? GDRELogger::get_thread_errors() : GDRELogger::get_errors());
		for (auto &msg : error_messages) {
			auto message = msg.strip_edges();
			if (message.to_lower().contains("animated track") || message.begins_with("at:") || message.begins_with("WARNING:")) {
				continue;
			}
			err = ERR_PRINTER_ON_FIRE;
			break;
		}
	}
	GDRE_SCN_EXP_FAIL_COND_V_MSG(err, err, "");
	if (!set_all_externals || has_script || has_shader) {
		err = ERR_PRINTER_ON_FIRE;
	}
	return OK;
}

Error SceneExporter::export_file(const String &p_dest_path, const String &p_src_path) {
	String ext = p_dest_path.get_extension().to_lower();
	if (ext != "escn" && ext != "tscn") {
		int ver_major = get_ver_major(p_src_path);
		ERR_FAIL_COND_V_MSG(ver_major != 4, ERR_UNAVAILABLE, "Scene export for engine version " + itos(ver_major) + " is not currently supported.");
	}
	return _export_file(p_dest_path, p_src_path, nullptr);
}

Ref<ExportReport> SceneExporter::export_resource(const String &output_dir, Ref<ImportInfo> iinfo) {
	Ref<ExportReport> report = memnew(ExportReport(iinfo));

	Error err;
	Vector<uint64_t> texture_uids;
	String orignal_export_dest = iinfo->get_export_dest();
	String new_path = orignal_export_dest;
	String ext = new_path.get_extension().to_lower();
	bool to_text = ext == "escn" || ext == "tscn";
	// GLTF export can result in inaccurate models
	// save it under .assets, which won't be picked up for import by the godot editor
	if (!to_text) {
		new_path = new_path.replace("res://", "res://.assets/");
		// we only export glbs
		if (ext != "glb") {
			new_path = new_path.get_basename() + ".glb";
		}
	}
	iinfo->set_export_dest(new_path);
	String dest_path = output_dir.path_join(new_path.replace("res://", ""));
	if (!to_text && iinfo->get_ver_major() != 4) {
		err = ERR_UNAVAILABLE;
		report->set_message("Scene export for engine version " + itos(iinfo->get_ver_major()) + " is not currently supported.");
		report->set_unsupported_format_type(itos(iinfo->get_ver_major()) + ".x PackedScene");
	} else {
		report->set_new_source_path(new_path);
		err = _export_file(dest_path, iinfo->get_path(), report);
	}
	if (err == ERR_PRINTER_ON_FIRE) {
		report->set_saved_path(dest_path);
	} else if (err == OK && !to_text) {
		// TODO: Turn this on when we feel confident that we can tell that are exporting correctly
		// TODO: do real GLTF validation
		// TODO: fix errors where some models aren't being textured?
		// move the file to the correct location
		// auto new_dest = output_dir.path_join(orignal_export_dest.replace("res://", ""));
		// auto da = DirAccess::create_for_path(new_dest.get_base_dir());
		// if (da.is_valid() && da->rename(dest_path, new_dest) == OK) {
		// 	iinfo->set_export_dest(orignal_export_dest);
		// 	report->set_new_source_path("");
		// 	report->set_saved_path(new_dest);
		// }
	}

#if DEBUG_ENABLED
	if (err && err != ERR_UNAVAILABLE) {
		// save it as a text scene so we can see what went wrong
		auto new_dest = dest_path.get_basename() + ".tscn";
		ResourceCompatLoader::to_text(iinfo->get_path(), new_dest);
	}
#endif
	report->set_error(err);
	return report; // We always save to an unoriginal path
}

bool SceneExporter::handles_import(const String &importer, const String &resource_type) const {
	return importer == "scene" || resource_type == "PackedScene";
}

void SceneExporter::get_handled_types(List<String> *out) const {
	out->push_back("PackedScene");
}

void SceneExporter::get_handled_importers(List<String> *out) const {
	out->push_back("scene");
}