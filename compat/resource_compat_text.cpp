/**************************************************************************/
/*  resource_format_text.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "resource_compat_text.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/missing_resource.h"
#include "core/io/resource_format_binary.h"
#include "core/object/script_language.h"
#include "core/version.h"

#include "compat/variant_writer_compat.h"
#include "utility/gdre_settings.h"

// Version 2: changed names for Basis, AABB, Vectors, etc.
// Version 3: new string ID for ext/subresources, breaks forward compat.
#define FORMAT_VERSION 3

#define BINARY_FORMAT_VERSION 4

#include "core/io/dir_access.h"
#include "core/version.h"

#define _printerr() ERR_PRINT(String(res_path + ":" + itos(lines) + " - Parse Error: " + error_text).utf8().get_data());

///

Ref<Resource> ResourceLoaderCompatText::get_resource() {
	return resource;
}

Error ResourceLoaderCompatText::_parse_sub_resource_dummy(DummyReadData *p_data, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style) or string (sub-resource index)";
		return ERR_PARSE_ERROR;
	}

	if (p_data->no_placeholders) {
		r_res.unref();
	} else {
		String unique_id = token.value;

		if (!p_data->resource_map.has(unique_id)) {
			r_err_str = "Found unique_id reference before mapping, sub-resources stored out of order in resource file";
			return ERR_PARSE_ERROR;
		}

		r_res = p_data->resource_map[unique_id];
	}

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error ResourceLoaderCompatText::_parse_ext_resource_dummy(DummyReadData *p_data, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style sub-resource index) or String (ext-resource ID)";
		return ERR_PARSE_ERROR;
	}

	if (p_data->no_placeholders) {
		r_res.unref();
	} else {
		String id = token.value;

		ERR_FAIL_COND_V(!p_data->rev_external_resources.has(id), ERR_PARSE_ERROR);

		r_res = p_data->rev_external_resources[id];
	}

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error ResourceLoaderCompatText::_parse_sub_resource(VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style sub-resource index) or string";
		return ERR_PARSE_ERROR;
	}

	String id = token.value;
	ERR_FAIL_COND_V(!int_resources.has(id), ERR_INVALID_PARAMETER);
	r_res = int_resources[id];

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error ResourceLoaderCompatText::_parse_ext_resource(VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style sub-resource index) or String (ext-resource ID)";
		return ERR_PARSE_ERROR;
	}

	String id = token.value;
	Error err = OK;

	if (!ignore_resource_parsing) {
		if (!ext_resources.has(id)) {
			r_err_str = "Can't load cached ext-resource id: " + id;
			return ERR_PARSE_ERROR;
		}

		String path = ext_resources[id].path;
		String type = ext_resources[id].type;
		Ref<ResourceLoader::LoadToken> &load_token = ext_resources[id].load_token;

		if (load_token.is_valid()) { // If not valid, it's OK since then we know this load accepts broken dependencies.
			Ref<Resource> res = finish_ext_load(load_token, &err);
			if (res.is_null()) {
				ERR_FAIL_COND_V_MSG(!is_real_load(), ERR_FILE_MISSING_DEPENDENCIES, "WE SHOULD NEVER GET HERE!!!!!!!!!!!!!!!!!!!!!!!!! : [ext_resource] referenced non-existent resource at: " + path);
				if (!ResourceLoader::is_cleaning_tasks()) {
					if (ResourceLoader::get_abort_on_missing_resources()) {
						error = ERR_FILE_MISSING_DEPENDENCIES;
						error_text = "[ext_resource] referenced non-existent resource at: " + path;
						_printerr();
						err = error;
					} else {
						ResourceLoader::notify_dependency_error(local_path, path, type);
					}
				}
			} else {
#ifdef TOOLS_ENABLED
				//remember ID for saving
				res->set_id_for_path(local_path, id);
#endif
				r_res = res;
			}
		} else {
			r_res = Ref<Resource>();
		}
	}

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return err;
}

Ref<PackedScene> ResourceLoaderCompatText::_parse_node_tag(VariantParser::ResourceParser &parser) {
	Ref<PackedScene> packed_scene;
	packed_scene.instantiate();

	// to check which version this needs to be saved as

	while (true) {
		if (next_tag.name == "node") {
			int parent = -1;
			int owner = -1;
			int type = -1;
			int name = -1;
			int instance = -1;
			int index = -1;
			//int base_scene=-1;

			if (next_tag.fields.has("name")) {
				name = packed_scene->get_state()->add_name(next_tag.fields["name"]);
			}

			if (next_tag.fields.has("parent")) {
				NodePath np = next_tag.fields["parent"];
				np.prepend_period(); //compatible to how it manages paths internally
				parent = packed_scene->get_state()->add_node_path(np);
			}

			if (next_tag.fields.has("type")) {
				type = packed_scene->get_state()->add_name(next_tag.fields["type"]);
			} else {
				type = SceneState::TYPE_INSTANTIATED; //no type? assume this was instantiated
			}

			HashSet<StringName> path_properties;

			if (next_tag.fields.has("node_paths")) {
				Vector<String> paths = next_tag.fields["node_paths"];
				for (int i = 0; i < paths.size(); i++) {
					path_properties.insert(paths[i]);
				}
			}

			if (next_tag.fields.has("instance")) {
				instance = packed_scene->get_state()->add_value(next_tag.fields["instance"]);

				if (packed_scene->get_state()->get_node_count() == 0 && parent == -1) {
					packed_scene->get_state()->set_base_scene(instance);
					instance = -1;
				}
			}

			if (next_tag.fields.has("instance_placeholder")) {
				String path = next_tag.fields["instance_placeholder"];

				int path_v = packed_scene->get_state()->add_value(path);

				if (packed_scene->get_state()->get_node_count() == 0) {
					error = ERR_FILE_CORRUPT;
					error_text = "Instance Placeholder can't be used for inheritance.";
					_printerr();
					return Ref<PackedScene>();
				}

				instance = path_v | SceneState::FLAG_INSTANCE_IS_PLACEHOLDER;
			}

			if (next_tag.fields.has("owner")) {
				owner = packed_scene->get_state()->add_node_path(next_tag.fields["owner"]);
			} else {
				if (parent != -1 && !(type == SceneState::TYPE_INSTANTIATED && instance == -1)) {
					owner = 0; //if no owner, owner is root
				}
			}

			if (next_tag.fields.has("index")) {
				index = next_tag.fields["index"];
			}

			int node_id = packed_scene->get_state()->add_node(parent, owner, type, name, instance, index);

			if (next_tag.fields.has("groups")) {
				Array groups = next_tag.fields["groups"];
				for (int i = 0; i < groups.size(); i++) {
					packed_scene->get_state()->add_node_group(node_id, packed_scene->get_state()->add_name(groups[i]));
				}
			}

			while (true) {
				String assign;
				Variant value;

				error = VariantParserCompat::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &parser);

				if (error) {
					if (error == ERR_FILE_MISSING_DEPENDENCIES) {
						// Resource loading error, just skip it.
					} else if (error != ERR_FILE_EOF) {
						_printerr();
						return Ref<PackedScene>();
					} else {
						error = OK;
						return packed_scene;
					}
				}

				if (!assign.is_empty()) {
					StringName assign_name = assign;
					int nameidx = packed_scene->get_state()->add_name(assign_name);
					int valueidx = packed_scene->get_state()->add_value(value);
					packed_scene->get_state()->add_node_property(node_id, nameidx, valueidx, path_properties.has(assign_name));

					// TODO: SOMETHING ABOUT THIS!!!!!!!!!!!:
					// ******
					// GDRE OLD CODE:
					// We add the values to the resourceproperty list too, as while object properties will be correctly set,
					// we can't get them out of the packed_scene if they're not instanced without traversing the node list,
					// packed_scene->get() will just return null

					// ResourceProperty rp;
					// rp.name = assign;
					// rp.type = value.get_type();
					// if (rp.type == Variant::OBJECT) {
					// 	Object *obj = value;
					// 	rp.class_name = obj->get_class_name();
					// }
					// rp.value = value;
					// lrp.push_back(rp);

					//it's assignment
				} else if (!next_tag.name.is_empty()) {
					break;
				}
			}
		} else if (next_tag.name == "connection") {
			if (!next_tag.fields.has("from")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'from' field from connection tag";
				return Ref<PackedScene>();
			}

			if (!next_tag.fields.has("to")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'to' field from connection tag";
				return Ref<PackedScene>();
			}

			if (!next_tag.fields.has("signal")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'signal' field from connection tag";
				return Ref<PackedScene>();
			}

			if (!next_tag.fields.has("method")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'method' field from connection tag";
				return Ref<PackedScene>();
			}

			NodePath from = next_tag.fields["from"];
			NodePath to = next_tag.fields["to"];
			StringName method = next_tag.fields["method"];
			StringName signal = next_tag.fields["signal"];
			int flags = Object::CONNECT_PERSIST;
			int unbinds = 0;
			Array binds;

			if (next_tag.fields.has("flags")) {
				flags = next_tag.fields["flags"];
			}

			if (next_tag.fields.has("binds")) {
				binds = next_tag.fields["binds"];
			}

			if (next_tag.fields.has("unbinds")) {
				packed_scene_version = 3;
				unbinds = next_tag.fields["unbinds"];
			}

			Vector<int> bind_ints;
			for (int i = 0; i < binds.size(); i++) {
				bind_ints.push_back(packed_scene->get_state()->add_value(binds[i]));
			}

			packed_scene->get_state()->add_connection(
					packed_scene->get_state()->add_node_path(from.simplified()),
					packed_scene->get_state()->add_node_path(to.simplified()),
					packed_scene->get_state()->add_name(signal),
					packed_scene->get_state()->add_name(method),
					flags,
					unbinds,
					bind_ints);

			error = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &parser);

			if (error) {
				if (error != ERR_FILE_EOF) {
					_printerr();
					return Ref<PackedScene>();
				} else {
					error = OK;
					return packed_scene;
				}
			}
		} else if (next_tag.name == "editable") {
			if (!next_tag.fields.has("path")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'path' field from editable tag";
				_printerr();
				return Ref<PackedScene>();
			}

			NodePath path = next_tag.fields["path"];

			packed_scene->get_state()->add_editable_instance(path.simplified());

			error = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &parser);

			if (error) {
				if (error != ERR_FILE_EOF) {
					_printerr();
					return Ref<PackedScene>();
				} else {
					error = OK;
					return packed_scene;
				}
			}
		} else {
			error = ERR_FILE_CORRUPT;
			_printerr();
			return Ref<PackedScene>();
		}
	}
}

Error ResourceLoaderCompatText::load() {
	if (error != OK) {
		return error;
	}

	while (true) {
		if (next_tag.name != "ext_resource") {
			break;
		}

		if (!next_tag.fields.has("path")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'path' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("id")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'id' in external resource tag";
			_printerr();
			return error;
		}

		String path = next_tag.fields["path"];
		String type = next_tag.fields["type"];
		String id = next_tag.fields["id"];

		ResourceUID::ID uid = ResourceUID::INVALID_ID;

		if (next_tag.fields.has("uid")) {
			String uidt = next_tag.fields["uid"];
			uid = ResourceUID::get_singleton()->text_to_id(uidt);
// UID Stuff
#if 0
			if (uid != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(uid)) {
				// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
				path = ResourceUID::get_singleton()->get_id_path(uid);
			} else {
#ifdef TOOLS_ENABLED
				// Silence a warning that can happen during the initial filesystem scan due to cache being regenerated.
				if (ResourceLoader::get_resource_uid(path) != uid) {
					WARN_PRINT(String(res_path + ":" + itos(lines) + " - ext_resource, invalid UID: " + uidt + " - using text path instead: " + path).utf8().get_data());
				}
#else
				WARN_PRINT(String(res_path + ":" + itos(lines) + " - ext_resource, invalid UID: " + uidt + " - using text path instead: " + path).utf8().get_data());
#endif
			}
#endif
		}

		if (!path.contains("://") && path.is_relative_path()) {
			// path is relative to file being loaded, so convert to a resource path
			WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!");
			path = GDRESettings::get_singleton()->localize_path(local_path.get_base_dir().path_join(path));
		}

		if (remaps.has(path)) {
			path = remaps[path];
		}

		ext_resources[id].path = path;
		ext_resources[id].type = type;
		ext_resources[id].load_token = start_ext_load(path, type, uid, id);

		// real load failed
		if (!ext_resources[id].load_token.is_valid()) {
			if (!is_real_load()) {
				WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!");
			}
			if (ResourceLoader::get_abort_on_missing_resources()) {
				error = ERR_FILE_CORRUPT;
				error_text = "[ext_resource] referenced non-existent resource at: " + path;
				_printerr();
				return error;
			} else {
				ResourceLoader::notify_dependency_error(local_path, path, type);
			}
		}

		error = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (error) {
			_printerr();
			return error;
		}

		resource_current++;
	}

	//these are the ones that count
	resources_total -= resource_current;
	resource_current = 0;

	// sub_resource parsing
	while (true) {
		if (next_tag.name != "sub_resource") {
			break;
		}

		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("id")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'id' in external resource tag";
			_printerr();
			return error;
		}

		String type = next_tag.fields["type"];
		String id = next_tag.fields["id"];

		String path = local_path + "::" + id;

		//bool exists=ResourceCache::has(path);

		Ref<Resource> res;
		bool do_assign = false;

		if (cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE && ResourceCache::has(path)) {
			//reuse existing
			if (!is_real_load()) {
				WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!");
			}
			Ref<Resource> cache = ResourceCache::get_ref(path);
			if (cache.is_valid() && cache->get_class() == type) {
				res = cache;
				res->reset_state();
				do_assign = true;
			}
		}

		MissingResource *missing_resource = nullptr;
		if (!is_real_load()) {
			missing_resource = CompatFormatLoader::create_missing_internal_resource(path, type, id);
			res = Ref<Resource>(missing_resource);
		} else {
			// TODO!!!!!!!!!!!!!!!
			// TODO!!!!!!!!!!!!!!!
			// TODO: non-global loads and real loads should check if there is a loader for this resource
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Non-global loads and real loads are not supported yet.");
		}
		if (res.is_null()) { // not reuse
			Ref<Resource> cache = ResourceCache::get_ref(path);
			if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE && cache.is_valid()) { //only if it doesn't exist
				// cached, do not assign
				res = cache;
			} else {
				// create

				Object *obj = ClassDB::instantiate(type);
				if (!obj) {
					if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
						missing_resource = memnew(MissingResource);
						missing_resource->set_original_class(type);
						missing_resource->set_recording_properties(true);
						obj = missing_resource;
					} else {
						error_text += "Can't create sub resource of type: " + type;
						_printerr();
						error = ERR_FILE_CORRUPT;
						return error;
					}
				}

				Resource *r = Object::cast_to<Resource>(obj);
				if (!r) {
					error_text += "Can't create sub resource of type, because not a resource: " + type;
					_printerr();
					error = ERR_FILE_CORRUPT;
					return error;
				}

				res = Ref<Resource>(r);
				do_assign = true;
			}
		}

		resource_current++;

		if (progress && resources_total > 0) {
			*progress = resource_current / float(resources_total);
		}

		int_resources[id] = res; // Always assign int resources.
		if (do_assign) {
			if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
				if (!is_real_load()) {
					WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!");
				}
				res->set_path(path, cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE);
			} else if (!path.is_resource_file()) {
				res->set_path_cache(path);
			}
			res->set_scene_unique_id(id);
		}

		Dictionary missing_resource_properties;

		while (true) {
			String assign;
			Variant value;

			error = VariantParserCompat::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp);

			if (error) {
				_printerr();
				return error;
			}

			if (!assign.is_empty()) {
				if (do_assign) {
					bool set_valid = true;

					// Should only get here on real loads.
					if (value.get_type() == Variant::OBJECT && missing_resource == nullptr && ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
						// If the property being set is a missing resource (and the parent is not),
						// then setting it will most likely not work.
						// Instead, save it as metadata.
						if (!is_real_load()) {
							WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!");
						}

						Ref<MissingResource> mr = value;
						if (mr.is_valid()) {
							missing_resource_properties[assign] = mr;
							set_valid = false;
						}
					}

					if (value.get_type() == Variant::ARRAY) {
						Array set_array = value;
						bool is_get_valid = false;
						Variant get_value = res->get(assign, &is_get_valid);
						if (is_get_valid && get_value.get_type() == Variant::ARRAY) {
							Array get_array = get_value;
							if (!set_array.is_same_typed(get_array)) {
								value = Array(set_array, get_array.get_typed_builtin(), get_array.get_typed_class_name(), get_array.get_typed_script());
							}
						}
					}

					if (set_valid) {
						res->set(assign, value);
					}
				}
				//it's assignment
			} else if (!next_tag.name.is_empty()) {
				error = OK;
				break;
			} else {
				error = ERR_FILE_CORRUPT;
				error_text = "Premature end of file while parsing [sub_resource]";
				_printerr();
				return error;
			}
		}

		if (missing_resource) {
			missing_resource->set_recording_properties(false);
		}

		if (!missing_resource_properties.is_empty()) {
			res->set_meta(META_MISSING_RESOURCES, missing_resource_properties);
		}
	}

	// main resource parsing (if it is a resource)
	while (true) {
		if (next_tag.name != "resource") {
			break;
		}

		if (is_scene) {
			error_text += "found the 'resource' tag on a scene file!";
			_printerr();
			error = ERR_FILE_CORRUPT;
			return error;
		}

		// clang-format off
		if (is_real_load()) {
		Ref<Resource> cache = ResourceCache::get_ref(local_path);
		if (cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE && cache.is_valid() && cache->get_class() == res_type) {
			cache->reset_state();
			resource = cache;
		}
		}
		// clang-format on

		MissingResource *missing_resource = nullptr;

		if (!is_real_load()) {
			missing_resource = CompatFormatLoader::create_missing_main_resource(local_path, res_type, res_uid);
			resource = Ref<Resource>(missing_resource);
		} else {
			// TODO!!!!!!!!!!!!!!!
			// TODO!!!!!!!!!!!!!!!
			// TODO: non-global loads and real loads should check if there is a loader for this resource
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Non-global loads and real loads are not supported yet.");
		}

		if (!resource.is_valid()) {
			if (!is_real_load()) {
				WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!");
			}
			Object *obj = ClassDB::instantiate(res_type);
			if (!obj) {
				if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
					missing_resource = memnew(MissingResource);
					missing_resource->set_original_class(res_type);
					missing_resource->set_recording_properties(true);
					obj = missing_resource;
				} else {
					error_text += "Can't create sub resource of type: " + res_type;
					_printerr();
					error = ERR_FILE_CORRUPT;
					return error;
				}
			}

			Resource *r = Object::cast_to<Resource>(obj);
			if (!r) {
				error_text += "Can't create sub resource of type, because not a resource: " + res_type;
				_printerr();
				error = ERR_FILE_CORRUPT;
				return error;
			}

			resource = Ref<Resource>(r);
		}

		Dictionary missing_resource_properties;

		// main resource property parsing
		while (true) {
			String assign;
			Variant value;

			error = VariantParserCompat::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp);

			if (error) {
				if (error != ERR_FILE_EOF) {
					_printerr();
					return error;
				}
				// EOF, Done parsing
				error = OK;
				if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
					if (!is_real_load()) {
						WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!");
					}
					if (!ResourceCache::has(res_path)) {
						resource->set_path(res_path);
					}
					resource->set_as_translation_remapped(translation_remapped);
				} else { // TODO: PR this to Godot
					resource->set_path_cache(res_path);
				}

				break;
			}

			if (!assign.is_empty()) {
				bool set_valid = true;

				if (value.get_type() == Variant::OBJECT && ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled() && missing_resource == nullptr) {
					// If the property being set is a missing resource (and the parent is not),
					// then setting it will most likely not work.
					// Instead, save it as metadata.

					Ref<MissingResource> mr = value;
					if (mr.is_valid()) {
						missing_resource_properties[assign] = mr;
						set_valid = false;
					}
				}

				if (value.get_type() == Variant::ARRAY) {
					Array set_array = value;
					bool is_get_valid = false;
					Variant get_value = resource->get(assign, &is_get_valid);
					if (is_get_valid && get_value.get_type() == Variant::ARRAY) {
						Array get_array = get_value;
						if (!set_array.is_same_typed(get_array)) {
							value = Array(set_array, get_array.get_typed_builtin(), get_array.get_typed_class_name(), get_array.get_typed_script());
						}
					}
				}

				if (set_valid) {
					resource->set(assign, value);
				}
				//it's assignment
			} else if (!next_tag.name.is_empty()) {
				error = ERR_FILE_CORRUPT;
				error_text = "Extra tag found when parsing main resource file";
				_printerr();
				return error;
			} else {
				break;
			}
		}

		resource_current++;

		if (progress && resources_total > 0) {
			*progress = resource_current / float(resources_total);
		}

		if (missing_resource) {
			missing_resource->set_recording_properties(false);
		}

		if (!missing_resource_properties.is_empty()) {
			resource->set_meta(META_MISSING_RESOURCES, missing_resource_properties);
		}
		set_compat_meta(resource);

		error = OK;

		return error;
	}
	// end main resource parsing

	//for scene files

	if (next_tag.name == "node") {
		if (!is_scene) {
			error_text += "found the 'node' tag on a resource file!";
			_printerr();
			error = ERR_FILE_CORRUPT;
			return error;
		}

		Ref<PackedScene> packed_scene = _parse_node_tag(rp);

		if (!packed_scene.is_valid()) {
			return error;
		}

		error = OK;
		//get it here
		resource = packed_scene;
		if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE && !ResourceCache::has(res_path)) {
			packed_scene->set_path(res_path);
		} else { // TODO: PR this to Godot
			packed_scene->set_path_cache(res_path);
		}

		resource_current++;

		if (progress && resources_total > 0) {
			*progress = resource_current / float(resources_total);
		}
		set_compat_meta(resource);

		return error;
	} else {
		error_text += "Unknown tag in file: " + next_tag.name;
		_printerr();
		error = ERR_FILE_CORRUPT;
		return error;
	}
}

int ResourceLoaderCompatText::get_stage() const {
	return resource_current;
}

int ResourceLoaderCompatText::get_stage_count() const {
	return resources_total; //+ext_resources;
}

void ResourceLoaderCompatText::set_translation_remapped(bool p_remapped) {
	translation_remapped = p_remapped;
}

ResourceLoaderCompatText::ResourceLoaderCompatText() :
		stream(false) {}

void ResourceLoaderCompatText::get_dependencies(Ref<FileAccess> p_f, List<String> *p_dependencies, bool p_add_types) {
	open(p_f);
	ignore_resource_parsing = true;
	ERR_FAIL_COND(error != OK);

	while (next_tag.name == "ext_resource") {
		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return;
		}

		if (!next_tag.fields.has("id")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'id' in external resource tag";
			_printerr();
			return;
		}

		String path = next_tag.fields["path"];
		String type = next_tag.fields["type"];
		String fallback_path;

		bool using_uid = false;
		if (next_tag.fields.has("uid")) {
			// If uid exists, return uid in text format, not the path.
			String uidt = next_tag.fields["uid"];
			ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(uidt);
			if (uid != ResourceUID::INVALID_ID) {
				fallback_path = path; // Used by Dependency Editor, in case uid path fails.
				path = ResourceUID::get_singleton()->id_to_text(uid);
				using_uid = true;
			}
		}

		if (!using_uid && !path.contains("://") && path.is_relative_path()) {
			// Path is relative to file being loaded, so convert to a resource path.
			WARN_PRINT("WE SHOULD NEVER GET HERE!!!!!!!!!!");
			path = local_path.get_base_dir().path_join(path);
		}

		if (p_add_types) {
			path += "::" + type;
		}
		if (!fallback_path.is_empty()) {
			if (!p_add_types) {
				path += "::"; // Ensure that path comes third, even if there is no type.
			}
			path += "::" + fallback_path;
		}

		p_dependencies->push_back(path);

		Error err = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (err) {
			print_line(error_text + " - " + itos(lines));
			error_text = "Unexpected end of file";
			_printerr();
			error = ERR_FILE_CORRUPT;
			return;
		}
	}
}

Error ResourceLoaderCompatText::rename_dependencies(Ref<FileAccess> p_f, const String &p_path, const HashMap<String, String> &p_map) {
	open(p_f, true);
	ERR_FAIL_COND_V(error != OK, error);
	ignore_resource_parsing = true;
	//FileAccess

	Ref<FileAccess> fw;

	String base_path = local_path.get_base_dir();

	uint64_t tag_end = f->get_position();

	while (true) {
		Error err = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (err != OK) {
			error = ERR_FILE_CORRUPT;
			ERR_FAIL_V(error);
		}

		if (next_tag.name != "ext_resource") {
			//nothing was done
			if (fw.is_null()) {
				return OK;
			}

			break;

		} else {
			if (fw.is_null()) {
				fw = FileAccess::open(p_path + ".depren", FileAccess::WRITE);

				if (res_uid == ResourceUID::INVALID_ID) {
					res_uid = ResourceSaver::get_resource_id_for_path(p_path);
				}

				String uid_text = "";
				if (res_uid != ResourceUID::INVALID_ID) {
					uid_text = " uid=\"" + ResourceUID::get_singleton()->id_to_text(res_uid) + "\"";
				}

				if (is_scene) {
					fw->store_line("[gd_scene load_steps=" + itos(resources_total) + " format=" + itos(FORMAT_VERSION) + uid_text + "]\n");
				} else {
					String script_res_text;
					if (!script_class.is_empty()) {
						script_res_text = "script_class=\"" + script_class + "\" ";
					}
					fw->store_line("[gd_resource type=\"" + res_type + "\" " + script_res_text + "load_steps=" + itos(resources_total) + " format=" + itos(FORMAT_VERSION) + uid_text + "]\n");
				}
			}

			if (!next_tag.fields.has("path") || !next_tag.fields.has("id") || !next_tag.fields.has("type")) {
				error = ERR_FILE_CORRUPT;
				ERR_FAIL_V(error);
			}

			String path = next_tag.fields["path"];
			String id = next_tag.fields["id"];
			String type = next_tag.fields["type"];

			if (next_tag.fields.has("uid")) {
				String uidt = next_tag.fields["uid"];
				ResourceUID::ID uid = ResourceUID::get_singleton()->text_to_id(uidt);
				if (uid != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(uid)) {
					// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
					path = ResourceUID::get_singleton()->get_id_path(uid);
				}
			}
			bool relative = false;
			if (!path.begins_with("res://")) {
				path = base_path.path_join(path).simplify_path();
				relative = true;
			}

			if (p_map.has(path)) {
				path = p_map[path];
			}

			if (relative) {
				//restore relative
				path = base_path.path_to_file(path);
			}

			String s = "[ext_resource type=\"" + type + "\"";

			// clang-format off
			if (ver_format >= 3){
			ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(path);
			if (uid != ResourceUID::INVALID_ID) {
				s += " uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\"";
			}
			}
			// clang-format on
			s += " path=\"" + path + "\" id=\"" + id + "\"]";
			fw->store_line(s); // Bundled.

			tag_end = f->get_position();
		}
	}

	f->seek(tag_end);

	const uint32_t buffer_size = 2048;
	uint8_t *buffer = (uint8_t *)alloca(buffer_size);
	uint32_t num_read;

	num_read = f->get_buffer(buffer, buffer_size);
	ERR_FAIL_COND_V_MSG(num_read == UINT32_MAX, ERR_CANT_CREATE, "Failed to allocate memory for buffer.");
	ERR_FAIL_COND_V(num_read == 0, ERR_FILE_CORRUPT);

	if (*buffer == '\n') {
		// Skip first newline character since we added one.
		if (num_read > 1) {
			fw->store_buffer(buffer + 1, num_read - 1);
		}
	} else {
		fw->store_buffer(buffer, num_read);
	}

	while (!f->eof_reached()) {
		num_read = f->get_buffer(buffer, buffer_size);
		fw->store_buffer(buffer, num_read);
	}

	bool all_ok = fw->get_error() == OK;

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

void ResourceLoaderCompatText::open(Ref<FileAccess> p_f, bool p_skip_first_tag) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;
	is_scene = false;
	ignore_resource_parsing = false;
	resource_current = 0;

	VariantParser::Tag tag;
	Error err = VariantParserCompat::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		error = err;
		_printerr();
		return;
	}

	if (tag.fields.has("format")) {
		ver_format = tag.fields["format"];
		if (ver_format > FORMAT_VERSION) {
			error_text = "Saved with newer format version";
			_printerr();
			error = ERR_PARSE_ERROR;
			return;
		}
		switch (ver_format) {
			// no engine version info in text, so we infer from the format version
			case 3:
				ver_major = 4;
				ver_minor = 0;
				break;
			case 2:
				ver_major = 3;
				ver_minor = 0;
				break;
			case 1:
				ver_major = 2;
				ver_minor = 0;
				break;
		}
	}

	if (tag.name == "gd_scene") {
		is_scene = true;

	} else if (tag.name == "gd_resource") {
		if (!tag.fields.has("type")) {
			error_text = "Missing 'type' field in 'gd_resource' tag";
			_printerr();
			error = ERR_PARSE_ERROR;
			return;
		}

		if (tag.fields.has("script_class")) {
			script_class = tag.fields["script_class"];
		}

		res_type = tag.fields["type"];

	} else {
		error_text = "Unrecognized file type: " + tag.name;
		_printerr();
		error = ERR_PARSE_ERROR;
		return;
	}

	if (tag.fields.has("uid")) {
		res_uid = ResourceUID::get_singleton()->text_to_id(tag.fields["uid"]);
	} else {
		res_uid = ResourceUID::INVALID_ID;
	}

	if (tag.fields.has("load_steps")) {
		resources_total = tag.fields["load_steps"];
	} else {
		resources_total = 0;
	}

	if (!p_skip_first_tag) {
		err = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (err) {
			error_text = "Unexpected end of file";
			_printerr();
			error = ERR_FILE_CORRUPT;
		}
	}

	rp.ext_func = _parse_ext_resources;
	rp.sub_func = _parse_sub_resources;
	rp.userdata = this;
}

static void bs_save_unicode_string(Ref<FileAccess> p_f, const String &p_string, bool p_bit_on_len = false) {
	CharString utf8 = p_string.utf8();
	if (p_bit_on_len) {
		p_f->store_32((utf8.length() + 1) | 0x80000000);
	} else {
		p_f->store_32(utf8.length() + 1);
	}
	p_f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

Error ResourceLoaderCompatText::save_as_binary(const String &p_path) {
#if 0
	if (error) {
		return error;
	}

	Ref<FileAccess> wf = FileAccess::open(p_path, FileAccess::WRITE);
	if (wf.is_null()) {
		return ERR_CANT_OPEN;
	}

	//save header compressed
	static const uint8_t header[4] = { 'R', 'S', 'R', 'C' };
	wf->store_buffer(header, 4);

	wf->store_32(0); //endianness, little endian
	wf->store_32(0); //64 bits file, false for now
	wf->store_32(VERSION_MAJOR);
	wf->store_32(VERSION_MINOR);
	static const int save_format_version = BINARY_FORMAT_VERSION;
	wf->store_32(save_format_version);

	bs_save_unicode_string(wf, is_scene ? "PackedScene" : resource_type);
	wf->store_64(0); //offset to import metadata, this is no longer used

	wf->store_32(ResourceFormatSaverBinaryInstance::FORMAT_FLAG_NAMED_SCENE_IDS | ResourceFormatSaverBinaryInstance::FORMAT_FLAG_UIDS);

	wf->store_64(res_uid);

	for (int i = 0; i < ResourceFormatSaverBinaryInstance::RESERVED_FIELDS; i++) {
		wf->store_32(0); // reserved
	}

	wf->store_32(0); //string table size, will not be in use
	uint64_t ext_res_count_pos = wf->get_position();

	wf->store_32(0); //zero ext resources, still parsing them

	//go with external resources

	DummyReadData dummy_read;
	VariantParser::ResourceParser rp_new;
	rp_new.ext_func = _parse_ext_resource_dummys;
	rp_new.sub_func = _parse_sub_resource_dummys;
	rp_new.userdata = &dummy_read;

	while (next_tag.name == "ext_resource") {
		if (!next_tag.fields.has("path")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'path' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("id")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'id' in external resource tag";
			_printerr();
			return error;
		}

		String path = next_tag.fields["path"];
		String type = next_tag.fields["type"];
		String id = next_tag.fields["id"];
		ResourceUID::ID uid = ResourceUID::INVALID_ID;
		if (next_tag.fields.has("uid")) {
			String uidt = next_tag.fields["uid"];
			uid = ResourceUID::get_singleton()->text_to_id(uidt);
		}

		bs_save_unicode_string(wf, type);
		bs_save_unicode_string(wf, path);
		wf->store_64(uid);

		int lindex = dummy_read.external_resources.size();
		Ref<DummyResource> dr;
		dr.instantiate();
		dr->set_path("res://dummy" + itos(lindex)); //anything is good to detect it for saving as external
		dummy_read.external_resources[dr] = lindex;
		dummy_read.rev_external_resources[id] = dr;

		error = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &rp_new);

		if (error) {
			_printerr();
			return error;
		}
	}

	// save external resource table
	wf->seek(ext_res_count_pos);
	wf->store_32(dummy_read.external_resources.size());
	wf->seek_end();

	//now, save resources to a separate file, for now

	uint64_t sub_res_count_pos = wf->get_position();
	wf->store_32(0); //zero sub resources, still parsing them

	String temp_file = p_path + ".temp";
	Vector<uint64_t> local_offsets;
	Vector<uint64_t> local_pointers_pos;
	{
		Ref<FileAccess> wf2 = FileAccess::open(temp_file, FileAccess::WRITE);
		if (wf2.is_null()) {
			return ERR_CANT_OPEN;
		}

		while (next_tag.name == "sub_resource" || next_tag.name == "resource") {
			String type;
			String id;
			bool main_res;

			if (next_tag.name == "sub_resource") {
				if (!next_tag.fields.has("type")) {
					error = ERR_FILE_CORRUPT;
					error_text = "Missing 'type' in external resource tag";
					_printerr();
					return error;
				}

				if (!next_tag.fields.has("id")) {
					error = ERR_FILE_CORRUPT;
					error_text = "Missing 'id' in external resource tag";
					_printerr();
					return error;
				}

				type = next_tag.fields["type"];
				id = next_tag.fields["id"];
				main_res = false;

				if (!dummy_read.resource_map.has(id)) {
					Ref<DummyResource> dr;
					dr.instantiate();
					dr->set_scene_unique_id(id);
					dummy_read.resource_map[id] = dr;
					uint32_t im_size = dummy_read.resource_index_map.size();
					dummy_read.resource_index_map.insert(dr, im_size);
				}

			} else {
				type = res_type;
				String uid_text = ResourceUID::get_singleton()->id_to_text(res_uid);
				id = type + "_" + uid_text.replace("uid://", "").replace("<invalid>", "0");
				main_res = true;
			}

			local_offsets.push_back(wf2->get_position());

			bs_save_unicode_string(wf, "local://" + id);
			local_pointers_pos.push_back(wf->get_position());
			wf->store_64(0); //temp local offset

			bs_save_unicode_string(wf2, type);
			uint64_t propcount_ofs = wf2->get_position();
			wf2->store_32(0);

			int prop_count = 0;

			while (true) {
				String assign;
				Variant value;

				error = VariantParserCompat::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp_new);

				if (error) {
					if (main_res && error == ERR_FILE_EOF) {
						next_tag.name = ""; //exit
						break;
					}

					_printerr();
					return error;
				}

				if (!assign.is_empty()) {
					HashMap<StringName, int> empty_string_map; //unused
					bs_save_unicode_string(wf2, assign, true);
					ResourceFormatSaverBinaryInstance::write_variant(wf2, value, dummy_read.resource_index_map, dummy_read.external_resources, empty_string_map);
					prop_count++;

				} else if (!next_tag.name.is_empty()) {
					error = OK;
					break;
				} else {
					error = ERR_FILE_CORRUPT;
					error_text = "Premature end of file while parsing [sub_resource]";
					_printerr();
					return error;
				}
			}

			wf2->seek(propcount_ofs);
			wf2->store_32(prop_count);
			wf2->seek_end();
		}

		if (next_tag.name == "node") {
			// This is a node, must save one more!

			if (!is_scene) {
				error_text += "found the 'node' tag on a resource file!";
				_printerr();
				error = ERR_FILE_CORRUPT;
				return error;
			}

			Ref<PackedScene> packed_scene = _parse_node_tag(rp_new);

			if (!packed_scene.is_valid()) {
				return error;
			}

			error = OK;
			//get it here
			List<PropertyInfo> props;
			packed_scene->get_property_list(&props);

			String id = "PackedScene_" + ResourceUID::get_singleton()->id_to_text(res_uid).replace("uid://", "").replace("<invalid>", "0");
			bs_save_unicode_string(wf, "local://" + id);
			local_pointers_pos.push_back(wf->get_position());
			wf->store_64(0); //temp local offset

			local_offsets.push_back(wf2->get_position());
			bs_save_unicode_string(wf2, "PackedScene");
			uint64_t propcount_ofs = wf2->get_position();
			wf2->store_32(0);

			int prop_count = 0;

			for (const PropertyInfo &E : props) {
				if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
					continue;
				}

				String name = E.name;
				Variant value = packed_scene->get(name);

				HashMap<StringName, int> empty_string_map; //unused
				bs_save_unicode_string(wf2, name, true);
				ResourceFormatSaverBinaryInstance::write_variant(wf2, value, dummy_read.resource_index_map, dummy_read.external_resources, empty_string_map);
				prop_count++;
			}

			wf2->seek(propcount_ofs);
			wf2->store_32(prop_count);
			wf2->seek_end();
		}
	}

	uint64_t offset_from = wf->get_position();
	wf->seek(sub_res_count_pos); //plus one because the saved one
	wf->store_32(local_offsets.size());

	for (int i = 0; i < local_offsets.size(); i++) {
		wf->seek(local_pointers_pos[i]);
		wf->store_64(local_offsets[i] + offset_from);
	}

	wf->seek_end();

	Vector<uint8_t> data = FileAccess::get_file_as_bytes(temp_file);
	wf->store_buffer(data.ptr(), data.size());
	{
		Ref<DirAccess> dar = DirAccess::open(temp_file.get_base_dir());
		ERR_FAIL_COND_V(dar.is_null(), FAILED);

		dar->remove(temp_file);
	}

	wf->store_buffer((const uint8_t *)"RSRC", 4); //magic at end

	return OK;
#endif
	return ERR_UNAVAILABLE; // Don't use this function; do a fake load with the binary loader and use the text saver.
}

Error ResourceLoaderCompatText::get_classes_used(HashSet<StringName> *r_classes) {
	if (error) {
		return error;
	}

	ignore_resource_parsing = true;

	DummyReadData dummy_read;
	dummy_read.no_placeholders = true;
	VariantParser::ResourceParser rp_new;
	rp_new.ext_func = _parse_ext_resource_dummys;
	rp_new.sub_func = _parse_sub_resource_dummys;
	rp_new.userdata = &dummy_read;

	while (next_tag.name == "ext_resource") {
		error = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &rp_new);

		if (error) {
			_printerr();
			return error;
		}
	}

	while (next_tag.name == "sub_resource" || next_tag.name == "resource") {
		if (next_tag.name == "sub_resource") {
			if (!next_tag.fields.has("type")) {
				error = ERR_FILE_CORRUPT;
				error_text = "Missing 'type' in external resource tag";
				_printerr();
				return error;
			}

			r_classes->insert(next_tag.fields["type"]);

		} else {
			r_classes->insert(next_tag.fields["res_type"]);
		}

		while (true) {
			String assign;
			Variant value;

			error = VariantParserCompat::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp_new);

			if (error) {
				if (error == ERR_FILE_EOF) {
					return OK;
				}

				_printerr();
				return error;
			}

			if (!assign.is_empty()) {
				continue;
			} else if (!next_tag.name.is_empty()) {
				error = OK;
				break;
			} else {
				error = ERR_FILE_CORRUPT;
				error_text = "Premature end of file while parsing [sub_resource]";
				_printerr();
				return error;
			}
		}
	}

	while (next_tag.name == "node") {
		// This is a node, must save one more!

		if (!is_scene) {
			error_text += "found the 'node' tag on a resource file!";
			_printerr();
			error = ERR_FILE_CORRUPT;
			return error;
		}

		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return error;
		}

		r_classes->insert(next_tag.fields["type"]);

		while (true) {
			String assign;
			Variant value;

			error = VariantParserCompat::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp_new);

			if (error) {
				if (error == ERR_FILE_MISSING_DEPENDENCIES) {
					// Resource loading error, just skip it.
				} else if (error != ERR_FILE_EOF) {
					_printerr();
					return error;
				} else {
					return OK;
				}
			}

			if (!assign.is_empty()) {
				continue;
			} else if (!next_tag.name.is_empty()) {
				error = OK;
				break;
			} else {
				error = ERR_FILE_CORRUPT;
				error_text = "Premature end of file while parsing [sub_resource]";
				_printerr();
				return error;
			}
		}
	}

	return OK;
}

String ResourceLoaderCompatText::recognize_script_class(Ref<FileAccess> p_f) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;

	ignore_resource_parsing = true;

	VariantParser::Tag tag;
	Error err = VariantParserCompat::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		_printerr();
		return "";
	}

	if (tag.fields.has("format")) {
		int fmt = tag.fields["format"];
		if (fmt > FORMAT_VERSION) {
			error_text = "Saved with newer format version";
			_printerr();
			return "";
		}
	}

	if (tag.name != "gd_resource") {
		return "";
	}

	if (tag.fields.has("script_class")) {
		return tag.fields["script_class"];
	}

	return "";
}

String ResourceLoaderCompatText::recognize(Ref<FileAccess> p_f) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;

	ignore_resource_parsing = true;

	VariantParser::Tag tag;
	Error err = VariantParserCompat::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		_printerr();
		return "";
	}

	if (tag.fields.has("format")) {
		int fmt = tag.fields["format"];
		if (fmt > FORMAT_VERSION) {
			error_text = "Saved with newer format version";
			_printerr();
			return "";
		}
	}

	if (tag.name == "gd_scene") {
		return "PackedScene";
	}

	if (tag.name != "gd_resource") {
		return "";
	}

	if (!tag.fields.has("type")) {
		error_text = "Missing 'type' field in 'gd_resource' tag";
		_printerr();
		return "";
	}

	return tag.fields["type"];
}

ResourceUID::ID ResourceLoaderCompatText::get_uid(Ref<FileAccess> p_f) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;

	ignore_resource_parsing = true;

	VariantParser::Tag tag;
	Error err = VariantParserCompat::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		_printerr();
		return ResourceUID::INVALID_ID;
	}

	if (tag.fields.has("uid")) { //field is optional
		String uidt = tag.fields["uid"];
		return ResourceUID::get_singleton()->text_to_id(uidt);
	}

	return ResourceUID::INVALID_ID;
}

/////////////////////

Ref<Resource> ResourceFormatCompatLoaderText::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	Error err;

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), "Cannot open file '" + p_path + "'.");

	ResourceLoaderCompatText loader;
	String path = !p_original_path.is_empty() ? p_original_path : p_path;
	loader.cache_mode = p_cache_mode;
	loader.use_sub_threads = p_use_sub_threads;
	loader.local_path = GDRESettings::get_singleton()->localize_path(path);
	loader.progress = r_progress;
	loader.res_path = loader.local_path;
	loader.open(f);
	err = loader.load();
	if (r_error) {
		*r_error = err;
	}
	if (err == OK) {
		return loader.get_resource();
	} else {
		return Ref<Resource>();
	}
}

void ResourceFormatCompatLoaderText::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (p_type.is_empty()) {
		get_recognized_extensions(p_extensions);
		return;
	}

	if (ClassDB::is_parent_class("PackedScene", p_type)) {
		p_extensions->push_back("tscn");
	}

	// Don't allow .tres for PackedScenes.
	if (p_type != "PackedScene") {
		p_extensions->push_back("tres");
	}
}

void ResourceFormatCompatLoaderText::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("tscn");
	p_extensions->push_back("tres");
}

bool ResourceFormatCompatLoaderText::handles_type(const String &p_type) const {
	return true;
}

void ResourceFormatCompatLoaderText::get_classes_used(const String &p_path, HashSet<StringName> *r_classes) {
	String ext = p_path.get_extension().to_lower();
	if (ext == "tscn") {
		r_classes->insert("PackedScene");
	}

	// ...for anything else must test...

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return; // Could not read.
	}

	ResourceLoaderCompatText loader;
	// TODO: revisit this
	loader.local_path = GDRESettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	loader.open(f);
	loader.get_classes_used(r_classes);
}

String ResourceFormatCompatLoaderText::get_resource_type(const String &p_path) const {
	String ext = p_path.get_extension().to_lower();
	if (ext == "tscn") {
		return "PackedScene";
	} else if (ext != "tres") {
		return String();
	}

	// ...for anything else must test...

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ""; //could not read
	}

	ResourceLoaderCompatText loader;
	// TODO: revisit this
	loader.local_path = GDRESettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	String r = loader.recognize(f);
	return ClassDB::get_compatibility_remapped_class(r);
}

String ResourceFormatCompatLoaderText::get_resource_script_class(const String &p_path) const {
	String ext = p_path.get_extension().to_lower();
	if (ext != "tres") {
		return String();
	}

	// ...for anything else must test...

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ""; //could not read
	}

	ResourceLoaderCompatText loader;
	// TODO: revisit this
	loader.local_path = GDRESettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	return loader.recognize_script_class(f);
}

ResourceUID::ID ResourceFormatCompatLoaderText::get_resource_uid(const String &p_path) const {
	String ext = p_path.get_extension().to_lower();

	if (ext != "tscn" && ext != "tres") {
		return ResourceUID::INVALID_ID;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ResourceUID::INVALID_ID; //could not read
	}

	ResourceLoaderCompatText loader;
	// TODO: revisit this
	loader.local_path = GDRESettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	return loader.get_uid(f);
}

void ResourceFormatCompatLoaderText::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		ERR_FAIL();
	}

	ResourceLoaderCompatText loader;
	// TODO: revisit this
	loader.local_path = GDRESettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	loader.get_dependencies(f, p_dependencies, p_add_types);
}

Error ResourceFormatCompatLoaderText::rename_dependencies(const String &p_path, const HashMap<String, String> &p_map) {
	Error err = OK;
	{
		Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
		if (f.is_null()) {
			ERR_FAIL_V(ERR_CANT_OPEN);
		}

		ResourceLoaderCompatText loader;
		// TODO: come back to this later
		loader.local_path = GDRESettings::get_singleton()->localize_path(p_path);
		loader.res_path = loader.local_path;
		err = loader.rename_dependencies(f, p_path, p_map);
	}

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (err == OK && da->file_exists(p_path + ".depren")) {
		da->remove(p_path);
		da->rename(p_path + ".depren", p_path);
	}

	return err;
}

ResourceFormatCompatLoaderText *ResourceFormatCompatLoaderText::singleton = nullptr;

Error ResourceFormatCompatLoaderText::convert_file_to_binary(const String &p_src_path, const String &p_dst_path) {
#if 0
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_src_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_OPEN, "Cannot open file '" + p_src_path + "'.");

	ResourceLoaderCompatText loader;
	const String &path = p_src_path;
	loader.local_path = GDRESettings::get_singleton()->localize_path(path);
	loader.res_path = loader.local_path;
	loader.open(f);
	return loader.save_as_binary(p_dst_path);
#endif
	return ERR_UNAVAILABLE; // Don't use this function; do a fake load with the binary loader and use the text saver.
}

/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/
/*****************************************************************************************************/

