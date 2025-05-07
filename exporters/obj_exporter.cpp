#include "obj_exporter.h"
#include "compat/resource_loader_compat.h"
#include "core/io/file_access.h"
#include "core/templates/hashfuncs.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "utility/common.h"
#include "utility/import_info.h"
#include <filesystem>

struct VertexKey {
	Vector3 vertex;
	Vector2 uv;
	Vector3 normal;
	bool has_uv = false;
	bool has_normal = false;

	bool operator==(const VertexKey &other) const {
		return vertex == other.vertex && (!has_uv || uv == other.uv) && (!has_normal || normal == other.normal);
	}

	bool operator<(const VertexKey &other) const {
		if (vertex != other.vertex) {
			return vertex < other.vertex;
		}
		if (has_uv != other.has_uv) {
			return has_uv < other.has_uv;
		}
		if (has_uv && uv != other.uv) {
			return uv < other.uv;
		}
		if (has_normal != other.has_normal) {
			return has_normal < other.has_normal;
		}
		if (has_normal && normal != other.normal) {
			return normal < other.normal;
		}
		return false;
	}
};

struct VertexKeyHasher {
	static uint32_t hash(const VertexKey &vk) {
		return HashMapHasherDefault::hash(vk.vertex) ^ HashMapHasherDefault::hash(vk.uv) ^ HashMapHasherDefault::hash(vk.normal);
	}
};
struct FaceVertex {
	int v_idx = -1;
	int vt_idx = -1;
	int vn_idx = -1;
};

struct Triplet {
	Vector3 v;
	Vector2 vt;
	Vector3 vn;
	Color vc;
	bool has_uv = false;
	bool has_normal = false;
	bool has_color = false;

	bool operator==(const Triplet &other) const {
		return v == other.v && (!has_uv || vt == other.vt) && (!has_normal || vn == other.vn);
	}

	bool operator<(const Triplet &other) const {
		if (v != other.v) {
			return v < other.v;
		}
		if (has_uv != other.has_uv) {
			return has_uv < other.has_uv;
		}
		if (has_uv && vt != other.vt) {
			return vt < other.vt;
		}
		if (has_normal != other.has_normal) {
			return has_normal < other.has_normal;
		}
		if (has_normal && vn != other.vn) {
			return vn < other.vn;
		}
		return false;
	}
};

struct TripletHasher {
	static uint32_t hash(const Triplet &t) {
		return HashMapHasherDefault::hash(t.v) ^ HashMapHasherDefault::hash(t.vt) ^ HashMapHasherDefault::hash(t.vn);
	}
};

