#include "pcfg_loader.h"
#include <core/os/input_event.h>
#include <core/os/input_event.h>
#include <core/os/keyboard.h>
#include <core/io/compression.h>
#include <core/os/file_access.h>
#include <core/io/marshalls.h>
#include <core/variant_parser.h>

Error ProjectConfigLoader::load_cfb(const String path){
    cfb_path = path;
    return _load_settings_binary(path);
}
Error ProjectConfigLoader::save_cfb(const String dir){
	String file = "project.godot";
	return save_custom(dir.plus_file(file));
}

Error ProjectConfigLoader::_load_settings_binary(const String &p_path) {

	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err != OK) {
		return err;
	}

	uint8_t hdr[4];
	f->get_buffer(hdr, 4);
	if (hdr[0] != 'E' || hdr[1] != 'C' || hdr[2] != 'F' || hdr[3] != 'G') {

		memdelete(f);
		ERR_EXPLAIN("Corrupted header in binary project.binary (not ECFG).");
		ERR_FAIL_V(ERR_FILE_CORRUPT);
	}

	uint32_t count = f->get_32();

	for (uint32_t i = 0; i < count; i++) {

		uint32_t slen = f->get_32();
		CharString cs;
		cs.resize(slen + 1);
		cs[slen] = 0;
		f->get_buffer((uint8_t *)cs.ptr(), slen);
		String key;
		key.parse_utf8(cs.ptr());

		uint32_t vlen = f->get_32();
		Vector<uint8_t> d;
		d.resize(vlen);
		f->get_buffer(d.ptr(), vlen);
		Variant value;
		err = decode_variant(value, d.ptr(), d.size(), NULL);
		ERR_EXPLAIN("Error decoding property: " + key + ".");
		ERR_CONTINUE(err != OK);
		props[key] = VariantContainer(value, last_builtin_order++, true);
	}

	f->close();
	memdelete(f);
	return OK;
}

struct _VCSort {

	String name;
	Variant::Type type;
	int order;
	int flags;

	bool operator<(const _VCSort &p_vcs) const { return order == p_vcs.order ? name < p_vcs.name : order < p_vcs.order; }
};

Error ProjectConfigLoader::save_custom(const String &p_path) {
	ERR_EXPLAIN("Project settings save path cannot be empty.");
	ERR_FAIL_COND_V(p_path == "", ERR_INVALID_PARAMETER);

	Set<_VCSort> vclist;

	for (Map<StringName, VariantContainer>::Element *G = props.front(); G; G = G->next()) {

		const VariantContainer *v = &G->get();

		if (v->hide_from_editor)
			continue;

		_VCSort vc;
		vc.name = G->key(); //*k;
		vc.order = v->order;
		vc.type = v->variant.get_type();
		vc.flags = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
		if (v->variant == v->initial)
			continue;

		vclist.insert(vc);
	}
	Map<String, List<String> > proops;

	for (Set<_VCSort>::Element *E = vclist.front(); E; E = E->next()) {

		String category = E->get().name;
		String name = E->get().name;

		int div = category.find("/");

		if (div < 0)
			category = "";
		else {

			category = category.substr(0, div);
			name = name.substr(div + 1, name.size());
		}
		proops[category].push_back(name);
	}

	return _save_settings_text(p_path, proops);
}

Error ProjectConfigLoader::_save_settings_text(const String &p_file, const Map<String, List<String> > &proops) {

	Error err;
	FileAccess *file = FileAccess::open(p_file, FileAccess::WRITE, &err);
	ERR_EXPLAIN("Couldn't save project.godot - " + p_file + ".")
	ERR_FAIL_COND_V(err != OK, err);

	file->store_line("; Engine configuration file.");
	file->store_line("; It's best edited using the editor UI and not directly,");
	file->store_line("; since the parameters that go here are not all obvious.");
	file->store_line(";");
	file->store_line("; Format:");
	file->store_line(";   [section] ; section goes between []");
	file->store_line(";   param=value ; assign values to parameters");
	file->store_line("");

	file->store_string("config_version=" + itos(4) + "\n");

	file->store_string("\n");

	for (Map<String, List<String> >::Element *E = proops.front(); E; E = E->next()) {

		if (E != proops.front())
			file->store_string("\n");

		if (E->key() != "")
			file->store_string("[" + E->key() + "]\n\n");
		for (List<String>::Element *F = E->get().front(); F; F = F->next()) {

			String key = F->get();
			if (E->key() != "")
				key = E->key() + "/" + key;
			Variant value;
			value = props[key].variant;

			String vstr;
			VariantWriter::write_to_string(value, vstr);
			
			file->store_string(F->get()+ "=" + vstr + "\n");
		}
	}

	file->close();
	memdelete(file);

	return OK;
}