String ResourceFormatSaverCompatTextInstance::_write_resources(void *ud, const Ref<Resource> &p_resource) {
	ResourceFormatSaverCompatTextInstance *rsi = static_cast<ResourceFormatSaverCompatTextInstance *>(ud);
	return rsi->_write_resource(p_resource);
}

String ResourceFormatSaverCompatTextInstance::_write_resource(const Ref<Resource> &res) {
	if (res->get_meta(SNAME("_skip_save_"), false)) {
		return "null";
	}

	String prefix;
	String id;
	String suffix = ver_format >= 3 ? "\")" : " )";
	// Godot 3.x had a format like this (note the spaces):
	// ExtResource( 1 )
	// Godot 4.x has a format like this:
	// ExtResource("2_dhasdh")
	if (external_resources.has(res)) {
		prefix = "ExtResource(" + String(ver_format >= 3 ? "\"" : " ");
		id = external_resources[res];
	} else if (internal_resources.has(res)) {
		prefix = "SubResource(" + String(ver_format >= 3 ? "\"" : " ");
		id = internal_resources[res];
	} else if (!res->is_built_in()) {
		if (res->get_path() == local_path) { //circular reference attempt
			return "null";
		}
		//external resource
		String path = relative_paths ? local_path.path_to_file(res->get_path()) : res->get_path();
		prefix = "Resource(" + String(ver_format >= 3 ? "" : " ");
		suffix = ver_format >= 3 ? suffix : " )";
		id = "\"" + path + "\"";
	} else {
		ERR_FAIL_V_MSG("null", "Resource was not pre cached for the resource section, bug?");
		//internal resource
	}

	return prefix + id + suffix;
#if 0
	if (external_resources.has(res)) {
		return "ExtResource(\"" + external_resources[res] + "\")";
	} else {
		if (internal_resources.has(res)) {
			return "SubResource(\"" + internal_resources[res] + "\")";
		} else if (!res->is_built_in()) {
			if (res->get_path() == local_path) { //circular reference attempt
				return "null";
			}
			//external resource
			String path = relative_paths ? local_path.path_to_file(res->get_path()) : res->get_path();
			return "Resource(\"" + path + "\")";
		} else {
			ERR_FAIL_V_MSG("null", "Resource was not pre cached for the resource section, bug?");
			//internal resource
		}
	}
#endif
}

