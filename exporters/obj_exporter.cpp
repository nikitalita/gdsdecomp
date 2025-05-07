#include "obj_exporter.h"
#include "compat/resource_loader_compat.h"
#include "core/io/file_access.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "utility/common.h"
#include "utility/import_info.h"

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
		if (vertex != other.vertex)
			return vertex < other.vertex;
		if (has_uv != other.has_uv)
			return has_uv < other.has_uv;
		if (has_uv && uv != other.uv)
			return uv < other.uv;
		if (has_normal != other.has_normal)
			return has_normal < other.has_normal;
		if (has_normal && normal != other.normal)
			return normal < other.normal;
		return false;
	}
};

Error ObjExporter::_write_mesh_to_obj(const Ref<ArrayMesh> &p_mesh, const String &p_path) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot open file for writing: " + p_path);

	// Write header
	f->store_line("# Exported from Godot Engine");

	// Global lists and mapping
	Vector<VertexKey> global_vertex_keys;
	HashMap<VertexKey, int> vertex_key_to_index;
	HashMap<String, Ref<Material>> materials;

	// For each surface, store the sequence of global indices for each face
	Vector<Vector<int>> surface_face_indices;
	Vector<String> surface_material_names;

	// Build global lists and face indices
	for (int surf_idx = 0; surf_idx < p_mesh->get_surface_count(); surf_idx++) {
		Array arrays = p_mesh->surface_get_arrays(surf_idx);
		Vector<Vector3> surface_vertices = arrays[Mesh::ARRAY_VERTEX];
		Vector<Vector2> surface_uvs = arrays[Mesh::ARRAY_TEX_UV];
		Vector<Vector3> surface_normals = arrays[Mesh::ARRAY_NORMAL];
		Vector<int> indices = arrays[Mesh::ARRAY_INDEX];

		bool has_uv = !surface_uvs.is_empty();
		bool has_normal = !surface_normals.is_empty();

		// Material
		Ref<Material> mat = p_mesh->surface_get_material(surf_idx);
		String mat_name;
		if (mat.is_valid()) {
			mat_name = mat->get_name();
			if (mat_name.is_empty()) {
				mat_name = "material_" + itos(surf_idx);
			}
			materials[mat_name] = mat;
		}
		surface_material_names.push_back(mat_name);

		Vector<int> face_indices;
		// For each face vertex, build VertexKey and assign global index
		if (!indices.is_empty()) {
			for (int i = 0; i < indices.size(); i++) {
				int vi = indices[i];
				VertexKey key;
				key.vertex = surface_vertices[vi];
				if (has_uv) {
					key.uv = surface_uvs[vi];
					key.has_uv = true;
				}
				if (has_normal) {
					key.normal = surface_normals[vi];
					key.has_normal = true;
				}
				if (!vertex_key_to_index.has(key)) {
					vertex_key_to_index[key] = global_vertex_keys.size();
					global_vertex_keys.push_back(key);
				}
				face_indices.push_back(vertex_key_to_index[key]);
			}
		} else {
			// No indices, use sequential
			for (int vi = 0; vi < surface_vertices.size(); vi++) {
				VertexKey key;
				key.vertex = surface_vertices[vi];
				if (has_uv) {
					key.uv = surface_uvs[vi];
					key.has_uv = true;
				}
				if (has_normal) {
					key.normal = surface_normals[vi];
					key.has_normal = true;
				}
				if (!vertex_key_to_index.has(key)) {
					vertex_key_to_index[key] = global_vertex_keys.size();
					global_vertex_keys.push_back(key);
				}
				face_indices.push_back(vertex_key_to_index[key]);
			}
		}
		surface_face_indices.push_back(face_indices);
	}

	// Write global vertex data
	for (int i = 0; i < global_vertex_keys.size(); i++) {
		const VertexKey &key = global_vertex_keys[i];
		const Vector3 &v = key.vertex;
		f->store_line(vformat("v %.6f %.6f %.6f", v.x, v.y, v.z));
	}
	// Write global uvs
	bool any_uv = false;
	for (int i = 0; i < global_vertex_keys.size(); i++) {
		if (global_vertex_keys[i].has_uv) {
			const Vector2 &uv = global_vertex_keys[i].uv;
			f->store_line(vformat("vt %.6f %.6f", uv.x, 1.0 - uv.y));
			any_uv = true;
		}
	}
	// Write global normals
	bool any_normal = false;
	for (int i = 0; i < global_vertex_keys.size(); i++) {
		if (global_vertex_keys[i].has_normal) {
			const Vector3 &n = global_vertex_keys[i].normal;
			f->store_line(vformat("vn %.6f %.6f %.6f", n.x, n.y, n.z));
			any_normal = true;
		}
	}

	// Write faces per surface
	int face_cursor = 0;
	for (int surf_idx = 0; surf_idx < surface_face_indices.size(); surf_idx++) {
		const Vector<int> &face_indices = surface_face_indices[surf_idx];
		const String &mat_name = surface_material_names[surf_idx];
		if (!mat_name.is_empty()) {
			f->store_line("usemtl " + mat_name);
		}
		// Write faces (assume triangles)
		for (int i = 0; i < face_indices.size(); i += 3) {
			String face_line = "f";
			for (int k = 0; k < 3; k++) {
				int idx = face_indices[i + k] + 1; // OBJ indices start at 1
				face_line += " ";
				face_line += itos(idx);
				if (any_uv || any_normal) {
					face_line += "/";
					if (any_uv)
						face_line += itos(idx);
					if (any_normal)
						face_line += "/" + itos(idx);
				}
			}
			f->store_line(face_line);
		}
	}

	// If we have materials, write the MTL file
	if (!materials.is_empty()) {
		String mtl_path = p_path.get_basename() + ".mtl";
		err = _write_materials_to_mtl(materials, mtl_path);
		if (err == OK) {
			f->store_line("mtllib " + mtl_path.get_file());
		}
	}

	return OK;
}

