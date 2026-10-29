#include "gdre_file_dialog.h"

#include "core/config/engine.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"

#include "gui/gdre_window.h"
#include "utility/common.h"
#include "utility/gdre_settings.h"

static constexpr const char *RECENT_SECTION = "recent";
static constexpr const char *RECENT_KEY_DIR = "last_dir";
static constexpr const char *RECENT_KEY_FILES = "last_files";
static constexpr const char *RECENT_CONFIG_DIR = "file_dialog_recents";

HashMap<String, GDREFileDialog::RecentState> GDREFileDialog::recent_cache;

static bool _selections_equal(const Vector<String> &p_a, const Vector<String> &p_b) {
	if (p_a.size() != p_b.size()) {
		return false;
	}
	for (int i = 0; i < p_a.size(); i++) {
		if (p_a[i] != p_b[i]) {
			return false;
		}
	}
	return true;
}

// Moves/inserts p_selection to the front of p_history, removing duplicates and capping the size.
static void _push_selection_front(Vector<Vector<String>> &p_history, const Vector<String> &p_selection, int p_max) {
	if (p_selection.is_empty()) {
		return;
	}
	for (int i = p_history.size() - 1; i >= 0; i--) {
		if (_selections_equal(p_history[i], p_selection)) {
			p_history.remove_at(i);
		}
	}
	p_history.insert(0, p_selection);
	if (p_history.size() > p_max) {
		p_history.resize(p_max);
	}
}

bool GDREFileDialog::_should_persist() const {
	// Don't write config files from the editor or from scenes being edited in the editor.
	return !Engine::get_singleton()->is_editor_hint() && !is_part_of_edited_scene();
}

String GDREFileDialog::_get_config_name() const {
	String name;
	if (!group_recent.is_empty()) {
		name = group_recent.validate_filename();
	}
	if (name.is_empty() && is_inside_tree()) {
		name = String(get_path()).trim_prefix("/").replace("/", "__").validate_filename();
	}
	return name;
}

String GDREFileDialog::get_config_path() const {
	String name = _get_config_name();
	if (name.is_empty()) {
		return String();
	}
	return GDRESettings::get_gdre_user_path().path_join(RECENT_CONFIG_DIR).path_join(name + ".cfg");
}

Error GDREFileDialog::load_recent_state() {
	config_loaded = false;
	const String path = get_config_path();
	ERR_FAIL_COND_V_MSG(path.is_empty(), ERR_UNCONFIGURED, "GDREFileDialog: cannot resolve config path before the dialog is in the tree.");

	if (!_should_persist()) {
		// Keep whatever is in memory, but don't touch the disk.
		config_loaded = true;
		return OK;
	}
	Error err = OK;

	if (const RecentState *cached = recent_cache.getptr(path)) {
		last_dir = cached->last_dir;
		last_opened_files = cached->last_opened_files;
		config_loaded = true;
		return OK;
	}

	RecentState state;
	if (FileAccess::exists(path)) {
		Ref<ConfigFile> cf;
		cf.instantiate();
		err = cf->load(path);
		if (err == OK) {
			state.last_dir = String(cf->get_value(RECENT_SECTION, RECENT_KEY_DIR, String()));
			Variant files_var = cf->get_value(RECENT_SECTION, RECENT_KEY_FILES, Array());
			if (files_var.get_type() == Variant::ARRAY) {
				Array files_arr = files_var;
				for (int i = 0; i < files_arr.size() && state.last_opened_files.size() < MAX_RECENT_SELECTIONS; i++) {
					if (files_arr[i].get_type() != Variant::PACKED_STRING_ARRAY) {
						continue;
					}
					Vector<String> selection = files_arr[i];
					if (!selection.is_empty()) {
						state.last_opened_files.push_back(selection);
					}
				}
			}
		}
	}
	last_dir = state.last_dir;
	last_opened_files = state.last_opened_files;
	recent_cache[path] = state;
	config_loaded = true;
	return err;
}

Error GDREFileDialog::save_recent_state() {
	if (!config_loaded || !_should_persist()) {
		return OK;
	}
	const String path = get_config_path();
	ERR_FAIL_COND_V(path.is_empty(), ERR_UNCONFIGURED);

	RecentState &state = recent_cache[path];
	state.last_dir = last_dir;
	state.last_opened_files = last_opened_files;

	Error err = gdre::ensure_dir(path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "GDREFileDialog: failed to create directory " + path.get_base_dir());

	Array files_arr;
	for (const Vector<String> &selection : last_opened_files) {
		files_arr.push_back(selection);
	}
	Ref<ConfigFile> cf;
	cf.instantiate();
	cf->set_value(RECENT_SECTION, RECENT_KEY_DIR, last_dir);
	cf->set_value(RECENT_SECTION, RECENT_KEY_FILES, files_arr);
	err = cf->save(path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "GDREFileDialog: failed to save " + path);
	return OK;
}

void GDREFileDialog::_apply_last_dir() {
	if (last_dir.is_empty() || last_dir == get_current_dir()) {
		return;
	}
	if (!DirAccess::dir_exists_absolute(last_dir)) {
		return;
	}
	set_current_dir(last_dir);
}

void GDREFileDialog::_refresh_from_cache() {
	if (!config_loaded) {
		return;
	}
	// Another dialog in the same group may have changed the state since we last looked.
	if (const RecentState *cached = recent_cache.getptr(get_config_path())) {
		last_dir = cached->last_dir;
		last_opened_files = cached->last_opened_files;
	}
	_apply_last_dir();
}

void GDREFileDialog::_record_selection(const Vector<String> &p_files, const String &p_dir) {
	if (p_files.is_empty()) {
		return;
	}
	if (!p_dir.is_empty()) {
		last_dir = p_dir;
	}
	_push_selection_front(last_opened_files, p_files, MAX_RECENT_SELECTIONS);
	save_recent_state();
}

