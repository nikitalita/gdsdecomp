#pragma once

#include "scene/gui/dialogs.h"
#include "scene/gui/rich_text_label.h"

class RichTextLabel;
class ErrorDialogWithRichText : public AcceptDialog {
	GDCLASS(ErrorDialogWithRichText, AcceptDialog);

	RichTextLabel *rich_text_label = nullptr;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	RichTextLabel *get_rich_text_label();

	void set_rich_text(const String &p_rich_text);
	String get_rich_text() const;

	ErrorDialogWithRichText();

};

class ConfirmationDialogWithRichText : public ErrorDialogWithRichText {
	GDCLASS(ConfirmationDialogWithRichText, ErrorDialogWithRichText);
	Button *cancel = nullptr;

protected:
	static void _bind_methods();

public:
	Button *get_cancel_button();

	void set_cancel_button_text(String p_cancel_button_text);
	String get_cancel_button_text() const;

	ConfirmationDialogWithRichText();
};