Error ObjExporter::_write_materials_to_mtl(const HashMap<String, Ref<Material>> &p_materials, const String &p_path) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot open MTL file for writing: " + p_path);

	f->store_line("# Exported from Godot Engine");

	for (const KeyValue<String, Ref<Material>> &E : p_materials) {
		const String &name = E.key;
		Ref<StandardMaterial3D> mat = E.value;

		if (mat.is_valid()) {
			f->store_line("\nnewmtl " + name);

			Color albedo = mat->get_albedo();
			f->store_line(vformat("Kd %.6f %.6f %.6f", albedo.r, albedo.g, albedo.b));

			float metallic = mat->get_metallic();
			f->store_line(vformat("Ks %.6f %.6f %.6f", metallic, metallic, metallic));

			float roughness = mat->get_roughness();
			f->store_line(vformat("Ns %.6f", (1.0 - roughness) * 1000.0));

			float alpha = mat->get_albedo().a;
			if (alpha < 1.0) {
				f->store_line(vformat("d %.6f", alpha));
			}

			// Handle textures if present
			Ref<Texture2D> tex = mat->get_texture(StandardMaterial3D::TEXTURE_ALBEDO);
			if (tex.is_valid()) {
				f->store_line("map_Kd " + tex->get_path().get_file());
			}
		}
	}

	return OK;
}

Error ObjExporter::export_file(const String &p_out_path, const String &p_source_path) {
	Error err;
	Ref<ArrayMesh> mesh = ResourceCompatLoader::custom_load(p_source_path, "ArrayMesh", ResourceInfo::LoadType::REAL_LOAD, &err, false, ResourceFormatLoader::CACHE_MODE_IGNORE);
	ERR_FAIL_COND_V_MSG(mesh.is_null(), ERR_FILE_UNRECOGNIZED, "Not a valid mesh resource: " + p_source_path);

	return _write_mesh_to_obj(mesh, p_out_path);
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
	String dst_path = p_output_dir.path_join(p_import_info->get_export_dest().replace("res://", ".assets/")); // TODO: REMOVE THIS

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
	err = _write_mesh_to_obj(mesh, dst_path);
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