// TODO: this could potentially break things if we start messing with resources (renaming dependencies, replacing them, etc.) more than we are currently.
String ResourceFormatSaverCompatTextInstance::get_id_for_ext_resource(Ref<Resource> res, int ext_resources_size) {
	Dictionary dict = res->get_meta(META_COMPAT, Dictionary());
	String id = dict.get("cached_id", String());
	if (id.is_empty()) {
		if (ver_format >= 3) {
			id = itos(ext_resources_size + 1) + "_" + Resource::generate_scene_unique_id();
		} else {
			id = itos(ext_resources_size);
		}
	}
	return id;
}

void ResourceFormatSaverCompatTextInstance::_find_resources(const Variant &p_variant, bool p_main) {
	switch (p_variant.get_type()) {
		case Variant::OBJECT: {
			Ref<Resource> res = p_variant;

			if (res.is_null() || external_resources.has(res) || res->get_meta(SNAME("_skip_save_"), false)) {
				return;
			}

			if (!p_main && (!bundle_resources) && !res->is_built_in()) {
				if (res->get_path() == local_path) {
					ERR_PRINT("Circular reference to resource being saved found: '" + local_path + "' will be null next time it's loaded.");
					return;
				}

				// Use a numeric ID as a base, because they are sorted in natural order before saving.
				// This increases the chances of thread loading to fetch them first.
#if 0
				String id = itos(external_resources.size() + 1) + "_" + Resource::generate_scene_unique_id();
#endif
				String id = get_id_for_ext_resource(res, external_resources.size());
				external_resources[res] = id;
				return;
			}

			if (resource_set.has(res)) {
				return;
			}

			resource_set.insert(res);

			List<PropertyInfo> property_list;

			res->get_property_list(&property_list);
			property_list.sort();

			List<PropertyInfo>::Element *I = property_list.front();

			while (I) {
				PropertyInfo pi = I->get();

				if (pi.usage & PROPERTY_USAGE_STORAGE) {
					Variant v = res->get(I->get().name);

					if (pi.usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
						NonPersistentKey npk;
						npk.base = res;
						npk.property = pi.name;
						non_persistent_map[npk] = v;

						Ref<Resource> sres = v;
						if (sres.is_valid()) {
							resource_set.insert(sres);
							saved_resources.push_back(sres);
						} else {
							_find_resources(v);
						}
					} else {
						_find_resources(v);
					}
				}

				I = I->next();
			}

			saved_resources.push_back(res); // Saved after, so the children it needs are available when loaded

		} break;
		case Variant::ARRAY: {
			Array varray = p_variant;
			int len = varray.size();
			for (int i = 0; i < len; i++) {
				const Variant &v = varray.get(i);
				_find_resources(v);
			}

		} break;
		case Variant::DICTIONARY: {
			Dictionary d = p_variant;
			List<Variant> keys;
			d.get_key_list(&keys);
			for (const Variant &E : keys) {
				// Of course keys should also be cached, after all we can't prevent users from using resources as keys, right?
				// See also ResourceFormatSaverBinaryInstance::_find_resources (when p_variant is of type Variant::DICTIONARY)
				_find_resources(E);
				Variant v = d[E];
				_find_resources(v);
			}
		} break;
		default: {
		}
	}
}

