#pragma once

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "utility/import_info.h"

class ResourceExporter : public RefCounted {
	GDCLASS(ResourceExporter, RefCounted);

protected:
	static void _bind_methods();

public:
	enum LossType {
		UNKNOWN = -1,
		LOSSLESS = 0,
		STORED_LOSSY = 1,
		IMPORTED_LOSSY = 2,
		STORED_AND_IMPORTED_LOSSY = 3,
	};

	virtual void get_recognized_extensions(List<String> *p_extensions) const = 0;
	virtual String get_resource_type() const = 0;
	virtual String get_exporter_name() const = 0;

	virtual Error export_file(Ref<ImportInfo> &p_resource, const String &p_output_proj_dir, const HashMap<StringName, Variant> &p_options) = 0;
	virtual LossType get_loss_type(Ref<ImportInfo> &p_resource) const = 0;
	virtual bool recognize(const Ref<ImportInfo> &p_resource) const = 0;

	virtual ~ResourceExporter() {}
};

class ResourceFormatExporter : public ResourceFormatSaver {
	static ResourceFormatExporter *singleton;
	Ref<ResourceExporter> _get_exporter_for_resource(const Ref<ImportInfo> &p_resource, Error *r_err);
	Ref<ResourceExporter> _get_exporter_by_name(const String &p_name);

protected:
	static void _bind_methods();

public:
	static ResourceFormatExporter *get_singleton() { return singleton; }
	static Ref<ResourceExporter> get_exporter_for_resource(const Ref<ImportInfo> &p_resource, Error *r_err);
	static Ref<ResourceExporter> get_exporter_by_name(const String &p_name);
	static Ref<ResourceExporter> add_exporter(const Ref<ResourceExporter> &p_exporter);
	ResourceFormatExporter() { singleton = this; }
};
