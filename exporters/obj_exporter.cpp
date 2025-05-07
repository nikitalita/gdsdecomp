#include "obj_exporter.h"
#include "compat/resource_loader_compat.h"
#include "core/io/file_access.h"
#include "core/templates/hashfuncs.h"
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

Error ObjExporter::_write_mesh_to_obj(const Ref<ArrayMesh> &p_mesh, const String &p_path) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot open file for writing: " + p_path);

	// Write header
	f->store_line("# Exported from Godot Engine");

	// Global lists and mapping
	Vector<Vector3> global_vertices;
	Vector<Vector2> global_uvs;
	Vector<Vector3> global_normals;
	HashMap<Vector3, int, HashMapHasherDefault> vertex_to_index;
	HashMap<Vector2, int, HashMapHasherDefault> uv_to_index;
	HashMap<Vector3, int, HashMapHasherDefault> normal_to_index;
	HashMap<String, Ref<Material>> materials;

	// For each surface, store the sequence of FaceVertex for each face
	Vector<Vector<FaceVertex>> surface_face_vertices;
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

		Vector<FaceVertex> face_vertices;
		if (!indices.is_empty()) {
			for (int i = 0; i < indices.size(); i++) {
				int vi = indices[i];
				FaceVertex fv;
				// Vertex
				const Vector3 &v = surface_vertices[vi];
				if (!vertex_to_index.has(v)) {
					vertex_to_index[v] = global_vertices.size();
					global_vertices.push_back(v);
				}
				fv.v_idx = vertex_to_index[v];
				// UV
				if (has_uv) {
					const Vector2 &uv = surface_uvs[vi];
					if (!uv_to_index.has(uv)) {
						uv_to_index[uv] = global_uvs.size();
						global_uvs.push_back(uv);
					}
					fv.vt_idx = uv_to_index[uv];
				}
				// Normal
				if (has_normal) {
					const Vector3 &n = surface_normals[vi];
					if (!normal_to_index.has(n)) {
						normal_to_index[n] = global_normals.size();
						global_normals.push_back(n);
					}
					fv.vn_idx = normal_to_index[n];
				}
				face_vertices.push_back(fv);
			}
		} else {
			for (int vi = 0; vi < surface_vertices.size(); vi++) {
				FaceVertex fv;
				const Vector3 &v = surface_vertices[vi];
				if (!vertex_to_index.has(v)) {
					vertex_to_index[v] = global_vertices.size();
					global_vertices.push_back(v);
				}
				fv.v_idx = vertex_to_index[v];
				if (has_uv) {
					const Vector2 &uv = surface_uvs[vi];
					if (!uv_to_index.has(uv)) {
						uv_to_index[uv] = global_uvs.size();
						global_uvs.push_back(uv);
					}
					fv.vt_idx = uv_to_index[uv];
				}
				if (has_normal) {
					const Vector3 &n = surface_normals[vi];
					if (!normal_to_index.has(n)) {
						normal_to_index[n] = global_normals.size();
						global_normals.push_back(n);
					}
					fv.vn_idx = normal_to_index[n];
				}
				face_vertices.push_back(fv);
			}
		}
		surface_face_vertices.push_back(face_vertices);
	}

	// Write global vertex data
	for (int i = 0; i < global_vertices.size(); i++) {
		const Vector3 &v = global_vertices[i];
		f->store_line(vformat("v %.6f %.6f %.6f", v.x, v.y, v.z));
	}
	// Write global uvs
	bool any_uv = !global_uvs.is_empty();
	for (int i = 0; i < global_uvs.size(); i++) {
		const Vector2 &uv = global_uvs[i];
		f->store_line(vformat("vt %.6f %.6f", uv.x, 1.0 - uv.y));
	}
	// Write global normals
	bool any_normal = !global_normals.is_empty();
	for (int i = 0; i < global_normals.size(); i++) {
		const Vector3 &n = global_normals[i];
		f->store_line(vformat("vn %.6f %.6f %.6f", n.x, n.y, n.z));
	}

	// Write faces per surface
	for (int surf_idx = 0; surf_idx < surface_face_vertices.size(); surf_idx++) {
		const Vector<FaceVertex> &face_vertices = surface_face_vertices[surf_idx];
		const String &mat_name = surface_material_names[surf_idx];
		if (!mat_name.is_empty()) {
			f->store_line("usemtl " + mat_name);
		}
		// Write faces (assume triangles)
		for (int i = 0; i < face_vertices.size(); i += 3) {
			String face_line = "f";
			for (int k = 0; k < 3; k++) {
				const FaceVertex &fv = face_vertices[i + k];
				face_line += " ";
				face_line += itos(fv.v_idx + 1);
				if (any_uv || any_normal) {
					face_line += "/";
					if (any_uv)
						face_line += itos(fv.vt_idx + 1);
					if (any_normal)
						face_line += "/" + itos(fv.vn_idx + 1);
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
