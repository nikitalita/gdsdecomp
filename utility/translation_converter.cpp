#include "translation_converter.h"

#include "utility/gdre_settings.h"
#include "utility/resource_downgrader.h"

#include "core/io/config_file.h"
#include "core/io/file_access_compressed.h"
#include "editor/import/resource_importer_csv_translation.h"

Vector<String> TranslationConverter::convert_translation_file(const String src_path) {
	Vector<String> result;
	Error err;

	uint32_t format_version = UINT32_MAX;

	String out_dir = src_path.get_base_dir();
	Ref<DirAccess> dir = DirAccess::open(out_dir);
	ERR_FAIL_COND_V(dir.is_null(), result);

	err = dir->list_dir_begin();
	ERR_FAIL_COND_V(err, result);
	for (String file_name = dir->get_next(); file_name != ""; file_name = dir->get_next()) {
		if (!dir->current_is_dir() && file_name.get_extension() == "translation") {
			String file_path = out_dir.path_join(file_name);

			Ref<FileAccess> f = FileAccess::open(file_path, FileAccess::READ);
			ERR_FAIL_COND_V_MSG(f.is_null(), Vector<String>(), vformat("Cannot open file '%s'.", file_path));

			uint8_t header[4];
			f->get_buffer(header, 4);
			if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
				// Compressed.
				Ref<FileAccessCompressed> fac;
				fac.instantiate();
				err = fac->open_after_magic(f);
				if (err != OK) {
					continue;
				}
				f = fac;

			} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
				continue;
			}

			f->seek(0x14);
			format_version = f->get_32();
			dir->list_dir_end();
			break;
		}
	}

	ERR_FAIL_COND_V(format_version == UINT32_MAX, result);

	ResourceUID::ID uid = ResourceUID::INVALID_ID;
	HashMap<StringName, Variant> options;
	options["compress"] = true;
	options["delimiter"] = 0;
	{
		Ref<ConfigFile> config;
		config.instantiate();
		err = config->load(src_path + ".import");
		if (err == OK) {
			if (config->has_section_key("remap", "uid")) {
				String uidt = config->get_value("remap", "uid");
				uid = ResourceUID::get_singleton()->text_to_id(uidt);
			}

			if (config->has_section("params")) {
				Vector<String> sk = config->get_section_keys("params");
				for (const String &param : sk) {
					Variant value = config->get_value("params", param);
					options[param] = value;
				}
			}
		}
	}

	Ref<ResourceImporterCSVTranslation> import_csv_translation = memnew(ResourceImporterCSVTranslation);

	List<String> gen_files;
	err = import_csv_translation->import(uid, src_path, "", options, nullptr, &gen_files);
	ERR_FAIL_COND_V(err, result);

	for (const String &str : gen_files) {
		err = ResourceDowngrader::resource_downgrade(str, format_version);
		ERR_FAIL_COND_V(err, result);
	}

	for (const String& str : gen_files) {
		result.push_back(str);
	}

	return result;
}

TranslationConverter::TranslationConverter() {
}

TranslationConverter::~TranslationConverter() {
}

void TranslationConverter::_bind_methods() {
	ClassDB::bind_static_method("TranslationConverter", D_METHOD("convert_translation_file", "src_path"), &TranslationConverter::convert_translation_file);
}
