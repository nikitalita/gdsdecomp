class_name GDREPatchPCK
extends GDREChildDialog

const file_icon: Texture2D = preload("res://gdre_icons/gdre_File.svg")
const file_ok: Texture2D = preload("res://gdre_icons/gdre_FileOk.svg")
const file_broken: Texture2D = preload("res://gdre_icons/gdre_FileBroken.svg")
const gdre_export_report = preload("res://gdre_export_report.tscn")
const gdre_file_tree = preload("res://gdre_file_tree.gd")
var arrow_right: Texture2D = get_theme_icon("arrow_collapsed", "Tree")
var select_button: Texture2D = get_theme_icon("select_option", "Tree")
var delete_button: Texture2D = get_theme_icon("indeterminate", "Tree")

var FILE_TREE : GDREFileTree = null
var RECOVER_WINDOW :Window = null
var VERSION_TEXT: Label = null
var SELECTED_PCK: LineEdit = null
var REPORT_DIALOG = null
var SELECT_PCK_DIALOG: FileDialog = null
var FILTER: LineEdit = null
var FILES_TEXT: Label = null
var PATCH_FILE_TREE: Tree = null
var PATCH_FILE_DIALOG: FileDialog = null
var PATCH_FILE_MAPPING_DIALOG: FileDialog = null

var button_clicked_item: TreeItem = null
# var isHiDPI = DisplayServer.screen_get_dpi() >= 240
var root: TreeItem = null
var userroot: TreeItem = null
var num_files:int = 0
var num_broken:int = 0
var num_malformed:int = 0
var _is_test:bool = false

signal patch_pck_done()
# file_map is from -> to, i.e. /path/to/file -> res://file
signal do_patch_pck(dest_pck: String, file_map: Dictionary[String, String]);

enum PatchTreeButton{
	SELECT,
	DELETE
}



func handle_patch_tree_item_edited(item: TreeItem):
	var text = item.get_text(1)
	if not text.is_empty():
		item.set_icon(1, file_ok)
	else:
		item.set_icon(1, file_broken)
	_validate()


func _on_patch_tree_item_edited():
	var item = PATCH_FILE_TREE.get_edited()
	handle_patch_tree_item_edited(item)
	pass

func _on_patch_tree_button_clicked(item: TreeItem, column: int, id: int, mouse_button_index: int):
	if (mouse_button_index != MOUSE_BUTTON_LEFT):
		return
	match id:
		PatchTreeButton.SELECT:
			button_clicked_item = item
			PATCH_FILE_MAPPING_DIALOG.current_file = item.get_text(0).get_file()
			PATCH_FILE_MAPPING_DIALOG.popup_centered()
		PatchTreeButton.DELETE:
			if (button_clicked_item == item):
				button_clicked_item = null
			PATCH_FILE_TREE.get_root().remove_child(item)
			
			pass
	pass

func _on_files_dropped(files: PackedStringArray):
	# get the current global mouse position
	var viewport = get_viewport()
	var mouse_pos = viewport.get_mouse_position()
	# check if it is inside the bounding box of the FileTree or the PatchFileTree
	if FILE_TREE.get_global_rect().has_point(mouse_pos):
		# convert the global mouse position to local position
		if (files.size() > 1):
			popup_error_box("You can only patch one PCK at a time", "Error")
			return
		_on_select_pck_dialog_file_selected(files[0])
	elif PATCH_FILE_TREE.get_global_rect().has_point(mouse_pos):
		_on_select_patch_files_dialog_files_selected(files)

func register_dropped_files():
	pass
	var window = get_viewport()
	var err = window.files_dropped.connect(self._on_files_dropped)
	if err != OK:
		print("Error: failed to connect window to files_dropped signal")
		print("Type: " + self.get_class())
		print("name: " + str(self.get_name()))
# MUST CALL set_root_window() first!!!
# Called when the node enters the scene tree for the first time.
func _ready():
	FILE_TREE =      $Control/FileTree
	VERSION_TEXT =   $Control/VersionText
	SELECTED_PCK = $Control/SelectedPck
	SELECT_PCK_DIALOG = $SelectPckDialog
	FILTER = $Control/Filter
	FILES_TEXT = $Control/FilesText
	PATCH_FILE_TREE = $Control/PatchFileTree
	PATCH_FILE_DIALOG = $SelectPatchFilesDialog
	PATCH_FILE_MAPPING_DIALOG = $SelectPatchMappingDialog
	if isHiDPI:
		# get_viewport().size *= 2.0
		# get_viewport().content_scale_factor = 2.0
		#ThemeDB.fallback_base_scale = 2.0
		self.content_scale_factor = 2.0
		self.size *= 2.0
	# This is a hack to get around not being able to open multiple scenes
	# unless they're attached to windows
	# The children are not already in the window for ease of GUI creation
	clear()
	PATCH_FILE_TREE.connect("item_edited", self._on_patch_tree_item_edited)
	PATCH_FILE_TREE.connect("button_clicked", self._on_patch_tree_button_clicked)
	PATCH_FILE_TREE.set_column_title(0, "File")
	PATCH_FILE_TREE.set_column_title(1, "Mapping")
	register_dropped_files()
	# TODO: remove this
	# var test_bin = "/Users/nikita/Workspace/godot-ws/godot-test-bins/demo_platformer.pck"
	# _on_select_pck_dialog_file_selected(test_bin)


