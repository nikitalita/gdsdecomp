#ifndef OBJ_EXPORTER_H
#define OBJ_EXPORTER_H

#include "resource_exporter.h"
#include "scene/resources/material.h"

class ObjExporter : public ResourceExporter {
	GDCLASS(ObjExporter, ResourceExporter);

private:
	Error write_meshes_to_obj(const Vector<Ref<ArrayMesh>> &p_meshes, const String &p_path, const String &p_output_dir);
	Error write_materials_to_mtl(const HashMap<String, Ref<Material>> &p_materials, const String &p_path, const String &p_output_dir);

protected:
	static void _bind_methods();

public:
	virtual Error export_file(const String &p_out_path, const String &p_source_path) override;
	virtual void get_handled_types(List<String> *r_types) const override;
	virtual void get_handled_importers(List<String> *r_importers) const override;
	virtual Ref<ExportReport> export_resource(const String &p_output_dir, Ref<ImportInfo> p_import_info) override;
	virtual bool supports_multithread() const override;
};

#endif // OBJ_EXPORTER_H