void GDREFileDialog::_on_file_selected(const String &p_path) {
	if (p_path.is_empty()) {
		return;
	}
	Vector<String> files;
	files.push_back(p_path);
	_record_selection(files, p_path.get_base_dir());
}

void GDREFileDialog::_on_files_selected(const PackedStringArray &p_paths) {
	if (p_paths.is_empty()) {
		return;
	}
	_record_selection(p_paths, p_paths[0].get_base_dir());
}

void GDREFileDialog::_on_dir_selected(const String &p_dir) {
	if (p_dir.is_empty()) {
		return;
	}
	Vector<String> files;
	files.push_back(p_dir);
	_record_selection(files, p_dir);
}

void GDREFileDialog::set_group_recent(const String &p_group) {
	if (group_recent == p_group) {
		return;
	}
	group_recent = p_group;
	if (config_loaded) {
		// Already ready; switch to the new group's state.
		load_recent_state();
		_apply_last_dir();
	}
}

String GDREFileDialog::get_group_recent() const {
	return group_recent;
}

Vector<Vector<String>> GDREFileDialog::get_last_opened_files() const {
	return last_opened_files;
}

void GDREFileDialog::set_last_opened_files(const Vector<Vector<String>> &p_files) {
	last_opened_files.clear();
	// Preserve order (most recent first), dropping empties and duplicates.
	for (const Vector<String> &selection : p_files) {
		if (selection.is_empty() || last_opened_files.size() >= MAX_RECENT_SELECTIONS) {
			continue;
		}
		bool duplicate = false;
		for (const Vector<String> &existing : last_opened_files) {
			if (_selections_equal(existing, selection)) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate) {
			last_opened_files.push_back(selection);
		}
	}
	save_recent_state();
}

Vector<String> GDREFileDialog::get_last_selection() const {
	return last_opened_files.is_empty() ? Vector<String>() : last_opened_files[0];
}

void GDREFileDialog::add_last_opened_selection(const Vector<String> &p_files) {
	if (p_files.is_empty()) {
		return;
	}
	_push_selection_front(last_opened_files, p_files, MAX_RECENT_SELECTIONS);
	save_recent_state();
}

String GDREFileDialog::get_last_opened_dir() const {
	return last_dir;
}

void GDREFileDialog::set_last_opened_dir(const String &p_dir) {
	last_dir = p_dir;
	save_recent_state();
}

TypedArray<PackedStringArray> GDREFileDialog::_get_last_opened_files_bind() const {
	TypedArray<PackedStringArray> ret;
	for (const Vector<String> &selection : last_opened_files) {
		ret.push_back(selection);
	}
	return ret;
}

void GDREFileDialog::_set_last_opened_files_bind(const TypedArray<PackedStringArray> &p_files) {
	Vector<Vector<String>> files;
	for (int i = 0; i < p_files.size(); i++) {
		PackedStringArray selection = p_files[i];
		files.push_back(selection);
	}
	set_last_opened_files(files);
}

PackedStringArray GDREFileDialog::_get_last_selection_bind() const {
	return get_last_selection();
}

void GDREFileDialog::_add_last_opened_selection_bind(const PackedStringArray &p_files) {
	add_last_opened_selection(p_files);
}

void GDREFileDialog::_popup_base(const Rect2i &p_screen_rect) {
	_refresh_from_cache();
	FileDialog::_popup_base(p_screen_rect);
}

void GDREFileDialog::set_visible(bool p_visible) {
	if (p_visible) {
		_refresh_from_cache();
	}
	FileDialog::set_visible(p_visible);
}

void GDREFileDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			GDREWindow::set_window_autoscaling(this, get_min_size());
		} break;
		case NOTIFICATION_READY: {
			// The node path (and group_recent, set by the scene) is only reliable now, so load the config here.
			load_recent_state();
			_apply_last_dir();
		} break;
	}
}

void GDREFileDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_group_recent", "group"), &GDREFileDialog::set_group_recent);
	ClassDB::bind_method(D_METHOD("get_group_recent"), &GDREFileDialog::get_group_recent);
	ClassDB::bind_method(D_METHOD("get_config_path"), &GDREFileDialog::get_config_path);
	ClassDB::bind_method(D_METHOD("load_recent_state"), &GDREFileDialog::load_recent_state);
	ClassDB::bind_method(D_METHOD("save_recent_state"), &GDREFileDialog::save_recent_state);

	ClassDB::bind_method(D_METHOD("get_last_opened_files"), &GDREFileDialog::_get_last_opened_files_bind);
	ClassDB::bind_method(D_METHOD("set_last_opened_files", "files"), &GDREFileDialog::_set_last_opened_files_bind);
	ClassDB::bind_method(D_METHOD("get_last_selection"), &GDREFileDialog::_get_last_selection_bind);
	ClassDB::bind_method(D_METHOD("add_last_opened_selection", "files"), &GDREFileDialog::_add_last_opened_selection_bind);
	ClassDB::bind_method(D_METHOD("get_last_opened_dir"), &GDREFileDialog::get_last_opened_dir);
	ClassDB::bind_method(D_METHOD("set_last_opened_dir", "dir"), &GDREFileDialog::set_last_opened_dir);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "group_recent"), "set_group_recent", "get_group_recent");
}

GDREFileDialog::GDREFileDialog() {
	connect("file_selected", callable_mp(this, &GDREFileDialog::_on_file_selected));
	connect("files_selected", callable_mp(this, &GDREFileDialog::_on_files_selected));
	connect("dir_selected", callable_mp(this, &GDREFileDialog::_on_dir_selected));
}

GDREFileDialog::~GDREFileDialog() {
}
