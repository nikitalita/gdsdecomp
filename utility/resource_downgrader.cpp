#include "resource_downgrader.h"

#include "utility/gdre_settings.h"

static Error _downgrade(const String src_path, uint32_t current_format_version, const uint32_t target_format_version) {
	while (current_format_version > target_format_version) {
		// 各バージョンの変更履歴は core\io\resource_format_binary.cpp 参照
		switch (current_format_version) {
			case 6: {
				// Version 6: Added PackedVector4Array Variant type.
				// PackedVector4Arrayが使用されていなければバージョン番号を落とすだけで動く
				// PackedVector4Arrayが存在するかのチェックは大変すぎるので省く(もし存在してたら差し替え後に動かした場合対象ファイルの読み込みが失敗するので分かるはず)

				Ref<FileAccess> f = FileAccess::open(src_path, FileAccess::READ_WRITE);

				f->seek(0xC);
				f->store_32(GDRESettings::get_singleton()->get_ver_major());
				f->store_32(GDRESettings::get_singleton()->get_ver_minor());
				f->store_32(5);

				current_format_version = 5;
			} break;
			default:
				return ERR_UNAVAILABLE; // 未対応のバージョン
		}
	}

	return OK;
}

Error ResourceDowngrader::resource_downgrade(const String src_path, const uint32_t target_format_version) {
	uint32_t current_format_version = 0;

	{
		Ref<FileAccess> f = FileAccess::open(src_path, FileAccess::READ_WRITE);
		ERR_FAIL_COND_V_MSG(f.is_null(), ERR_FILE_CANT_OPEN, vformat("Cannot open file '%s'.", src_path));

		uint8_t header[4];
		f->get_buffer(header, 4);
		if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
			return ERR_UNAVAILABLE; // 圧縮フォーマットは未対応

		} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
			return ERR_UNAVAILABLE; // 不明なフォーマット
		}

		f->seek(0x14);
		current_format_version = f->get_32();
	}

	return _downgrade(src_path, current_format_version, target_format_version);
}

ResourceDowngrader::ResourceDowngrader() {
}

ResourceDowngrader::~ResourceDowngrader() {
}