static String _resource_get_class(Ref<Resource> p_resource) {
	Ref<MissingResource> missing_resource = p_resource;
	if (missing_resource.is_valid()) {
		return missing_resource->get_original_class();
	} else {
		return p_resource->get_class();
	}
}

/* this is really only appropriate for saving fake-loaded resources right now; don't use it to save anything else*/
Error ResourceFormatSaverCompatTextInstance::save(const String &p_path, const Ref<Resource> &p_resource, uint32_t p_flags) {
#if 0
	if (p_path.ends_with(".tscn")) {
		packed_scene = p_resource;
	}
#endif
	Dictionary compat = p_resource->get_meta("compat");
	if (compat.is_empty()) {
		WARN_PRINT("Resource does not have compat metadata set?!?!?!?!");
		ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Resource does not have compat metadata set?!?!?!?!");
	}

	if (p_path.ends_with(".tscn") || p_path.ends_with(".escn")) {
		// If this is a MissingResource holder for a PackedScene, we need to instance it for reals
		if (p_resource->get_class() == "MissingResource" && p_resource->get_save_class() == "PackedScene") {
			Dictionary bundle = p_resource->get("_bundled");
			packed_scene = Ref<PackedScene>(memnew(PackedScene));
			packed_scene->set("_bundled", bundle);
		} else {
			packed_scene = p_resource;
		}
	}

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, ERR_CANT_OPEN, "Cannot save file '" + p_path + "'.");
	Ref<FileAccess> _fref(f);

	local_path = GDRESettings::get_singleton()->localize_path(p_path);

	ver_format = compat.get("ver_format", 0);
	ver_major = compat.get("ver_major", 0);
	ver_minor = compat.get("ver_minor", 0);
	ResourceUID::ID res_uid = compat.get("uid", ResourceUID::INVALID_ID);
	String format = compat.get("resource_format", "binary");
	bool using_script_class = compat.get("using_script_class", false);
	String script_class = compat.get("script_class", String());
	bool using_uids = compat.get("using_uids", false);
	bool using_named_scene_ids = compat.get("using_named_scene_ids", false);
	Ref<ResourceImportMetadatav2> imd = compat.get("import_metadata", Ref<ResourceImportMetadatav2>());
	if (format != "text") {
		if (using_uids || using_named_scene_ids || using_script_class) {
			ver_format = 3;
		} else {
			switch (ver_major) {
				case 2:
					ver_format = 1;
					break;
				case 3:
					ver_format = 2;
					break;
				case 4:
					ver_format = 3;
					break;
				default:
					ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid version major: " + itos(ver_major));
					break;
			}
		}
	}
