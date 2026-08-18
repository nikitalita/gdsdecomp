#include "gdre_window.h"
#include "gui/gdre_progress.h"
#include "scene/main/node.h"
#include "scene/main/window.h"
#include "utility/gdre_settings.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"

void GDREWindow::popup_box(Node *p_parent, Window *p_box, const String &p_message, const String &p_title, const Callable &p_confirm_callback, const Callable &p_cancel_callback, const String &p_ok_button_text, const String &p_cancel_button_text) {
	if (p_parent && p_box->get_parent() != p_parent) {
		if (p_box->get_parent()) {
			p_box->reparent(p_parent);
		} else {
			p_parent->add_child(p_box);
		}
	}
	ConfirmationDialog *confirmation_dialog = Object::cast_to<ConfirmationDialog>(p_box);
	if (confirmation_dialog) {
		confirmation_dialog->set_ok_button_text(p_ok_button_text);
		confirmation_dialog->set_cancel_button_text(p_cancel_button_text);
		confirmation_dialog->set_text(p_message);
		confirmation_dialog->reset_size();
	} else {
		GDREAcceptDialogBase *gdre_accept_dialog = Object::cast_to<GDREAcceptDialogBase>(p_box);
		if (gdre_accept_dialog) {
			gdre_accept_dialog->set_ok_button_text(p_ok_button_text);
			gdre_accept_dialog->set_text(p_message);
			gdre_accept_dialog->reset_size();
		} else {
			AcceptDialog *accept_dialog = Object::cast_to<AcceptDialog>(p_box);
			if (accept_dialog) {
				accept_dialog->set_ok_button_text(p_ok_button_text);
				accept_dialog->set_text(p_message);
				accept_dialog->reset_size();
			} else {
				ERR_FAIL_MSG("Unknown dialog type");
			}
		}
	}
	p_box->set_title(p_title);
	if (!p_confirm_callback.is_null()) {
		p_box->connect("confirmed", p_confirm_callback, CONNECT_ONE_SHOT);
	}
	if (!p_cancel_callback.is_null()) {
		p_box->connect("canceled", p_cancel_callback, CONNECT_ONE_SHOT);
	}
	// p_parent->add_child(p_box);
	p_box->popup_centered();
}

void GDREWindow::set_window_autoscaling(Window *p_window, Size2i p_min_size) {
#ifdef ANDROID_ENABLED
	return;
#endif
	ERR_FAIL_NULL(p_window);
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	float current_scale = p_window->get_content_scale_factor();
	float auto_display_scale = GDRESettings::get_auto_display_scale();
	if (auto_display_scale != 1.0 && auto_display_scale != current_scale) {
		p_window->set_content_scale_factor(auto_display_scale);
	}
	bool no_min = p_min_size == Size2i();
	float size_scale = auto_display_scale / current_scale;
	Size2i new_min_size = Size2i(float(p_min_size.x) * size_scale, float(p_min_size.y) * size_scale);

	if (!no_min && p_window->get_min_size() != new_min_size) {
		p_window->set_min_size(new_min_size);
	}

	Size2i current_size = p_window->get_size();
	Size2i new_size = no_min ? Size2i(float(current_size.x) * size_scale, float(current_size.y) * size_scale) : new_min_size;
	if (no_min || current_size < new_size) {
		p_window->set_size(new_size);
	}
	p_window->set_oversampling_override(auto_display_scale);
}

void GDREWindow::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		GDREWindow::set_window_autoscaling(this, get_min_size());
		GDREWindow::set_window_autoscaling(confirmation_dialog, confirmation_dialog->get_min_size());
		GDREWindow::set_window_autoscaling(error_dialog, error_dialog->get_min_size());
	}
}

GDREWindow::GDREWindow() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->add_host_window(this);
	}
	confirmation_dialog = memnew(ConfirmationDialog);
	error_dialog = memnew(AcceptDialog);
	add_child(confirmation_dialog);
	add_child(error_dialog);
}

GDREWindow::~GDREWindow() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->remove_host_window(this);
	}
}

void GDREWindow::popup_confirm_box(const String &p_message, const String &p_title, const Callable &p_confirm_callback, const Callable &p_cancel_callback, const String &p_ok_button_text, const String &p_cancel_button_text) {
	popup_box(this, confirmation_dialog, p_message, p_title, p_confirm_callback, p_cancel_callback, p_ok_button_text, p_cancel_button_text);
}

void GDREWindow::popup_error_box(const String &p_message, const String &p_title, const Callable &p_callback) {
	popup_box(this, error_dialog, p_message, p_title, p_callback);
}

GDREAcceptDialogBase::GDREAcceptDialogBase() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->add_host_window(this);
	}
	confirmation_dialog = memnew(ConfirmationDialog);
	error_dialog = memnew(AcceptDialog);
	add_child(confirmation_dialog);
	add_child(error_dialog);
}

GDREAcceptDialogBase::~GDREAcceptDialogBase() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->remove_host_window(this);
	}
}

