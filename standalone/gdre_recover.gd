extends Node

const file_icon: Texture2D = preload("res://gdre_icons/gdre_File.svg")
const file_ok: Texture2D = preload("res://gdre_icons/gdre_FileOk.svg")
const file_broken: Texture2D = preload("res://gdre_icons/gdre_FileBroken.svg")
const gdre_export_report = preload("res://gdre_export_report.tscn")

var FILE_TREE : Tree = null
var EXTRACT_ONLY : CheckBox = null
var RECOVER : CheckBox = null
var RECOVER_WINDOW :Window = null
var VERSION_TEXT: Label = null
var INFO_TEXT : Label = null
var POPUP_PARENT_WINDOW : Window = null
var REPORT_DIALOG = null

var isHiDPI = DisplayServer.screen_get_dpi() >= 240
#var isHiDPI = false
var root: TreeItem = null
var userroot: TreeItem = null
var num_files:int = 0
var num_broken:int = 0
var num_malformed:int = 0
var _is_test:bool = true

signal recovery_done()

func popup_error_box(message: String, title: String, parent_window: Window, call_func: Callable = parent_window.show) -> AcceptDialog:
	var dialog = AcceptDialog.new()
	dialog.set_text(message)
	dialog.set_title(title)
	parent_window.add_child(dialog)
	dialog.connect("confirmed", call_func)
	dialog.connect("canceled", call_func)
	dialog.popup_centered()
	return dialog

func _propagate_check(item: TreeItem, checked: bool):
	item.set_checked(0, checked)
	var it: TreeItem = item.get_first_child()
	while (it):
		_propagate_check(it, checked)
		it = it.get_next()

func _on_item_edited():
	var item = FILE_TREE.get_edited()
	var checked = item.is_checked(0)
	_propagate_check(item, checked)

# MUST CALL set_root_window() first!!!
# Called when the node enters the scene tree for the first time.
func _ready():
	FILE_TREE =      $FileTree
	EXTRACT_ONLY =   $RadioButtons/ExtractOnly
	RECOVER =        $RadioButtons/FullRecovery
	VERSION_TEXT =   $VersionText
	INFO_TEXT =      $InfoText
	RECOVER_WINDOW = $RecoverWindow
	if !_is_test:
		assert(POPUP_PARENT_WINDOW)
	else:
		POPUP_PARENT_WINDOW = get_window()

	if isHiDPI:
		# get_viewport().size *= 2.0
		# get_viewport().content_scale_factor = 2.0
		#ThemeDB.fallback_base_scale = 2.0
		RECOVER_WINDOW.content_scale_factor = 2.0
		RECOVER_WINDOW.size *= 2.0
	# This is a hack to get around not being able to open multiple scenes
	# unless they're attached to windows
	# The children are not already in the window for ease of GUI creation
	var children: Array[Node] = self.get_children()
	for child in children:
		# remove the child from root and add it to the window
		# check if it is the recover window
		if child.get_name() == "RecoverWindow":
			continue
		self.remove_child(child)
		RECOVER_WINDOW.add_child(child)
	clear()
	
	var file_list: Tree = FILE_TREE
	file_list.set_column_title(0, "File name")
	file_list.set_column_title(1, "Size")
	file_list.set_column_title(2, "Info")

	file_list.set_column_expand(0, true)
	file_list.set_column_expand(1, false)
	file_list.set_column_expand(2, false)
	file_list.set_column_custom_minimum_width(1, 120)
	file_list.set_column_custom_minimum_width(2, 120)
	file_list.add_theme_constant_override("draw_relationship_lines", 1)
	file_list.connect("item_edited", self._on_item_edited)
	# TODO: remove me
	if _is_test:
		load_test()
	
	pass # Replace with function body.

# called before _ready
func set_root_window(window: Window):
	POPUP_PARENT_WINDOW = window
	_is_test = false

func show_win():
	# get the screen size
	var safe_area: Rect2i = DisplayServer.get_display_safe_area()
	var center = (safe_area.position + safe_area.size - RECOVER_WINDOW.size) / 2
	RECOVER_WINDOW.set_position(center)
	RECOVER_WINDOW.show()

func hide_win():
	RECOVER_WINDOW.hide()

