#include "core/string/ustring.h"
#include "scene/resources/packed_scene.h"

#include "resource_import_metadatav2.h"

struct ResourceProperty {
	String name;
	Variant::Type type;
	Variant value;
	StringName class_name;
};

// this is derived from PackedScene because node instances in PackedScene cannot be returned otherwise
class FakeResource : public PackedScene {
	GDCLASS(FakeResource, PackedScene);
	String real_path;
	String real_type;

public:
	String get_path() const { return real_path; }
	String get_real_path() { return real_path; }
	String get_real_type() { return real_type; }
	void set_real_type(const String &t) { real_type = t; }
	void set_real_path(const String &p) { real_path = p; }
};

class FakeScript : public Script {
	GDCLASS(FakeScript, Script);
	String real_path;
	String real_type;

public:
	String get_path() const { return real_path; }
	String get_real_path() { return real_path; }
	String get_real_type() { return real_type; }
	void set_real_type(const String &t) { real_type = t; }
	void set_real_path(const String &p) { real_path = p; }
};

class ResourceData {
	// friend class ResourceLoaderCompat;
	// friend class ResourceFormatLoaderCompat;

private:
	struct ExtResource {
		String path;
		String type;
		String id;
		ResourceUID::ID uid = ResourceUID::INVALID_ID;
		RES cache;
		bool fake_load = true;
	};

	struct IntResource {
		String path;
		String class_name;
		uint64_t offset;
		String id;
		RES cache;
		List<ResourceProperty> cached_props;
		bool fake_load = true;
	};

	Vector<ExtResource> external_resources;
	Map<IntResource, int> internal_resources;
	//Vector<IntResource> internal_resources;

	String name = "";
	String path;
	String local_path;
	StringName class_name;
	Ref<ResourceImportMetadatav2> imd;
	ResourceUID::ID uid;
	int ver_format;
	int engine_ver_major;
	int engine_ver_minor;
	bool is_text;
	bool fake_load;
	List<ResourceProperty> props;

protected:
	RES make_dummy(const String &path, const String &type, const String &id);
	RES set_dummy_ext(const uint32_t erindex);
	RES set_dummy_ext(const String &path, const String &exttype);
	RES set_ext_res_path_and_type(const uint32_t id, const String &path, const String &type);

public:
	String getName() const { return name; }
	void setName(const String &name_) { name = name_; }

	String getPath() const { return path; }
	void setPath(const String &path_) { path = path_; }

	StringName className() const { return class_name; }
	void setClassName(const StringName &className) { class_name = className; }

	Ref<ResourceImportMetadatav2> getImd() const { return imd; }
	void setImd(const Ref<ResourceImportMetadatav2> &imd_) { imd = imd_; }

	ResourceUID::ID getUid() const { return uid; }
	void setUid(const ResourceUID::ID &uid_) { uid = uid_; }

	int get_format() const { return ver_format; }

	int engineVerMajor() const { return engine_ver_major; }
	void setEngineVerMajor(int engineVerMajor) { engine_ver_major = engineVerMajor; }

	int engineVerMinor() const { return engine_ver_minor; }
	void setEngineVerMinor(int engineVerMinor) { engine_ver_minor = engineVerMinor; }

	bool fakeLoad() const { return fake_load; }
	void setFakeLoad(bool fakeLoad) { fake_load = fakeLoad; }

	bool isText() const { return is_text; }

	bool is_using_uids() {
		return engine_ver_major >= 4 || (ver_format >= 3 && is_text) || (ver_format >= 4 && !is_text);
	}

	Error load_ext_resource(const uint32_t i);
	Error is_ext_resource_loaded(const uint32_t i);
	String get_external_resource_path(const RES &res);
	RES get_external_resource(const int subindex);
	RES get_external_resource_by_id(const String &id);
	RES get_external_resource(const String &path);
	bool has_external_resource_by_id(const String &id);
	bool has_external_resource(const String &path);
	bool has_external_resource(const RES &res);

	RES instance_internal_resource(const String &path, const String &type, const String &id);
	String get_internal_resource_path(const RES &res);
	RES get_internal_resource(const int subindex);
	RES get_internal_resource(const String &path);
	RES get_internal_resource_by_id(const String &id);

	String get_internal_resource_type(const String &path);
	bool has_internal_resource(const RES &res);
	bool has_internal_resource(const String &path);
	List<ResourceProperty> get_internal_resource_properties(const String &path);

	// String get_path() { return path; }
	// String get_name() { return name; }
	// bool is_text_resurce() { return is_text; }
};