void GDREAcceptDialogBase::popup_confirm_box(const String &p_message, const String &p_title, const Callable &p_confirm_callback, const Callable &p_cancel_callback, const String &p_ok_button_text, const String &p_cancel_button_text) {
	GDREWindow::popup_box(this, confirmation_dialog, p_message, p_title, p_confirm_callback, p_cancel_callback, p_ok_button_text, p_cancel_button_text);
}

void GDREAcceptDialogBase::popup_error_box(const String &p_message, const String &p_title, const Callable &p_callback) {
	GDREWindow::popup_box(this, error_dialog, p_message, p_title, p_callback, p_callback);
}

void GDREAcceptDialogBase::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		GDREWindow::set_window_autoscaling(this, get_min_size());
		GDREWindow::set_window_autoscaling(confirmation_dialog, confirmation_dialog->get_min_size());
		GDREWindow::set_window_autoscaling(error_dialog, error_dialog->get_min_size());
	}
}

GDREConfirmationDialogBase::GDREConfirmationDialogBase() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->add_host_window(this);
	}
	confirmation_dialog = memnew(ConfirmationDialog);
	error_dialog = memnew(AcceptDialog);
	add_child(confirmation_dialog);
	add_child(error_dialog);
}

GDREConfirmationDialogBase::~GDREConfirmationDialogBase() {
	if (GDREProgressDialog::get_singleton()) {
		GDREProgressDialog::get_singleton()->remove_host_window(this);
	}
}

void GDREConfirmationDialogBase::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		GDREWindow::set_window_autoscaling(this, get_min_size());
		GDREWindow::set_window_autoscaling(confirmation_dialog, confirmation_dialog->get_min_size());
		GDREWindow::set_window_autoscaling(error_dialog, error_dialog->get_min_size());
	}
}

void GDREConfirmationDialogBase::popup_confirm_box(const String &p_message, const String &p_title, const Callable &p_confirm_callback, const Callable &p_cancel_callback, const String &p_ok_button_text, const String &p_cancel_button_text) {
	GDREWindow::popup_box(this, confirmation_dialog, p_message, p_title, p_confirm_callback, p_cancel_callback, p_ok_button_text, p_cancel_button_text);
}

void GDREConfirmationDialogBase::popup_error_box(const String &p_message, const String &p_title, const Callable &p_callback) {
	GDREWindow::popup_box(this, error_dialog, p_message, p_title, p_callback);
}

void GDREFileDialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		GDREWindow::set_window_autoscaling(this, get_min_size());
	}
}

void GDREWindow::_bind_methods() {
	ClassDB::bind_static_method(get_class_static(), D_METHOD("popup_box", "p_parent", "p_box", "p_message", "p_title", "p_confirm_callback", "p_cancel_callback", "p_ok_button_text", "p_cancel_button_text"), &GDREWindow::popup_box, DEFVAL(Callable()), DEFVAL(Callable()), DEFVAL("OK"), DEFVAL("Cancel"));
	ClassDB::bind_method(D_METHOD("popup_confirm_box", "p_message", "p_title", "p_confirm_callback", "p_cancel_callback", "p_ok_button_text", "p_cancel_button_text"), &GDREWindow::popup_confirm_box, DEFVAL(Callable()), DEFVAL(Callable()), DEFVAL("OK"), DEFVAL("Cancel"));
	ClassDB::bind_method(D_METHOD("popup_error_box", "p_message", "p_title", "p_callback"), &GDREWindow::popup_error_box, DEFVAL("Error"), DEFVAL(Callable()));
	ClassDB::bind_static_method(get_class_static(), D_METHOD("set_window_autoscaling", "p_window", "p_min_size"), &GDREWindow::set_window_autoscaling);
}

void GDREAcceptDialogBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("popup_confirm_box", "p_message", "p_title", "p_confirm_callback", "p_cancel_callback", "p_ok_button_text", "p_cancel_button_text"), &GDREAcceptDialogBase::popup_confirm_box, DEFVAL(Callable()), DEFVAL(Callable()), DEFVAL("OK"), DEFVAL("Cancel"));
	ClassDB::bind_method(D_METHOD("popup_error_box", "p_message", "p_title", "p_callback"), &GDREAcceptDialogBase::popup_error_box, DEFVAL("Error"), DEFVAL(Callable()));
}

void GDREConfirmationDialogBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("popup_confirm_box", "p_message", "p_title", "p_confirm_callback", "p_cancel_callback", "p_ok_button_text", "p_cancel_button_text"), &GDREConfirmationDialogBase::popup_confirm_box, DEFVAL(Callable()), DEFVAL(Callable()), DEFVAL("OK"), DEFVAL("Cancel"));
	ClassDB::bind_method(D_METHOD("popup_error_box", "p_message", "p_title", "p_callback"), &GDREConfirmationDialogBase::popup_error_box, DEFVAL("Error"), DEFVAL(Callable()));
}