#if 0
	relative_paths = p_flags & ResourceSaver::FLAG_RELATIVE_PATHS;
	skip_editor = p_flags & ResourceSaver::FLAG_OMIT_EDITOR_PROPERTIES;
	bundle_resources = p_flags & ResourceSaver::FLAG_BUNDLE_RESOURCES;
	takeover_paths = p_flags & ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;
#endif
	if (!p_path.begins_with("res://")) {
		takeover_paths = false;
	}

	// Save resources.
	_find_resources(p_resource, true);

	if (packed_scene.is_valid()) {
		// Add instances to external resources if saving a packed scene.
		for (int i = 0; i < packed_scene->get_state()->get_node_count(); i++) {
			if (packed_scene->get_state()->is_node_instance_placeholder(i)) {
				continue;
			}

			Ref<PackedScene> instance = packed_scene->get_state()->get_node_instance(i);
			if (instance.is_valid() && !external_resources.has(instance)) {
#if 0
				int index = external_resources.size() + 1;
				external_resources[instance] = itos(index) + "_" + Resource::generate_scene_unique_id(); // Keep the order for improved thread loading performance.
#endif
				external_resources[instance] = get_id_for_ext_resource(instance, external_resources.size());
			}
		}
	}

	{
		String title = packed_scene.is_valid() ? "[gd_scene " : "[gd_resource ";
		if (packed_scene.is_null()) {
			title += "type=\"" + _resource_get_class(p_resource) + "\" ";
			if (!script_class.is_empty()) {
				title += "script_class=\"" + script_class + "\" ";
			}
#if 0
			Ref<Script> script = p_resource->get_script();
			if (script.is_valid() && script->get_global_name()) {
				title += "script_class=\"" + String(script->get_global_name()) + "\" ";
			}
#endif
		}

		int load_steps = saved_resources.size() + external_resources.size();

		if (load_steps > 1) {
			title += "load_steps=" + itos(load_steps) + " ";
		}
		title += "format=" + itos(FORMAT_VERSION) + "";

#if 0
		ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(local_path, true);
#endif
		ResourceUID::ID uid = res_uid;

		if (uid != ResourceUID::INVALID_ID) {
			title += " uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\"";
		}

		f->store_string(title);
		f->store_line("]\n"); // One empty line.
	}