Error ObjExporter::_write_mesh_to_obj(const Ref<ArrayMesh> &p_mesh, const String &p_path, const String &p_output_dir) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot open file for writing: " + p_path);

	// Write header
	f->store_line("# Exported from Godot Engine");

	// Global list of unique triplets and mapping
	Vector<Triplet> global_triplets;
	HashMap<Triplet, int, TripletHasher> triplet_to_index;
	HashMap<String, Ref<Material>> materials;

	// For each surface, store the sequence of triplet indices for each face
	Vector<Vector<int>> surface_face_triplet_indices;
	Vector<String> surface_material_names;

	// Build global triplet list and face indices
	const Color default_color = Color(1.0, 1.0, 1.0);
	for (int surf_idx = 0; surf_idx < p_mesh->get_surface_count(); surf_idx++) {
		Array arrays = p_mesh->surface_get_arrays(surf_idx);
		Vector<Vector3> surface_vertices = arrays[Mesh::ARRAY_VERTEX];
		Vector<Vector2> surface_uvs = arrays[Mesh::ARRAY_TEX_UV];
		Vector<Vector3> surface_normals = arrays[Mesh::ARRAY_NORMAL];
		Vector<Color> surface_colors = arrays[Mesh::ARRAY_COLOR];
		Vector<int> indices = arrays[Mesh::ARRAY_INDEX];

		bool has_uv = !surface_uvs.is_empty();
		bool has_normal = !surface_normals.is_empty();
		bool has_vertex_colors = !surface_colors.is_empty();

		// Material
		Ref<Material> mat = p_mesh->surface_get_material(surf_idx);
		String mat_name;
		String surface_name = p_mesh->surface_get_name(surf_idx);
		if (mat.is_valid()) {
			mat_name = mat->get_name();
			if (mat_name.is_empty() || mat_name == "Material" || mat_name.begins_with("Material.")) {
				if (!surface_name.is_empty() && surface_name != "Material" && !surface_name.begins_with("Material.")) {
					mat_name = surface_name;
				} else if (mat_name.is_empty()) {
					mat_name = vformat("Material.%03d", surf_idx);
				}
			}
			materials[mat_name] = mat;
		}
		surface_material_names.push_back(mat_name);

		Vector<int> face_triplet_indices;
		if (!indices.is_empty()) {
			for (int i = 0; i < indices.size(); i++) {
				int vi = indices[i];
				Triplet t;
				t.v = surface_vertices[vi];
				if (has_uv) {
					t.vt = surface_uvs[vi];
					t.has_uv = true;
				}
				if (has_normal) {
					t.vn = surface_normals[vi];
					t.has_normal = true;
				}
				if (!triplet_to_index.has(t)) {
					triplet_to_index[t] = global_triplets.size();
					global_triplets.push_back(t);
				}
				face_triplet_indices.push_back(triplet_to_index[t]);
			}
		} else {
			for (int vi = 0; vi < surface_vertices.size(); vi++) {
				Triplet t;
				t.v = surface_vertices[vi];
				if (has_uv) {
					t.vt = surface_uvs[vi];
					t.has_uv = true;
				}
				if (has_normal) {
					t.vn = surface_normals[vi];
					t.has_normal = true;
				}
				if (has_vertex_colors && surface_colors[vi] != default_color) {
					t.vc = surface_colors[vi];
					t.has_color = true;
				}
				if (!triplet_to_index.has(t)) {
					triplet_to_index[t] = global_triplets.size();
					global_triplets.push_back(t);
				}
				face_triplet_indices.push_back(triplet_to_index[t]);
			}
		}
		surface_face_triplet_indices.push_back(face_triplet_indices);
	}

	if (!materials.is_empty()) {
		String mtl_path = p_path.get_basename() + ".mtl";
		err = _write_materials_to_mtl(materials, mtl_path, p_output_dir);
		if (err == OK) {
			f->store_line("mtllib " + mtl_path.get_file());
		}
	}
	String mesh_name = p_mesh->get_name();
	if (mesh_name.is_empty()) {
		mesh_name = p_path.get_file().get_basename();
	}
	f->store_line("o " + mesh_name);

	// Write v, vt, vn in triplet order
	bool any_uv = false;
	bool any_normal = false;
	for (int i = 0; i < global_triplets.size(); i++) {
		const Triplet &t = global_triplets[i];
		const Vector3 &v = t.v;
		String v_line = vformat("v %.6f %.6f %.6f", v.x, v.y, v.z);
		if (t.has_color) {
			v_line += vformat(" %.6f %.6f %.6f", t.vc.r, t.vc.g, t.vc.b);
		}
		f->store_line(v_line);
		if (t.has_uv) {
			const Vector2 &uv = t.vt;
			f->store_line(vformat("vt %.6f %.6f", uv.x, 1.0 - uv.y));
			any_uv = true;
		}
		if (t.has_normal) {
			const Vector3 &n = t.vn;
			f->store_line(vformat("vn %.6f %.6f %.6f", n.x, n.y, n.z));
			any_normal = true;
		}
	}

	// Write faces per surface
	for (int surf_idx = 0; surf_idx < surface_face_triplet_indices.size(); surf_idx++) {
		const Vector<int> &face_triplet_indices = surface_face_triplet_indices[surf_idx];
		const String &mat_name = surface_material_names[surf_idx];
		if (!mat_name.is_empty()) {
			f->store_line("usemtl " + mat_name);
		}
		// Write faces (assume triangles)
		for (int i = 0; i < face_triplet_indices.size(); i += 3) {
			String face_line = "f";
			for (int k = 2; k >= 0; k--) {
				int idx = face_triplet_indices[i + k] + 1; // OBJ indices start at 1
				face_line += " ";
				face_line += itos(idx);
				if (any_uv || any_normal) {
					face_line += "/";
					if (any_uv) {
						face_line += itos(idx);
					}
					if (any_normal) {
						face_line += "/" + itos(idx);
					}
				}
			}
			f->store_line(face_line);
		}
	}

	return OK;
}