func add_pack(path: String) -> int:
	var err = GDRESettings.load_pack(path)
	if (err != OK):
		popup_error_box("Error: failed to open " + path, "Error", POPUP_PARENT_WINDOW)
		return err
	var pckdump = PckDumper.new()
	var popup = popup_error_box("This will take a while, please wait...", "Info", POPUP_PARENT_WINDOW)
	err = pckdump.check_md5_all_files()
	popup.hide()
	POPUP_PARENT_WINDOW.remove_child(popup)
	VERSION_TEXT.text = GDRESettings.get_version_string()
	var arr: Array = GDRESettings.get_file_info_array()
	for info: PackedFileInfo in arr:
		add_file(info)
	INFO_TEXT.text = "Total files: " + String.num(num_files) + "\n" + "Broken files: " + String.num(num_broken) + "\n" + "Malformed paths: " + String.num(num_malformed)
	return OK

func load_test():
	const path = "/Users/nikita/Workspace/godot-test-bins/satryn.apk"
	add_pack(path)
	show_win()
	
func add_file(info: PackedFileInfo):
	num_files += 1
	var size = info.get_size()
	var path = info.get_path()
	var is_malformed = info.is_malformed()
	var is_verified = info.is_checksum_validated()
	var has_md5 = info.has_md5()
	var icon = file_icon
	var errstr = ""
	if is_malformed:
		icon = file_broken
		errstr = "Malformed path"
		num_malformed += 1
	elif is_verified:
		icon = file_ok
	elif !is_verified && has_md5:
		icon = file_broken
		errstr = "Checksum mismatch"
		num_broken += 1
	if ("user://" in path):
		if userroot == null:
			userroot = FILE_TREE.create_item(root)
			userroot.set_cell_mode(0, TreeItem.CELL_MODE_CHECK)
			userroot.set_checked(0, true)
			userroot.set_editable(0, true)
			userroot.set_icon(0, POPUP_PARENT_WINDOW.get_theme_icon("folder", "FileDialog"))
			userroot.set_text(0, "user://")
			userroot.set_metadata(0, String())
		add_file_to_item(userroot, path, path.replace("user://", ""), size, icon, errstr, false)
	else:
		add_file_to_item(root, path, path.replace("res://", ""), size, icon, errstr, false)

func clear():
	if (userroot != null):
		userroot.clear()
		userroot = null
	FILE_TREE.clear()
	root = FILE_TREE.create_item()
	root.set_cell_mode(0, TreeItem.CELL_MODE_CHECK)
	root.set_checked(0, true)
	root.set_editable(0, true)
	root.set_icon(0, POPUP_PARENT_WINDOW.get_theme_icon("folder", "FileDialog"))
	root.set_text(0, "res://")
	root.set_metadata(0, String())
	
func add_file_to_item(p_item: TreeItem, p_fullname: String, p_name: String, p_size: int, p_icon: Texture2D,  p_error: String, p_enc: bool):
	var pp: int = p_name.find("/")
	if (pp == -1):
		# Add file
		var item: TreeItem = FILE_TREE.create_item(p_item);
		item.set_cell_mode(0, TreeItem.CELL_MODE_CHECK);
		item.set_checked(0, true);
		item.set_editable(0, true);
		item.set_icon(0, p_icon);
		item.set_text(0, p_name);
		item.set_metadata(0, p_fullname);
		if (p_size < (1024)):
			item.set_text(1, String.num_int64(p_size) + " B");
		elif (p_size < (1024 * 1024)):
			item.set_text(1, String.num(float(p_size) / 1024, 2) + " KiB");
		elif (p_size < (1024 * 1024 * 1024)):
			item.set_text(1, String.num(float(p_size) / (1024 * 1024), 2) + " MiB");
		else:
			item.set_text(1, String.num(float(p_size) / (1024 * 1024 * 1024), 2) + " GiB");
		
		item.set_tooltip_text(0, p_error);
		item.set_tooltip_text(1, p_error);
		item.set_text(2, "Encrypted" if p_enc else "");