#ifdef TOOLS_ENABLED
	// Keep order from cached ids.
	HashSet<String> cached_ids_found;
	// clang-format off
	if (ver_format >= 3){
	for (KeyValue<Ref<Resource>, String> &E : external_resources) {
		Dictionary compat = E.key->get_meta("compat", Dictionary());
		String sc_id = compat.get("cached_id", String());
		if (!sc_id.is_empty()) {
			E.value = sc_id;
			cached_ids_found.insert(sc_id);
			continue;
		}
		String cached_id = E.key->get_id_for_path(local_path);
		if (cached_id.is_empty() || cached_ids_found.has(cached_id)) {
			int sep_pos = E.value.find("_");
			if (sep_pos != -1) {
				E.value = E.value.substr(0, sep_pos + 1); // Keep the order found, for improved thread loading performance.
			} else {
				E.value = "";
			}

		} else {
			E.value = cached_id;
			cached_ids_found.insert(cached_id);
		}
	}
	// Create IDs for non cached resources.
	for (KeyValue<Ref<Resource>, String> &E : external_resources) {
		if (cached_ids_found.has(E.value)) { // Already cached, go on.
			continue;
		}

		String attempt;
		while (true) {
			attempt = E.value + Resource::generate_scene_unique_id();
			if (!cached_ids_found.has(attempt)) {
				break;
			}
		}

		cached_ids_found.insert(attempt);
		E.value = attempt;
		// Update also in resource.
		Ref<Resource> res = E.key;
		res->set_id_for_path(local_path, attempt);
	}
	}
	// clang-format on
