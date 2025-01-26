#pragma once
#include "exporters/resource_exporter.h"

struct dep_info;

class SceneExporter : public ResourceExporter {
	GDCLASS(SceneExporter, ResourceExporter);

	virtual Error _export_file(const String &out_path, const String &res_path, Ref<ExportReport> p_report);
	bool using_threaded_load() const;

public:
	virtual Error export_file(const String &out_path, const String &res_path) override;
	virtual Ref<ExportReport> export_resource(const String &output_dir, Ref<ImportInfo> import_infos) override;
	virtual bool handles_import(const String &importer, const String &resource_type = String()) const override;
	virtual void get_handled_types(List<String> *out) const override;
	virtual void get_handled_importers(List<String> *out) const override;
	virtual bool supports_multithread() const override { return false; }
};