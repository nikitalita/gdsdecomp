#include "gdre_file_dialog.h"

#include "core/object/class_db.h"

#include "gui/gdre_window.h"

void GDREFileDialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		GDREWindow::set_window_autoscaling(this, get_min_size());
	}
}

void GDREConfirmationDialogBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("popup_confirm_box", "p_message", "p_title", "p_confirm_callback", "p_cancel_callback", "p_ok_button_text", "p_cancel_button_text"), &GDREConfirmationDialogBase::popup_confirm_box, DEFVAL(Callable()), DEFVAL(Callable()), DEFVAL("OK"), DEFVAL("Cancel"));
	ClassDB::bind_method(D_METHOD("popup_error_box", "p_message", "p_title", "p_callback"), &GDREConfirmationDialogBase::popup_error_box, DEFVAL("Error"), DEFVAL(Callable()));
}