#else
	// Make sure to start from one, as it makes format more readable.
	int counter = 1;
	// Actually, no, don't do that you idiot, it breaks everything
	counter = ver_format >= 3 ? 1 : 0;
	for (KeyValue<Ref<Resource>, String> &E : external_resources) {
		Dictionary compat = E.key->get_meta("compat", Dictionary());
		String sc_id = compat.get("cached_id", String());
		if (!sc_id.is_empty()) {
			E.value = sc_id;
			counter++;
			continue;
		}

		E.value = itos(counter++);
	}
#endif

	Vector<ResourceSort> sorted_er;

	for (const KeyValue<Ref<Resource>, String> &E : external_resources) {
		ResourceSort rs;
		rs.resource = E.key;
		rs.id = E.value;
		sorted_er.push_back(rs);
	}

	sorted_er.sort();

	for (int i = 0; i < sorted_er.size(); i++) {
		String p = sorted_er[i].resource->get_path();

		String s = "[ext_resource type=\"" + sorted_er[i].resource->get_save_class() + "\"";
		Dictionary compat = sorted_er[i].resource->get_meta("compat", Dictionary());
		ResourceUID::ID uid = compat.get("uid", ResourceUID::INVALID_ID);
		if (ver_format >= 3 && uid != ResourceUID::INVALID_ID) {
			s += " uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\"";
		}
		s += " path=\"" + p + "\" id=";
		// IDs are strings in format 3
		if (ver_format >= 3) {
			s += "\"" + sorted_er[i].id + "\"]\n";
		} else {
			s += sorted_er[i].id + "]\n";
		}

#if 0
		ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(p, false);
		if (uid != ResourceUID::INVALID_ID) {
			s += " uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\"";
		}
		s += " path=\"" + p + "\" id=\"" + sorted_er[i].id + "\"]\n";
#endif
		f->store_string(s); // Bundled.
	}

	if (external_resources.size()) {
		f->store_line(String()); // Separate.
	}

	HashSet<String> used_unique_ids;

	for (List<Ref<Resource>>::Element *E = saved_resources.front(); E; E = E->next()) {
		Ref<Resource> res = E->get();
		if (E->next() && res->is_built_in()) {
			if (!res->get_scene_unique_id().is_empty()) {
				if (used_unique_ids.has(res->get_scene_unique_id())) {
					res->set_scene_unique_id(""); // Repeated.
				} else {
					used_unique_ids.insert(res->get_scene_unique_id());
				}
			}
		}
	}

	for (List<Ref<Resource>>::Element *E = saved_resources.front(); E; E = E->next()) {
		Ref<Resource> res = E->get();
		ERR_CONTINUE(!resource_set.has(res));
		bool main = (E->next() == nullptr);

		if (main && packed_scene.is_valid()) {
			break; // Save as a scene.
		}

		if (main) {
			f->store_line("[resource]");
		} else {
			String line = "[sub_resource ";
			if (res->get_scene_unique_id().is_empty()) {
				String new_id;
				while (true) {
					new_id = _resource_get_class(res) + "_" + Resource::generate_scene_unique_id();

					if (!used_unique_ids.has(new_id)) {
						break;
					}
				}

				res->set_scene_unique_id(new_id);
				used_unique_ids.insert(new_id);
			}

			String id = res->get_scene_unique_id();
			line += "type=\"" + _resource_get_class(res) + "\" id=\"" + id;
			f->store_line(line + "\"]");
			if (takeover_paths) {
				res->set_path(p_path + "::" + id, true);
			}

			internal_resources[res] = id;
#ifdef TOOLS_ENABLED
			res->set_edited(false);
#endif
		}

		Dictionary missing_resource_properties = p_resource->get_meta(META_MISSING_RESOURCES, Dictionary());

		List<PropertyInfo> property_list;
		res->get_property_list(&property_list);
		for (List<PropertyInfo>::Element *PE = property_list.front(); PE; PE = PE->next()) {
			if (skip_editor && PE->get().name.begins_with("__editor")) {
				continue;
			}
			if (PE->get().name == META_PROPERTY_COMPAT_DATA) {
				continue;
			}
			if (PE->get().name == META_PROPERTY_MISSING_RESOURCES) {
				continue;
			}

			if (PE->get().usage & PROPERTY_USAGE_STORAGE || missing_resource_properties.has(PE->get().name)) {
				String name = PE->get().name;
				Variant value;
				if (PE->get().usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
					NonPersistentKey npk;
					npk.base = res;
					npk.property = name;
					if (non_persistent_map.has(npk)) {
						value = non_persistent_map[npk];
					}
				} else {
					value = res->get(name);
				}

				if (PE->get().type == Variant::OBJECT && missing_resource_properties.has(PE->get().name)) {
					// Was this missing resource overridden? If so do not save the old value.
					Ref<Resource> ures = value;
					if (ures.is_null()) {
						value = missing_resource_properties[PE->get().name];
					}
				}

#if 0
				Variant default_value = ClassDB::class_get_default_property_value(res->get_class(), name);
#endif
				Variant default_value = Variant(); // save all properties, even default ones
				// Except for the default "Resource" properties
				if (PE->get().name == "resource_name") {
					default_value = "";
				} else if (PE->get().name == "resource_local_to_scene") {
					default_value = false;
				}

				if (default_value.get_type() != Variant::NIL && bool(Variant::evaluate(Variant::OP_EQUAL, value, default_value))) {
					continue;
				}

				if (PE->get().type == Variant::OBJECT && value.is_zero() && !(PE->get().usage & PROPERTY_USAGE_STORE_IF_NULL)) {
					continue;
				}

				String vars;
				VariantWriterCompat::write_to_string(value, vars, ver_major, _write_resources, this);
				f->store_string(name.property_name_encode() + " = " + vars + "\n");
			}
		}

		if (E->next()) {
			f->store_line(String());
		}
	}

	if (packed_scene.is_valid()) {
		// If this is a scene, save nodes and connections!
		Ref<SceneState> state = packed_scene->get_state();
		for (int i = 0; i < state->get_node_count(); i++) {
			StringName type = state->get_node_type(i);
			StringName name = state->get_node_name(i);
			int index = state->get_node_index(i);
			NodePath path = state->get_node_path(i, true);
			NodePath owner = state->get_node_owner_path(i);
			Ref<PackedScene> instance = state->get_node_instance(i);
			String instance_placeholder = state->get_node_instance_placeholder(i);
			Vector<StringName> groups = state->get_node_groups(i);
			Vector<String> deferred_node_paths = state->get_node_deferred_nodepath_properties(i);

			String header = "[node";
			header += " name=\"" + String(name).c_escape() + "\"";
			if (type != StringName()) {
				header += " type=\"" + String(type) + "\"";
			}
			if (path != NodePath()) {
				header += " parent=\"" + String(path.simplified()).c_escape() + "\"";
			}
			if (owner != NodePath() && owner != NodePath(".")) {
				header += " owner=\"" + String(owner.simplified()).c_escape() + "\"";
			}
			if (index >= 0) {
				header += " index=\"" + itos(index) + "\"";
			}

			// clang-format off
			if (ver_format >= 3) {
			if (deferred_node_paths.size()) {
				header += " node_paths=" + Variant(deferred_node_paths).get_construct_string();
			}
			}
			// clang-format on

			if (groups.size()) {
				// Write all groups on the same line as they're part of a section header.
				// This improves readability while not impacting VCS friendliness too much,
				// since it's rare to have more than 5 groups assigned to a single node.
				groups.sort_custom<StringName::AlphCompare>();
				String sgroups = " groups=[";
				for (int j = 0; j < groups.size(); j++) {
					sgroups += "\"" + String(groups[j]).c_escape() + "\"";
					if (j < groups.size() - 1) {
						sgroups += ", ";
					}
				}
				sgroups += "]";
				header += sgroups;
			}

			f->store_string(header);

			if (!instance_placeholder.is_empty()) {
				String vars;
				f->store_string(" instance_placeholder=");
				VariantWriterCompat::write_to_string(instance_placeholder, vars, ver_major, _write_resources, this);
				f->store_string(vars);
			}

			if (instance.is_valid()) {
				String vars;
				f->store_string(" instance=");
				VariantWriterCompat::write_to_string(instance, vars, ver_major, _write_resources, this);
				f->store_string(vars);
			}

			f->store_line("]");

			for (int j = 0; j < state->get_node_property_count(i); j++) {
				String vars;
				VariantWriterCompat::write_to_string(state->get_node_property_value(i, j), vars, ver_major, _write_resources, this);

				f->store_string(String(state->get_node_property_name(i, j)).property_name_encode() + " = " + vars + "\n");
			}

			if (i < state->get_node_count() - 1) {
				f->store_line(String());
			}
		}

		for (int i = 0; i < state->get_connection_count(); i++) {
			if (i == 0) {
				f->store_line("");
			}

			String connstr = "[connection";
			connstr += " signal=\"" + String(state->get_connection_signal(i)) + "\"";
			connstr += " from=\"" + String(state->get_connection_source(i).simplified()) + "\"";
			connstr += " to=\"" + String(state->get_connection_target(i).simplified()) + "\"";
			connstr += " method=\"" + String(state->get_connection_method(i)) + "\"";
			int flags = state->get_connection_flags(i);
			if (flags != Object::CONNECT_PERSIST) {
				connstr += " flags=" + itos(flags);
			}

			int unbinds = state->get_connection_unbinds(i);
			if (unbinds > 0) {
				connstr += " unbinds=" + itos(unbinds);
			}

			Array binds = state->get_connection_binds(i);
			f->store_string(connstr);
			if (binds.size()) {
				String vars;
				VariantWriterCompat::write_to_string(binds, vars, ver_major, _write_resources, this);
				f->store_string(" binds= " + vars);
			}

			f->store_line("]");
		}

		Vector<NodePath> editable_instances = state->get_editable_instances();
		for (int i = 0; i < editable_instances.size(); i++) {
			if (i == 0) {
				f->store_line("");
			}
			f->store_line("[editable path=\"" + editable_instances[i].operator String() + "\"]");
		}
	}

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error ResourceLoaderCompatText::set_uid(Ref<FileAccess> p_f, ResourceUID::ID p_uid) {
	open(p_f, true);
	ERR_FAIL_COND_V(error != OK, error);
	ignore_resource_parsing = true;

	if (ver_format < 3) {
		WARN_PRINT("Cannot set UID on format version < 3");
		return ERR_UNAVAILABLE;
	}
	Ref<FileAccess> fw;

	fw = FileAccess::open(local_path + ".uidren", FileAccess::WRITE);
	if (is_scene) {
		fw->store_string("[gd_scene load_steps=" + itos(resources_total) + " format=" + itos(ver_format) + " uid=\"" + ResourceUID::get_singleton()->id_to_text(p_uid) + "\"]");
	} else {
		String script_res_text;
		if (!script_class.is_empty()) {
			script_res_text = "script_class=\"" + script_class + "\" ";
		}

		fw->store_string("[gd_resource type=\"" + res_type + "\" " + script_res_text + "load_steps=" + itos(resources_total) + " format=" + itos(ver_format) + " uid=\"" + ResourceUID::get_singleton()->id_to_text(p_uid) + "\"]");
	}

	uint8_t c = f->get_8();
	while (!f->eof_reached()) {
		fw->store_8(c);
		c = f->get_8();
	}

	bool all_ok = fw->get_error() == OK;

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

/* this is really only appropriate for saving fake-loaded resources right now; don't use it to save anything else*/
Error ResourceFormatSaverCompatText::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	if (p_path.ends_with(".tscn") && !Ref<PackedScene>(p_resource).is_valid()) {
		return ERR_FILE_UNRECOGNIZED;
	}

	ResourceFormatSaverCompatTextInstance saver;
	return saver.save(p_path, p_resource, p_flags);
}