#
		#_validate_selection();
	else:
		var fld_name: String = p_name.substr(0, pp);
		var path: String = p_name.substr(pp + 1, p_name.length());
		# Add folder if any
		var it: TreeItem = p_item.get_first_child();
		while (it) :
			if (it.get_text(0) == fld_name) :
				add_file_to_item(it, p_fullname, path, p_size, p_icon, p_error, false);
				return;
			it = it.get_next();
		
		var item:TreeItem = FILE_TREE.create_item(p_item);
		item.set_cell_mode(0, TreeItem.CELL_MODE_CHECK);
		item.set_checked(0, true);
		item.set_editable(0, true);
		item.set_icon(0, POPUP_PARENT_WINDOW.get_theme_icon("folder", "FileDialog"));
		item.set_text(0, fld_name);
		item.set_metadata(0, String());
		add_file_to_item(item, p_fullname, path, p_size, p_icon, p_error, false);

func _get_selected_files(p_item: TreeItem):
	var arr: PackedStringArray = []
	var it: TreeItem = p_item
	var p_name: String = it.get_metadata(0)
	if (it.is_checked(0) and !p_name.is_empty()):
		arr.append(p_name)
	it = p_item.get_first_child();
	while (it):
		arr.append_array(_get_selected_files(it))
		it = it.get_next()
	return arr
	
func get_selected_files() -> PackedStringArray:
	var arr: PackedStringArray = _get_selected_files(root)
	if (userroot != null):
		arr.append_array(_get_selected_files(userroot))
	return arr

func extract_and_recover(output_dir: String):
	GDRESettings.open_log_file(output_dir)
	var log_path = GDRESettings.get_log_file_path()
	var report_str = "Log file written to " + log_path
	report_str += "\nPlease include this file when reporting an issue!\n\n"
	var pck_dumper = PckDumper.new()
	var files_to_extract = get_selected_files()
	var err = pck_dumper.pck_dump_to_dir(output_dir, files_to_extract)
	if (err):
		popup_error_box("Error: Could not extract files!!", "Error", POPUP_PARENT_WINDOW)
		return
	# check if ExtractOnly is pressed
	if (EXTRACT_ONLY.is_pressed()):
		report_str += "Total files extracted: " + String.num(files_to_extract.size()) + "\n"
		popup_error_box(report_str, "Info", POPUP_PARENT_WINDOW)
		# GDRESettings.unload_pack()
		return
	# otherwise, continue to recover
	var import_exporter = ImportExporter.new()
	import_exporter.export_imports(output_dir, files_to_extract)
	var report = import_exporter.get_report()
	GDRESettings.close_log_file()
	REPORT_DIALOG = gdre_export_report.instantiate()
	REPORT_DIALOG.set_root_window(POPUP_PARENT_WINDOW)
	POPUP_PARENT_WINDOW.add_child(REPORT_DIALOG)
	POPUP_PARENT_WINDOW.move_child(REPORT_DIALOG, self.get_index() -1)
	REPORT_DIALOG.add_report(report)
	REPORT_DIALOG.connect("report_done", self._report_done)

	REPORT_DIALOG.show_win()
	hide_win()

func _report_done():
	if REPORT_DIALOG:
		REPORT_DIALOG.disconnect("report_done", self._report_done)
		REPORT_DIALOG.hide_win()
		POPUP_PARENT_WINDOW.remove_child(REPORT_DIALOG)
		REPORT_DIALOG = null
	else:
		print("REPORT DONE WITHOUT REPORT_DIALOG?!?!")
	close()

func close():
	_exit_tree()
	hide_win()
	emit_signal("recovery_done")

func cancel_extract():
	close()

func _extract_pressed():
	# pop open a file dialog to pick a directory
	var dialog = FileDialog.new()
	dialog.set_access(FileDialog.ACCESS_FILESYSTEM)
	dialog.file_mode = FileDialog.FILE_MODE_OPEN_DIR
	var pck_path = GDRESettings.get_pack_path().get_base_dir()
	dialog.set_current_dir(pck_path)
	dialog.set_title("Select a directory to extract to")
	RECOVER_WINDOW.add_child(dialog)
	dialog.connect("dir_selected", self.extract_and_recover)
	dialog.connect("canceled", self.cancel_extract)
	dialog.popup_centered()
	

func _close_requested():
	close()

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta):
	pass

func _exit_tree():
	GDRESettings.unload_pack()
