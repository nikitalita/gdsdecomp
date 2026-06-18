#include "gdre_previewer.h"
#include "core/object/class_db.h"

Error GDREPreviewer::edit(Ref<Resource> p_resource) {
	Error ret = OK;
	GDVIRTUAL_CALL(_edit, p_resource, ret);
	return ret;
}

bool GDREPreviewer::can_edit(const String &p_resource_path, const String &p_resource_type) const {
	bool ret = false;
	GDVIRTUAL_CALL(_can_edit, p_resource_path, p_resource_type, ret);
	return ret;
}

String GDREPreviewer::get_edited_resource_path() const {
	String ret = "";
	GDVIRTUAL_CALL(_get_edited_resource_path, ret);
	return ret;
}

void GDREPreviewer::reset() {
	GDVIRTUAL_CALL(_reset);
}

ResourceInfo::LoadType GDREPreviewer::get_load_type() const {
	ResourceInfo::LoadType ret = ResourceInfo::LoadType::REAL_LOAD;
	GDVIRTUAL_CALL(_get_load_type, ret);
	return ret;
}

void GDREPreviewer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("edit", "resource"), &GDREPreviewer::edit);
	ClassDB::bind_method(D_METHOD("can_edit", "resource_path"), &GDREPreviewer::can_edit);
	ClassDB::bind_method(D_METHOD("get_edited_resource_path"), &GDREPreviewer::get_edited_resource_path);
	ClassDB::bind_method(D_METHOD("reset"), &GDREPreviewer::reset);
	ClassDB::bind_method(D_METHOD("get_load_type"), &GDREPreviewer::get_load_type);
	GDVIRTUAL_BIND(_edit, "resource");
	GDVIRTUAL_BIND(_can_edit, "resource_path", "resource_type");
	GDVIRTUAL_BIND(_get_edited_resource_path);
	GDVIRTUAL_BIND(_reset);
	GDVIRTUAL_BIND(_get_load_type);
}
