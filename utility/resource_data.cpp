#include "resource_data.h"

RES ResourceLoadData::make_dummy(const String &path, const String &type, const String &id) {
	Ref<FakeResource> dummy;
	dummy.instantiate();
	dummy->set_real_path(path);
	dummy->set_real_type(type);
	dummy->set_scene_unique_id(id);
	return dummy;
}

RES ResourceLoadData::set_dummy_ext(const uint32_t erindex) {
	ERR_FAIL_INDEX_V(erindex, external_resources.size(), RES());
	if (external_resources[erindex].cache.is_valid()) {
		return external_resources[erindex].cache;
	}
	String id;
	if (is_using_uids()) {
		id = ResourceUID::get_singleton()->id_to_text(external_resources[erindex].uid);
	} else if (external_resources[erindex].id != "") {
		id = external_resources[erindex].id;
	} else {
		id = itos(erindex + 1);
	}
	RES dummy = make_dummy(external_resources[erindex].path, external_resources[erindex].type, id);
	external_resources.write[erindex].cache = dummy;

	return dummy;
}

RES ResourceLoadData::set_dummy_ext(const String &path, const String &exttype) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].path == path) {
			if (external_resources[i].cache.is_valid()) {
				return external_resources[i].cache;
			}
			return set_dummy_ext(i);
		}
	}
	// If not found in cache...
	WARN_PRINT("External resource not found in cache???? Making dummy anyway...");
	ExtResource er;
	er.path = path;
	er.type = exttype;
	er.cache = make_dummy(path, exttype, itos(external_resources.size() + 1));
	external_resources.push_back(er);

	return er.cache;
}

RES ResourceLoadData::get_external_resource(const int subindex) {
	if (external_resources[subindex - 1].cache.is_valid()) {
		return external_resources[subindex - 1].cache;
	}
	// We don't do multithreading, so if this external resource is not cached (either dummy or real)
	// then we return a blank resource
	return RES();
}
String ResourceLoadData::get_external_resource_path(const RES &res) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].cache == res) {
			return external_resources[i].path;
		}
	}
	return String();
}
RES ResourceLoadData::get_external_resource(const String &path) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].path == path) {
			return external_resources[i].cache;
		}
	}
	// We don't do multithreading, so if this external resource is not cached (either dummy or real)
	// then we return a blank resource
	return RES();
}

RES ResourceLoadData::get_external_resource_by_id(const String &id) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].id == id) {
			return external_resources[i].cache;
		}
	}
	return RES();
}

bool ResourceLoadData::has_external_resource(const RES &res) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].cache == res) {
			return true;
		}
	}
	return false;
}

// bool ResourceLoadData::has_external_resource(const String &path) {
// 	for (int i = 0; i < external_resources.size(); i++) {
// 		if (external_resources[i].path == path) {
// 			return true;
// 		}
// 	}
// 	return false;
// }

// RES ResourceLoadData::get_internal_resource(const int subindex) {
// 	for (auto R = internal_resources.front(); R; R = R->next()) {
// 		if (R->value() == subindex) {
// 			return R->key().cache;
// 		}
// 	}
// 	return RES();
// }
// RES ResourceLoadData::get_internal_resource_by_id(const String &id) {
// 	for (auto R = internal_resources.front(); R; R = R->next()) {
// 		if (R->key().id == id) {
// 			return R->key().cache;
// 		}
// 	}

// 	if (has_internal_resource("local://" + id)) {
// 		return get_internal_resource("local://" + id);
// 	}
// 	return RES();
// }

// RES ResourceLoadData::get_internal_resource(const String &path) {
// 	if (has_internal_resource(path)) {
// 		return internal_res_cache[path];
// 	}
// 	return RES();
// }

// String ResourceLoadData::get_internal_resource_type(const String &path) {
// 	if (has_internal_resource(path)) {
// 		return internal_type_cache[path];
// 	}
// 	return "None";
// }

// List<ResourceProperty> ResourceLoadData::get_internal_resource_properties(const String &path) {
// 	if (has_internal_resource(path)) {
// 		return internal_index_cached_properties[path];
// 	}
// 	return List<ResourceProperty>();
// }

// bool ResourceLoadData::has_internal_resource(const RES &res) {
// 	for (KeyValue<String, RES> E : internal_res_cache) {
// 		if (E.value == res) {
// 			return true;
// 		}
// 	}
// 	return false;
// }

// bool ResourceLoadData::has_internal_resource(const String &path) {
// 	return internal_res_cache.has(path);
// }

// String ResourceLoadData::get_resource_path(const RES &res) {
// 	if (res.is_null()) {
// 		return "";
// 	}
// 	if (res->is_class("FakeResource")) {
// 		return ((Ref<FakeResource>)res)->get_real_path();
// 	} else if (res->is_class("FakeScript")) {
// 		return ((Ref<FakeScript>)res)->get_real_path();
// 	} else {
// 		return res->get_path();
// 	}
// }
