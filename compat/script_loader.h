
#pragma once
#include "compat/resource_loader_compat.h"

class ResourceFormatGDScriptLoader : public CompatFormatLoader {
	GDCLASS(ResourceFormatGDScriptLoader, CompatFormatLoader);

public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;

	static Ref<Script> _set_script(const String &p_path, ResourceInfo::LoadType p_type, const String &script_text);
	virtual Ref<Resource> custom_load(const String &p_path, const String &p_original_path, ResourceInfo::LoadType p_type, Error *r_error = nullptr, bool use_threads = true, ResourceFormatLoader::CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual Ref<ResourceInfo> get_resource_info(const String &p_path, Error *r_error) const override;
	virtual bool handles_fake_load() const override { return false; }
	static String _get_resource_type(const String &p_path);
	static Error _set_resource_info(Ref<ResourceInfo> &info, const String &p_path);
};

class FakeScriptConverterCompat : public ResourceCompatConverter {
	GDCLASS(FakeScriptConverterCompat, ResourceCompatConverter);

public:
	virtual Ref<Resource> convert(const Ref<MissingResource> &res, ResourceInfo::LoadType p_type, int ver_major, Error *r_error = nullptr) override;
	virtual bool handles_type(const String &p_type, int ver_major) const override;
};
