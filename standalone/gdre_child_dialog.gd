class_name GDREChildDialog
extends GDREAcceptDialogBase

var ERROR_DIALOG: AcceptDialog = null
var CONFIRM_DIALOG: ConfirmationDialog = null
var POPUP_PARENT_WINDOW: Window = null

var isHiDPI = false
static var void_func: Callable = func(): return

static func popup_box(parent_window: Node, dialog: AcceptDialog, message: String, box_title: String, confirm_func: Callable = void_func, cancel_func: Callable = void_func):
	if (dialog == null):
		dialog = AcceptDialog.new()
	if (dialog.get_parent() != parent_window):
		if (dialog.get_parent() == null):
			parent_window.add_child(dialog)
		else:
			dialog.reparent(parent_window)
	dialog.reset_size()
	dialog.set_text(message)
	dialog.set_title(box_title)
	var _confirm_func: Callable
	var _cancel_func: Callable
	var arr = dialog.get_signal_connection_list("confirmed")
	for dict in arr:
		dialog.disconnect("confirmed", dict.callable)
	arr = dialog.get_signal_connection_list("canceled")
	for dict in arr:
		dialog.disconnect("canceled", dict.callable)
	dialog.connect("confirmed", confirm_func)
	dialog.connect("canceled", cancel_func)
	dialog.popup_centered()

func popup_error_box(message: String, box_title: String, call_func: Callable = void_func):
	if not ERROR_DIALOG:
		_init_error_dialog()
	return popup_box(self, ERROR_DIALOG, message, box_title, call_func, call_func)
	# return dialog

func popup_confirm_box(message: String, box_title: String, confirm_func: Callable  = void_func, cancel_func: Callable  = void_func):
	if not CONFIRM_DIALOG:
		_init_confirm_dialog()
	return popup_box(self, CONFIRM_DIALOG, message, box_title, confirm_func, cancel_func)
	# return dialog


# MUST CALL set_root_window() first!!!
# Called when the node enters the scene tree for the first time.
# called before _ready
func set_root_window(window: Window):
	POPUP_PARENT_WINDOW = window

func show_win():
	# get the screen size
	var safe_area: Rect2i = DisplayServer.get_display_safe_area()
	var center = (safe_area.position + safe_area.size - self.size) / 2
	self.set_position(center)
	self.show()

func hide_win():
	self.hide()



func open_subwindow(window: Window):
	window.set_transient(true)
	window.set_exclusive(true)
	window.popup_centered()
	# window.set_unparent_when_invisible(true)

func close_subwindow(window: Window):
	window.hide()
	window.set_exclusive(false)
	window.set_transient(false)

func clear():
	assert(false, "clear() not implemented")

func close():
	assert(false, "close() not implemented")

func confirm():
	assert(false, "closed() not implemented")

func cancelled():
	assert(false, "cancelled() not implemented")


func _on_cancelled():
	cancelled()

func _on_confirmed():
	confirm()

func _on_close_requested():
	close()

func _init_error_dialog():
	if (ERROR_DIALOG == null):
		ERROR_DIALOG = AcceptDialog.new()
	if (not ERROR_DIALOG.is_inside_tree()):
		self.add_child(ERROR_DIALOG)
	pass
func _init_confirm_dialog():
	if (CONFIRM_DIALOG == null):
		CONFIRM_DIALOG = ConfirmationDialog.new()
	if (not CONFIRM_DIALOG.is_inside_tree()):
		self.add_child(CONFIRM_DIALOG)
	pass

func _parent_ready():
	_init_error_dialog()
	_init_confirm_dialog()
	self.connect("close_requested", self._on_close_requested)
	self.connect("confirmed", self._on_confirmed)
	self.connect("canceled", self._on_cancelled)

func _init() -> void:
	self.connect("ready", self._parent_ready)