Error ResourceFormatSaverCompatText::set_uid(const String &p_path, ResourceUID::ID p_uid) {
	String lc = p_path.to_lower();
	if (!lc.ends_with(".tscn") && !lc.ends_with(".tres")) {
		return ERR_FILE_UNRECOGNIZED;
	}

	String local_path = GDRESettings::get_singleton()->localize_path(p_path);
	Error err = OK;
	{
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
		if (file.is_null()) {
			ERR_FAIL_V(ERR_CANT_OPEN);
		}

		ResourceLoaderCompatText loader;
		loader.local_path = local_path;
		loader.res_path = loader.local_path;
		err = loader.set_uid(file, p_uid);
	}

	if (err == OK) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		da->remove(local_path);
		da->rename(local_path + ".uidren", local_path);
	}

	return err;
}

bool ResourceFormatSaverCompatText::recognize(const Ref<Resource> &p_resource) const {
	return true; // All resources recognized!
}

void ResourceFormatSaverCompatText::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Ref<PackedScene>(p_resource).is_valid()) {
		p_extensions->push_back("tscn"); // Text scene.
	} else {
		p_extensions->push_back("tres"); // Text resource.
	}
}

// ResourceFormatSaverCompatText *ResourceFormatSaverCompatText::singleton = nullptr;
ResourceFormatSaverCompatText::ResourceFormatSaverCompatText() {
	// singleton = this;
}

void ResourceLoaderCompatText::set_compat_meta(Ref<Resource> &r_res) {
	Dictionary compat = get_resource_info();
	compat["main_resource"] = true;
	if (is_scene) {
		compat["packed_scene_version"] = packed_scene_version;
	}
	r_res->set_meta(META_COMPAT, compat);
}

Ref<ResourceLoader::LoadToken> ResourceLoaderCompatText::start_ext_load(const String &p_path, const String &p_type_hint, const ResourceUID::ID uid, const String id) {
	Ref<ResourceLoader::LoadToken> load_token;
	if (!is_real_load()) {
		Error err;
		load_token = Ref<ResourceLoader::LoadToken>(memnew(ResourceLoader::LoadToken));
		load_token->local_path = p_path;
		load_token->user_path = p_path;
		if (load_type == ResourceCompatLoader::GLTF_LOAD) {
			load_token->res_if_unregistered = ResourceCompatLoader::gltf_load(p_path, p_type_hint, &err);
			if (err != OK || load_token->res_if_unregistered.is_null()) {
				load_token = Ref<ResourceLoader::LoadToken>();
			}
		} else {
			load_token->res_if_unregistered = CompatFormatLoader::create_missing_external_resource(p_path, p_type_hint, uid, id);
		}
	} else { // real load
		load_token = ResourceLoader::_load_start(p_path, p_type_hint, use_sub_threads ? ResourceLoader::LOAD_THREAD_DISTRIBUTE : ResourceLoader::LOAD_THREAD_FROM_CURRENT, ResourceFormatLoader::CACHE_MODE_REUSE);
	}
	return load_token;
}
Ref<Resource> ResourceLoaderCompatText::finish_ext_load(Ref<ResourceLoader::LoadToken> &load_token, Error *r_err) {
	if (load_type == ResourceCompatLoader::REAL_LOAD) {
		return ResourceLoader::_load_complete(*load_token.ptr(), r_err);
	} else if (load_type == ResourceCompatLoader::GLTF_LOAD) {
		// TODO: if implemented multi-threaded GLTF load, do something; for right now it's already loaded
	} // Fake_load, non-global load
	return load_token->res_if_unregistered;
}

Dictionary ResourceLoaderCompatText::get_resource_info() {
	Dictionary info;
	info["uid"] = res_uid;
	info["type"] = res_type;
	info["main_resource"] = true;
	info["ver_major"] = ver_major;
	info["ver_minor"] = ver_minor;
	info["ver_format"] = ver_format;
	info["resource_format"] = "binary";
	info["load_type"] = load_type;
	info["using_script_class"] = !script_class.is_empty();
	// info["using_real_t_double"] = using_real_t_double;
	// info["stored_use_real64"] = stored_use_real64;
	// info["stored_big_endian"] = stored_big_endian;
	info["using_named_scene_ids"] = ver_format >= 3;
	info["using_uids"] = ver_format >= 3;
	info["script_class"] = script_class;
	// info["import_metadata"] = imd;
	return info;
}

Ref<Resource> ResourceFormatCompatLoaderText::custom_load(const String &p_path, ResourceCompatLoader::LoadType p_load_type, Error *r_error) {
	if (r_error) {
		*r_error = ERR_CANT_OPEN;
	}

	Error err;

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), "Cannot open file '" + p_path + "'.");

	ResourceLoaderCompatText loader;
	String path = p_path;
	loader.load_type = p_load_type;
	switch (p_load_type) {
		// TODO: Figure out if we can do caching at all
		case ResourceCompatLoader::FAKE_LOAD:
		case ResourceCompatLoader::NON_GLOBAL_LOAD:
			loader.cache_mode = ResourceFormatLoader::CACHE_MODE_IGNORE;
			loader.use_sub_threads = false;
			break;
		case ResourceCompatLoader::GLTF_LOAD:
			loader.cache_mode = ResourceFormatLoader::CACHE_MODE_REPLACE;
			loader.use_sub_threads = true;
			break;
		case ResourceCompatLoader::REAL_LOAD:
		default:
			loader.cache_mode = ResourceFormatLoader::CACHE_MODE_REPLACE;
			loader.use_sub_threads = true;
			break;
	}
	loader.local_path = GDRESettings::get_singleton()->localize_path(path);
	loader.progress = nullptr;
	loader.res_path = loader.local_path;
	loader.open(f);
	err = loader.load();
	if (r_error) {
		*r_error = err;
	}
	if (err == OK) {
		return loader.get_resource();
	} else {
		return Ref<Resource>();
	}
}