static String _encode_variant(const Variant &p_variant) {

	switch (p_variant.get_type()) {

		case Variant::BOOL: {
			bool val = p_variant;
			return (val ? "true" : "false");
		} break;
		case Variant::INT: {
			int val = p_variant;
			return itos(val);
		} break;
		case Variant::REAL: {
			float val = p_variant;
			return rtos(val) + (val == int(val) ? ".0" : "");
		} break;
		case Variant::VECTOR2: {
			Vector2 val = p_variant;
			return String("Vector2(") + rtos(val.x) + String(", ") + rtos(val.y) + String(")");
		} break;
		case Variant::VECTOR3: {
			Vector3 val = p_variant;
			return String("Vector3(") + rtos(val.x) + String(", ") + rtos(val.y) + String(", ") + rtos(val.z) + String(")");
		} break;
		case Variant::STRING: {
			String val = p_variant;
			return "\"" + val.xml_escape() + "\"";
		} break;
		case Variant::COLOR: {

			Color val = p_variant;
			return "#" + val.to_html();
		} break;
		case Variant::STRING_ARRAY:
		case Variant::INT_ARRAY:
		case Variant::REAL_ARRAY:
		case Variant::ARRAY: {
			Array arr = p_variant;
			String str = "[";
			for (int i = 0; i < arr.size(); i++) {

				if (i > 0)
					str += ", ";
				str += _encode_variant(arr[i]);
			}
			str += "]";
			return str;
		} break;
		case Variant::DICTIONARY: {
			Dictionary d = p_variant;
			String str = "{";
			List<Variant> keys;
			d.get_key_list(&keys);
			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {

				if (E != keys.front())
					str += ", ";
				str += _encode_variant(E->get());
				str += ":";
				str += _encode_variant(d[E->get()]);
			}
			str += "}";
			return str;
		} break;
		case Variant::IMAGE: {
			String str = "img(";

			Image img = p_variant;
			if (!img.empty()) {

				String format;
				switch (img.get_format()) {

					case Image::FORMAT_GRAYSCALE: format = "grayscale"; break;
					case Image::FORMAT_INTENSITY: format = "intensity"; break;
					case Image::FORMAT_GRAYSCALE_ALPHA: format = "grayscale_alpha"; break;
					case Image::FORMAT_RGB: format = "rgb"; break;
					case Image::FORMAT_RGBA: format = "rgba"; break;
					case Image::FORMAT_INDEXED: format = "indexed"; break;
					case Image::FORMAT_INDEXED_ALPHA: format = "indexed_alpha"; break;
					case Image::FORMAT_BC1: format = "bc1"; break;
					case Image::FORMAT_BC2: format = "bc2"; break;
					case Image::FORMAT_BC3: format = "bc3"; break;
					case Image::FORMAT_BC4: format = "bc4"; break;
					case Image::FORMAT_BC5: format = "bc5"; break;
					case Image::FORMAT_CUSTOM: format = "custom custom_size=" + itos(img.get_data().size()) + ""; break;
					default: {
					}
				}

				str += format + ", ";
				str += itos(img.get_mipmaps()) + ", ";
				str += itos(img.get_width()) + ", ";
				str += itos(img.get_height()) + ", ";
				DVector<uint8_t> data = img.get_data();
				int ds = data.size();
				DVector<uint8_t>::Read r = data.read();
				for (int i = 0; i < ds; i++) {
					uint8_t byte = r[i];
					const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
					char bstr[3] = { hex[byte >> 4], hex[byte & 0xF], 0 };
					str += bstr;
				}
			}
			str += ")";
			return str;
		} break;
		case Variant::INPUT_EVENT: {

			InputEvent ev = p_variant;

			switch (ev.type) {

				case InputEvent::KEY: {

					String mods;
					if (ev.key.mod.control)
						mods += "C";
					if (ev.key.mod.shift)
						mods += "S";
					if (ev.key.mod.alt)
						mods += "A";
					if (ev.key.mod.meta)
						mods += "M";
					if (mods != "")
						mods = ", " + mods;

					return "key(" + keycode_get_string(ev.key.scancode) + mods + ")";
				} break;
				case InputEvent::MOUSE_BUTTON: {

					return "mbutton(" + itos(ev.device) + ", " + itos(ev.mouse_button.button_index) + ")";
				} break;
				case InputEvent::JOYSTICK_BUTTON: {

					return "jbutton(" + itos(ev.device) + ", " + itos(ev.joy_button.button_index) + ")";
				} break;
				case InputEvent::JOYSTICK_MOTION: {

					return "jaxis(" + itos(ev.device) + ", " + itos(ev.joy_motion.axis * 2 + (ev.joy_motion.axis_value < 0 ? 0 : 1)) + ")";
				} break;
				default: {

					return "nil";
				} break;
			}
		} break;
		default: {
		}
	}

	return "nil"; //don't know wha to do with this
}


ProjectConfigLoader::ProjectConfigLoader() {
}

ProjectConfigLoader::~ProjectConfigLoader(){
}
