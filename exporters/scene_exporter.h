#pragma once
#include "exporters/obj_exporter.h"
#include "exporters/resource_exporter.h"
struct dep_info;

class SceneExporter : public ResourceExporter {
	GDCLASS(SceneExporter, ResourceExporter);

public:
	static constexpr bool can_multithread = false;

	virtual Error export_file(const String &out_path, const String &res_path) override;
	virtual Ref<ExportReport> export_resource(const String &output_dir, Ref<ImportInfo> import_infos) override;
	virtual void get_handled_types(List<String> *out) const override;
	virtual void get_handled_importers(List<String> *out) const override;
	virtual bool supports_multithread() const override { return can_multithread; }
	virtual String get_name() const override;
	virtual bool supports_nonpack_export() const override { return false; }
	virtual String get_default_export_extension(const String &res_path) const override;
};

struct SceneExporterInstance {
	int get_ver_major(const String &res_path);
	void rewrite_global_mesh_import_params(Ref<ImportInfo> p_import_info, const ObjExporter::MeshInfo &p_mesh_info);
	bool using_threaded_load() const;
	bool supports_multithread() const { return SceneExporter::can_multithread; }
	Error _export_file(const String &out_path, const String &res_path, Ref<ExportReport> p_report);
	Error export_file_to_obj(const String &out_path, const String &res_path, int ver_major, ObjExporter::MeshInfo &r_mesh_info);
};
