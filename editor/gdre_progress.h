#ifndef GDRE_PROGRESS_H
#define GDRE_PROGRESS_H
#include "core/string/ustring.h"
#ifdef TOOLS_ENABLED
struct EditorProgress;
class ProgressDialog;
class Node;
#endif
#ifndef TOOLS_ENABLED
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/popup.h"
#include "scene/gui/progress_bar.h"
class ProgressDialog : public PopupPanel {
	GDCLASS(ProgressDialog, PopupPanel);
	struct Task {
		String task;
		VBoxContainer *vb = nullptr;
		ProgressBar *progress = nullptr;
		Label *state = nullptr;
		uint64_t last_progress_tick = 0;
	};
	HBoxContainer *cancel_hb = nullptr;
	Button *cancel = nullptr;

	HashMap<String, Task> tasks;
	VBoxContainer *main = nullptr;
	uint64_t last_progress_tick;

	LocalVector<Window *> host_windows;

	static ProgressDialog *singleton;
	void _popup();

	void _cancel_pressed();

	void _update_ui();
	bool canceled = false;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	static ProgressDialog *get_singleton() { return singleton; }
	void add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel = false);
	bool task_step(const String &p_task, const String &p_state, int p_step = -1, bool p_force_redraw = true);
	void end_task(const String &p_task);

	void add_host_window(Window *p_window);

	ProgressDialog();
};
#endif

struct EditorProgressGDDC {
	String task;
	ProgressDialog *progress_dialog;
#ifdef TOOLS_ENABLED
	EditorProgress *ep;
#endif
	bool step(const String &p_state, int p_step = -1, bool p_force_refresh = true);
	EditorProgressGDDC(Node *p_parent, const String &p_task, const String &p_label, int p_amount, bool p_can_cancel = false);
	~EditorProgressGDDC();
};

#endif // GDRE_PROGRESS_H