func add_project(paths: PackedStringArray) -> int:
	if (GDRESettings.is_pack_loaded()):
		GDRESettings.unload_project()
		FILE_TREE._clear()
	var err = GDRESettings.load_project(paths, true)
	if (err != OK):
		popup_error_box("Error: failed to open " + str(paths), "Error")
		return err
	var type = GDRESettings.get_pack_type()
	if (type != PackInfo.PCK and type != PackInfo.EXE):
		GDRESettings.unload_project()
		clear_selected_pack()
		popup_error_box("Error: You can only use this to patch PCKs or embedded PCKs", "Error")
		return err
	# need to do this to force it to get a new DirAccess
	PATCH_FILE_MAPPING_DIALOG.set_access(FileDialog.ACCESS_FILESYSTEM)
	PATCH_FILE_MAPPING_DIALOG.current_dir = ""
	PATCH_FILE_MAPPING_DIALOG.set_access(FileDialog.ACCESS_RESOURCES)
	PATCH_FILE_MAPPING_DIALOG.current_dir = "res://"
	VERSION_TEXT.text = GDRESettings.get_version_string()
	var arr: Array = GDRESettings.get_file_info_array()
	FILES_TEXT.text = str(arr.size())
	FILE_TREE.add_files(arr, true)
	return OK

func clear_patch_files():
	PATCH_FILE_TREE.clear()
	# create a dummy root node (this is a flat tree, and scene tree has root nodes as not visible)
	PATCH_FILE_TREE.create_item()

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta):
	pass

func _exit_tree():
	if GDRESettings.is_pack_loaded():
		GDRESettings.unload_project()

func clear_selected_pack():
	SELECTED_PCK.text = ""
	if GDRESettings.is_pack_loaded():
		GDRESettings.unload_project()


func _on_select_pck_button_pressed() -> void:
	SELECT_PCK_DIALOG.popup_centered()

func _on_select_pck_dialog_file_selected(path: String) -> void:
	SELECTED_PCK.text = path
	add_project([path])
	_validate()

func _on_filter_text_changed(new_text: String) -> void:
	FILE_TREE.filter(new_text)
	pass # Replace with function body.

func _on_check_all_pressed() -> void:
	FILE_TREE.check_all_shown(true)


func _on_uncheck_all_pressed() -> void:
	FILE_TREE.check_all_shown(false)

func _on_select_patch_files_dialog_files_selected(paths: PackedStringArray) -> void:
	for path in paths:
		#TreeCellMode
		if (not PATCH_FILE_TREE.get_root()):
			PATCH_FILE_TREE.create_item()
		var item = PATCH_FILE_TREE.create_item()
		item.set_cell_mode(0, TreeItem.CELL_MODE_STRING)
		item.set_cell_mode(1, TreeItem.CELL_MODE_STRING)
		#item.set_icon(0, file_icon)
		#item.set_text_alignment(0, HORIZONTAL_ALIGNMENT_RIGHT)
		#item.set_text_overrun_behavior(0, TextServer.OVERRUN_TRIM_WORD_ELLIPSIS)
		#var drive = path.get_slice("/", 0)
		item.set_text_direction(0, Control.TextDirection.TEXT_DIRECTION_RTL)
		item.set_structured_text_bidi_override(0, TextServer.STRUCTURED_TEXT_FILE)
		item.set_text(0, path)
		item.set_icon(1, file_broken)
		item.set_text_direction(1, Control.TextDirection.TEXT_DIRECTION_RTL)
		item.set_structured_text_bidi_override(1, TextServer.STRUCTURED_TEXT_FILE)
		item.add_button(1, select_button, PatchTreeButton.SELECT)
		item.add_button(1, delete_button, PatchTreeButton.DELETE)
		
		# item.set_custom_as_button(1, true)
		item.set_editable(1, true)

func _on_select_patch_files_pressed() -> void:
	PATCH_FILE_DIALOG.popup_centered()

func _on_select_patch_mapping_dialog_file_selected(path: String) -> void:
	if button_clicked_item:
		button_clicked_item.set_text(1, path)
		handle_patch_tree_item_edited(button_clicked_item)
		button_clicked_item = null


func _on_patch_clear_button_pressed() -> void:
	clear_patch_files()
	_validate()
	
func _validate():
	if (!GDRESettings.is_pack_loaded()):
		self.get_ok_button().disabled = true
		return false
	if (not PATCH_FILE_TREE.get_root() or PATCH_FILE_TREE.get_root().get_children().size() == 0):
		self.get_ok_button().disabled = true
		return false
	var item = PATCH_FILE_TREE.get_root().get_first_child()
	while (item):
		if (item.get_text(1).is_empty()):
			self.get_ok_button().disabled = true
			return false
		item = item.get_next()
	self.get_ok_button().disabled = false
	return true

# override GDREChildDialog

func show_win():
	clear()
	super.show_win()

func clear():
	SELECTED_PCK.clear()
	FILE_TREE._clear()
	clear_patch_files()

func close():
	if GDRESettings.is_pack_loaded():
		GDRESettings.unload_project()
	hide_win()

func cancelled():
	close()

func confirm():
	$SavePckDialog.popup_centered()
	
func _on_save_pck_dialog_file_selected(path: String) -> void:
	$SavePckDialog.hide()
	if (not _validate()):
		print("Validation failed")
		close()
	var file_map: Dictionary[String, String] = {}
	var pck_files = FILE_TREE.get_checked_files()

	var patch_files = PATCH_FILE_TREE.get_root().get_children()
	var reverse_map = {}
	for item in patch_files:
		file_map[item.get_text(0)] = item.get_text(1)
		reverse_map[item.get_text(1)] = true
	for file in pck_files:
		if not (reverse_map.has(file) or reverse_map.has(file.trim_prefix("res://"))):
			file_map[file] = file
	emit_signal("do_patch_pck", path, file_map)
