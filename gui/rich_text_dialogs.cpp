#include "gui/rich_text_dialogs.h"

#include "scene/gui/rich_text_label.h"
#include "core/object/object.h"
#include "core/object/class_db.h"

#include "gui/gdre_window.h"

ErrorDialogWithRichText::ErrorDialogWithRichText() {
	rich_text_label = memnew(RichTextLabel);
	rich_text_label->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	rich_text_label->set_anchor(SIDE_RIGHT, Control::ANCHOR_END);
	rich_text_label->set_anchor(SIDE_BOTTOM, Control::ANCHOR_END);
	rich_text_label->set_grow_direction_preset(Control::LayoutPreset::PRESET_FULL_RECT);
	rich_text_label->set_use_bbcode(true);
	rich_text_label->set_fit_content(true);
	rich_text_label->set_autowrap_mode(TextServer::AutowrapMode::AUTOWRAP_OFF);
	rich_text_label->add_theme_style_override("normal", memnew(StyleBoxEmpty));
	// add it right after the parent's message_label
	add_child(rich_text_label, false, INTERNAL_MODE_FRONT);
}
void ErrorDialogWithRichText::set_rich_text(const String &p_rich_text) {
	if (!get_text().is_empty()) {
		set_text("");
	}
	rich_text_label->set_text(p_rich_text);
}

String ErrorDialogWithRichText::get_rich_text() const {
	String text = rich_text_label->get_text();
	if (text.is_empty()) {
		text = get_text();
	}
	return text;
}

RichTextLabel *ErrorDialogWithRichText::get_rich_text_label() {
	return rich_text_label;
}

void ErrorDialogWithRichText::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		GDREWindow::set_window_autoscaling(this, get_min_size());
	}
}

void ErrorDialogWithRichText::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_rich_text_label"), &ErrorDialogWithRichText::get_rich_text_label);
	ClassDB::bind_method(D_METHOD("set_rich_text", "p_rich_text"), &ErrorDialogWithRichText::set_rich_text);
	ClassDB::bind_method(D_METHOD("get_rich_text"), &ErrorDialogWithRichText::get_rich_text);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "rich_text"), "set_rich_text", "get_rich_text");
	ADD_CLASS_DEPENDENCY("RichTextLabel");
}

void ConfirmationDialogWithRichText::set_cancel_button_text(String p_cancel_button_text) {
	cancel->set_text(p_cancel_button_text);
}

String ConfirmationDialogWithRichText::get_cancel_button_text() const {
	return cancel->get_text();
}

void ConfirmationDialogWithRichText::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_cancel_button"), &ConfirmationDialogWithRichText::get_cancel_button);
	ClassDB::bind_method(D_METHOD("set_cancel_button_text", "text"), &ConfirmationDialogWithRichText::set_cancel_button_text);
	ClassDB::bind_method(D_METHOD("get_cancel_button_text"), &ConfirmationDialogWithRichText::get_cancel_button_text);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "cancel_button_text"), "set_cancel_button_text", "get_cancel_button_text");

	ADD_CLASS_DEPENDENCY("Button");
}

Button *ConfirmationDialogWithRichText::get_cancel_button() {
	return cancel;
}

ConfirmationDialogWithRichText::ConfirmationDialogWithRichText() {
	set_default_title(ETR("Please Confirm..."));
	set_min_size(Size2(200, 70));

	cancel = add_cancel_button();
}
