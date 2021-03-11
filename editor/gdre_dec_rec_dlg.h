/*************************************************************************/
/*  gdre_dec_rec_dlg.h                                                   */
/*************************************************************************/

#ifndef GODOT_RE_DEC_REC_DLG_H
#define GODOT_RE_DEC_REC_DLG_H

#include "core/map.h"
#include "core/resource.h"

#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/item_list.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/text_edit.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#else
#define EDSCALE 1.0
#endif

class ScriptDecompRecursiveDialog : public AcceptDialog {
	OBJ_TYPE(ScriptDecompRecursiveDialog, AcceptDialog)

	FileDialog *source_folder_selection;
	FileDialog *file_selection;

	OptionButton *scrver;

	LineEdit *script_key;
	Label *script_key_error;

	LineEdit *source_dir;
	Button *select_dir;
	 
	void _validate_input();
	void _script_encryption_key_changed(const String &p_key);
	void _dir_select_pressed();
	void _dir_select_request(const String &p_path);
	void _bytcode_changed(int p_id);

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	Vector<String> get_file_list() const;
	String get_source_dir() const;
	Vector<uint8_t> get_key() const;
	int get_bytecode_version() const;
	Vector<String> gde_files;
	ScriptDecompRecursiveDialog();
	~ScriptDecompRecursiveDialog();
};

#endif