static String get_relative_path(const String &p_path, const String &p_base_path) {
	std::filesystem::path path = p_path.utf8().get_data();
	std::filesystem::path base_path = p_base_path.utf8().get_data();
	std::filesystem::path relative_path = std::filesystem::relative(path, base_path);
	return String::utf8(relative_path.string().c_str()).simplify_path();
}

Error ObjExporter::_write_materials_to_mtl(const HashMap<String, Ref<Material>> &p_materials, const String &p_path, const String &p_output_dir) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot open MTL file for writing: " + p_path);

	f->store_line("# Exported from Godot Engine");
	auto base_dir = p_path.get_base_dir();
	auto filebasename = p_path.get_file().get_basename();

	auto check_and_save_texture = [&](Ref<Texture2D> tex, const String &suffix) {
		if (tex.is_valid()) {
			String path = tex->get_path();
			bool save_texture = false;
			// if no output_dir or no path, we need to save it to disk
			if (p_output_dir.is_empty() || path.is_empty() || !path.begins_with("res://")) {
				// we need to save it to disk
				save_texture = true;
			} else {
				// if we have an output_dir, check if the path is relative to the output_dir
				String local_dir = p_output_dir.path_join(path.trim_prefix("res://"));
				String relative_path = get_relative_path(local_dir, base_dir);
				if (relative_path.is_empty()) {
					save_texture = true;
				} else {
					path = relative_path;
				}
			}
			if (save_texture) {
				String name = tex->get_name();
				if (name.is_empty()) {
					name = filebasename + "_" + suffix + ".png";
				}
				gdre::ensure_dir(base_dir);
				tex->get_image()->save_png(base_dir.path_join(name));
				path = name;
			}
			return path;
		}
		return String();
	};

	for (const KeyValue<String, Ref<Material>> &E : p_materials) {
		const String &name = E.key;
		Ref<StandardMaterial3D> mat = E.value;

		if (mat.is_valid()) {
			f->store_line("\nnewmtl " + name);

			Color albedo = mat->get_albedo();
			// if (mat->get_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR)) {
			// 	albedo = albedo.linear_to_srgb();
			// }
			f->store_line(vformat("Kd %.6f %.6f %.6f", albedo.r, albedo.g, albedo.b));

			float metallic = mat->get_metallic();
			f->store_line(vformat("Ks %.6f %.6f %.6f", metallic, metallic, metallic));

			// float roughness = mat->get_roughness();
			// f->store_line(vformat("Ns %.6f", (1.0 - roughness) * 1000.0));

			float alpha = mat->get_albedo().a;
			if (alpha < 1.0) {
				f->store_line(vformat("d %.6f", alpha));
			}

			//sharpness
			float sharpness = 1.0 - mat->get_roughness();
			if (sharpness != 0.0) {
				f->store_line(vformat("sharpness %d", (int)(sharpness * 1000)));
			}

			// Handle textures if present
			Ref<Texture2D> tex = mat->get_texture(StandardMaterial3D::TEXTURE_ALBEDO);
			String path = check_and_save_texture(tex, "albedo");
			if (!path.is_empty()) {
				f->store_line("map_Kd " + get_relative_path(path, base_dir));
			}
			Ref<Texture2D> met_tex = mat->get_texture(StandardMaterial3D::TEXTURE_METALLIC);
			path = check_and_save_texture(met_tex, "metallic");
			if (!path.is_empty()) {
				f->store_line("map_Ks " + get_relative_path(path, base_dir));
			}
			Ref<Texture2D> rough_tex = mat->get_texture(StandardMaterial3D::TEXTURE_ROUGHNESS);
			path = check_and_save_texture(rough_tex, "roughness");
			if (!path.is_empty()) {
				f->store_line("map_Ns " + get_relative_path(path, base_dir));
			}
			Ref<Texture2D> norm_tex = mat->get_texture(StandardMaterial3D::TEXTURE_NORMAL);
			path = check_and_save_texture(norm_tex, "normal");
			if (!path.is_empty()) {
				f->store_line("map_bump " + get_relative_path(path, base_dir));
			}
		}
	}

	return OK;
}

