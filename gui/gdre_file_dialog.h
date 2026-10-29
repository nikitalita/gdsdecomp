#pragma once

#include "core/templates/hash_map.h"
#include "core/variant/typed_array.h"
#include "scene/gui/file_dialog.h"

// A FileDialog that remembers the last directory and the last selections made with it.
//
// State is persisted to a ConfigFile in the GDRE user path. If `group_recent` is set, every dialog with the
// same group shares one config file; otherwise the config file is derived from the dialog's node path.
// Because the node path is only valid inside the tree, the config is loaded on NOTIFICATION_READY.
class GDREFileDialog : public FileDialog {
	GDCLASS(GDREFileDialog, FileDialog);

public:
	static constexpr int MAX_RECENT_SELECTIONS = 20;

private:
	struct RecentState {
		String last_dir;
		// Most recent selection first. Each inner vector is one selection (one file, several files, or one dir).
		Vector<Vector<String>> last_opened_files;
	};

	// Shared between all dialogs, keyed by config path.
	static HashMap<String, RecentState> recent_cache;

	String group_recent;
	String last_dir;
	Vector<Vector<String>> last_opened_files;
	bool config_loaded = false;

	bool _should_persist() const;
	String _get_config_name() const;
	void _apply_last_dir();
	void _refresh_from_cache();
	void _record_selection(const Vector<String> &p_files, const String &p_dir);

	void _on_file_selected(const String &p_path);
	void _on_files_selected(const PackedStringArray &p_paths);
	void _on_dir_selected(const String &p_dir);

	// Script-facing wrappers (Vector<Vector<String>> can't be a Variant).
	TypedArray<PackedStringArray> _get_last_opened_files_bind() const;
	void _set_last_opened_files_bind(const TypedArray<PackedStringArray> &p_files);
	PackedStringArray _get_last_selection_bind() const;
	void _add_last_opened_selection_bind(const PackedStringArray &p_files);

protected:
	void _notification(int p_what);
	static void _bind_methods();

	virtual void _popup_base(const Rect2i &p_screen_rect = Rect2i()) override;

public:
	virtual void set_visible(bool p_visible) override;

	void set_group_recent(const String &p_group);
	String get_group_recent() const;

	// Resolved path of the config file for this dialog (empty until inside the tree, unless group_recent is set).
	String get_config_path() const;
	Error load_recent_state();
	Error save_recent_state();

	Vector<Vector<String>> get_last_opened_files() const;
	void set_last_opened_files(const Vector<Vector<String>> &p_files);
	// Most recent selection; empty if there is none.
	Vector<String> get_last_selection() const;
	// Adds a selection to the front of the history (moving it if it already exists).
	void add_last_opened_selection(const Vector<String> &p_files);

	String get_last_opened_dir() const;
	void set_last_opened_dir(const String &p_dir);

	GDREFileDialog();
	~GDREFileDialog();
};
