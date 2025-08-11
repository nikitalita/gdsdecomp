#pragma once

#include "scene/gui/dialogs.h"
#include "scene/main/window.h"

class GDREWindow : public Window {
	GDCLASS(GDREWindow, Window);

	Vector<Callable> next_process_calls;

protected:
	void _notification(int p_what);

	static void _bind_methods();

public:
	void call_on_next_process(const Callable &p_callable);

	GDREWindow();
	~GDREWindow();
};

class GDREAcceptDialogBase : public AcceptDialog {
	GDCLASS(GDREAcceptDialogBase, AcceptDialog);
	Vector<Callable> next_process_calls;

protected:
	void _notification(int p_what);

	static void _bind_methods();

public:
	void call_on_next_process(const Callable &p_callable);

	GDREAcceptDialogBase();
	~GDREAcceptDialogBase();
};