Error ObjExporter::export_file(const String &p_out_path, const String &p_source_path) {
	Error err;
	Ref<ArrayMesh> mesh = ResourceCompatLoader::custom_load(p_source_path, "ArrayMesh", ResourceInfo::LoadType::REAL_LOAD, &err, false, ResourceFormatLoader::CACHE_MODE_IGNORE);
	ERR_FAIL_COND_V_MSG(mesh.is_null(), ERR_FILE_UNRECOGNIZED, "Not a valid mesh resource: " + p_source_path);

	return _write_mesh_to_obj(mesh, p_out_path, "");
}

void ObjExporter::get_handled_types(List<String> *r_types) const {
	r_types->push_back("ArrayMesh");
}

void ObjExporter::get_handled_importers(List<String> *r_importers) const {
	r_importers->push_back("wavefront_obj");
}

void ObjExporter::_bind_methods() {
	// No methods to bind in this implementation
}

Ref<ExportReport> ObjExporter::export_resource(const String &p_output_dir, Ref<ImportInfo> p_import_info) {
	String src_path = p_import_info->get_path();
	String dst_path = p_output_dir.path_join(p_import_info->get_export_dest().replace("res://", ""));

	// Create the export report
	Ref<ExportReport> report = memnew(ExportReport(p_import_info));

	// Load the mesh
	Error err;
	Ref<ArrayMesh> mesh = ResourceCompatLoader::custom_load(src_path, "ArrayMesh", ResourceInfo::LoadType::REAL_LOAD, &err, false, ResourceFormatLoader::CACHE_MODE_IGNORE);
	if (mesh.is_null()) {
		report->set_error(ERR_FILE_UNRECOGNIZED);
		report->set_message("Not a valid mesh resource: " + src_path);
		return report;
	}

	// Ensure the output directory exists
	err = gdre::ensure_dir(dst_path.get_base_dir());
	if (err != OK) {
		report->set_error(err);
		report->set_message("Failed to create output directory for: " + dst_path);
		return report;
	}

	// Export the mesh
	err = _write_mesh_to_obj(mesh, dst_path, p_output_dir);
	// If we generated an MTL file, add it to the report
	String mtl_path = dst_path.get_basename() + ".mtl";
	if (FileAccess::exists(mtl_path)) {
		Dictionary extra_info;
		extra_info["mtl_path"] = mtl_path;
		report->set_extra_info(extra_info);
	}

	if (err != OK) {
		report->set_error(err);
		report->set_message("Failed to export mesh: " + src_path);
		return report;
	}

	// Set success information
	report->set_saved_path(dst_path);
	print_verbose("Converted " + src_path + " to " + dst_path);

	return report;
}

bool ObjExporter::supports_multithread() const {
	return false;
}
