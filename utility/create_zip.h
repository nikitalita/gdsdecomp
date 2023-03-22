
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/zip_io.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "platform/web/api/javascript_bridge_singleton.h"
#include "thirdparty/minizip/zip.h"

#include <emscripten/emscripten.h>

bool is_executable(String path) {
	if (path.get_extension() == "so") {
		return true;
	}
	return false;
}

void _zip_file(String p_path, String p_base_path, zipFile p_zip) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		WARN_PRINT("Unable to open file for zipping: " + p_path);
		return;
	}
	Vector<uint8_t> data;
	uint64_t len = f->get_length();
	data.resize(len);
	f->get_buffer(data.ptrw(), len);

	String path = p_path.replace_first(p_base_path, "");
	zipOpenNewFileInZip(p_zip,
			path.utf8().get_data(),
			nullptr,
			nullptr,
			0,
			nullptr,
			0,
			nullptr,
			Z_DEFLATED,
			Z_DEFAULT_COMPRESSION);
	zipWriteInFileInZip(p_zip, data.ptr(), data.size());
	zipCloseFileInZip(p_zip);
}

void zip_folder_recursive(String p_path, String p_base_path, zipFile p_zip) {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		WARN_PRINT("Unable to open directory for zipping: " + p_path);
		return;
	}
	dir->list_dir_begin();
	String cur = dir->get_next();
	while (!cur.is_empty()) {
		String cs = p_path.path_join(cur);
		if (cur == "." || cur == "..") {
			// Skip
		} else if (dir->current_is_dir()) {
			String path = cs.replace_first(p_base_path, "") + "/";
			zipOpenNewFileInZip(p_zip,
					path.utf8().get_data(),
					nullptr,
					nullptr,
					0,
					nullptr,
					0,
					nullptr,
					Z_DEFLATED,
					Z_DEFAULT_COMPRESSION);
			zipCloseFileInZip(p_zip);
			zip_folder_recursive(cs, p_base_path, p_zip);
		} else {
			_zip_file(cs, p_base_path, p_zip);
		}
		cur = dir->get_next();
	}
}

void create_zip_from_folder(String zip_path, String folder_path) {
	if (FileAccess::exists(zip_path)) {
		Ref<DirAccess> da = DirAccess::open(zip_path.get_base_dir());
		ERR_FAIL_COND_MSG(!da.is_valid(), "Failed to open dir " + zip_path.get_base_dir());
		da->remove(zip_path);
	}

	Ref<FileAccess> io_fa_dst;
	zlib_filefunc_def io_dst = zipio_create_io(&io_fa_dst);
	WARN_PRINT("ZIP_PATH IS " + zip_path);
	zipFile zip = zipOpen2(zip_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io_dst);
	const String base_path = folder_path.substr(0, folder_path.rfind("/")) + "/";

	zip_folder_recursive(folder_path, base_path, zip);

	zipClose(zip, nullptr);

	// if (tmp_app_dir->change_dir(tmp_dir_path) == OK) {
	// 	tmp_app_dir->erase_contents_recursive();
	// 	tmp_app_dir->change_dir("..");
	// 	tmp_app_dir->remove(pkg_name);
	// }
}

void download_zip(String projectName, String folder_path) {
#ifdef WEB_ENABLED
	String zip_path = "/userfs/" + projectName + ".zip";
	create_zip_from_folder(zip_path, folder_path);

	Ref<FileAccess> f = FileAccess::open(zip_path, FileAccess::READ);
	ERR_FAIL_COND_MSG(f.is_null(), "Unable to create ZIP file.");
	Vector<uint8_t> buf;
	buf.resize(f->get_length());
	f->get_buffer(buf.ptrw(), buf.size());

	JavaScriptBridge::get_singleton()->download_buffer(buf, zip_path.get_file(), "application/zip");
	f->close();
	f = Ref<FileAccess>();
	Ref<DirAccess> da = DirAccess::open(zip_path.get_base_dir());
	ERR_FAIL_COND_MSG(!da.is_valid(), "Failed to open dir " + zip_path.get_base_dir());
	da->remove(zip_path);
#endif
}