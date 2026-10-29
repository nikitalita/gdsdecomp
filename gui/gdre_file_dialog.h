#pragma once

#include "scene/gui/file_dialog.h"

class GDREFileDialog : public FileDialog {
	GDCLASS(GDREFileDialog, FileDialog);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	GDREFileDialog();
	~GDREFileDialog();
};
