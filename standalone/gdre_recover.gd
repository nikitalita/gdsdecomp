class_name GDRERecoverDialog
extends Window


var FILE_TREE : GDREFileTree = null
var EXTRACT_ONLY : CheckBox = null
var RECOVER : CheckBox = null
var RECOVER_WINDOW :Window = null
var VERSION_TEXT: Label = null
var INFO_TEXT : Label = null
var DIRECTORY: LineEdit = null
var RESOURCE_PREVIEW: Control = null
var HSPLIT_CONTAINER: HSplitContainer = null
var SHOW_PREVIEW_BUTTON: Button = null
var DESKTOP_DIR = OS.get_system_dir(OS.SystemDir.SYSTEM_DIR_DESKTOP)
var ERROR_DIALOG: AcceptDialog = null

var isHiDPI = false #DisplayServer.screen_get_dpi() >= 240
var root: TreeItem = null
var userroot: TreeItem = null
var num_files:int = 0
var num_broken:int = 0
var num_malformed:int = 0
var _file_dialog: FileDialog = null

signal recovery_done()
signal recovery_confirmed(files_to_extract: PackedStringArray, output_dir: String, extract_only: bool)

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

func show_win():
	# get the screen size
	set_process_input(true)
	var safe_area: Rect2i = DisplayServer.get_display_safe_area()
	var new_size = Vector2(safe_area.size.x - 100, safe_area.size.y - 100)
	self.size = new_size
	var center = (safe_area.position + self.size / 2)
	self.set_position(center)
	SHOW_PREVIEW_BUTTON.set_pressed_no_signal(true)
	SHOW_PREVIEW_BUTTON.toggled.emit(true)
	self.popup_centered()

static var void_func: Callable = func(): return
func _init_error_dialog():
	if (ERROR_DIALOG == null):
		ERROR_DIALOG = AcceptDialog.new()
	if (not ERROR_DIALOG.is_inside_tree()):
		self.add_child(ERROR_DIALOG)
	pass

func popup_error_box(message: String, box_title: String, call_func: Callable = void_func):
	if not ERROR_DIALOG:
		_init_error_dialog()
	return GDREChildDialog.popup_box(self, ERROR_DIALOG, message, box_title, call_func, call_func)
	# return dialog


func extract_file(file: String, output_dir: String, dir_structure: DirStructure, rel_base: String) -> String:
	var bytes = FileAccess.get_file_as_bytes(file)
	if bytes.is_empty():
		return "Failed to read file: " + file
	else:
		var file_path = get_output_file_name(file, output_dir, dir_structure, "", rel_base)
		GDRECommon.ensure_dir(file_path.get_base_dir())
		var f = FileAccess.open(file_path, FileAccess.WRITE)
		if f:
			f.store_buffer(bytes)
			f.close()
		else:
			return "Failed to open file for writing: " + file_path
	return ""


var prev_items: Array = []
var REL_BASE_DIR = "res://"

func _get_all_files(files: PackedStringArray) -> PackedStringArray:
	var new_files: Dictionary = {}
	for file in files:
		if DirAccess.dir_exists_absolute(file):
			var new_arr = GDRECommon.get_recursive_dir_list(file, [], true)
			for new_file in new_arr:
				new_files[new_file] = true
		else:
			new_files[file] = true
	return PackedStringArray(new_files.keys())

const DIR_STRUCTURE_OPTION_NAME = "Directory Structure"

enum DirStructure {
	FLAT,
	RELATIVE_HIERARCHICAL,
	ABSOLUTE_HIERARCHICAL,
}

func get_output_file_name(src: String, output_folder: String, dir_structure_option: DirStructure, new_ext: String = "", rel_base: String = "") -> String:
	var new_name = ""
	if dir_structure_option == DirStructure.FLAT:
		new_name = output_folder.path_join(src.get_file())
	elif dir_structure_option == DirStructure.ABSOLUTE_HIERARCHICAL:
		new_name = output_folder.path_join(src.trim_prefix("res://").replace("user://", ".user/"))
	elif dir_structure_option == DirStructure.RELATIVE_HIERARCHICAL:
		new_name = output_folder.path_join(src.trim_prefix(rel_base))
	if !new_ext.is_empty():
		new_name = new_name.get_basename() + "." + new_ext
	return new_name

func _export_files(files: PackedStringArray, output_dir: String, dir_structure: DirStructure, rel_base: String) -> PackedStringArray:
	var errs: PackedStringArray = []
	files = _get_all_files(files)

	for file in files:
		if DirAccess.dir_exists_absolute(file):
			continue

		if file.get_file() == "project.binary" || file.get_file() == "engine.cfb":
			var ret = GDREGlobals.convert_pcfg_to_text(file, output_dir)
			if ret[0] != OK:
				errs.append("Failed to convert project config: " + file + "\n" + "\n".join(GDRESettings.get_errors()))
			continue
		var _ret = GDRESettings.get_import_info_by_dest(file)
		if _ret:
			var iinfo: ImportInfo = ImportInfo.copy(_ret)
			var report: ExportReport = null
			# if file.get_extension().to_lower() == "scn" and iinfo.get_importer() == "autoconverted":
			# 	var exporter = Exporter.get_exporter_from_path(file)
			# 	if exporter:
			# 		report = exporter.export_resource(output_dir, iinfo)
			# 	else:
			# 		errs.append("Failed to export file: " + file + "\n" + "\n".join(GDRESettings.get_errors()))
			# else:
			iinfo.export_dest = get_output_file_name(iinfo.source_file, "res://", dir_structure, iinfo.source_file.get_extension(), rel_base)
			report = Exporter.export_resource(output_dir, iinfo)
			if not report:
				errs.append("Failed to export resource: " + file)
			elif report.error != OK:
				errs.append("Failed to export resource: " + file + "\n" + "\n".join(report.get_error_messages()))
			else:
				var actual_output_path = report.saved_path
				var rel_path = actual_output_path.trim_prefix(output_dir)
				if rel_path.begins_with(".assets"):
					var new_path = output_dir.path_join(rel_path.trim_prefix(".assets"))
					GDRECommon.ensure_dir(new_path.get_base_dir())
					DirAccess.rename_absolute(actual_output_path, new_path)

		elif Exporter.is_exportable_resource(file):
			var ext = Exporter.get_default_export_extension(file)
			var dest_file = get_output_file_name(file, output_dir, dir_structure, ext, rel_base)
			var err = Exporter.export_file(dest_file, file)
			if err:
				errs.append("Failed to export file: " + file + "\n" + "\n".join(GDRESettings.get_errors()))
		elif ResourceFormatLoaderCompatBinary.is_binary_resource(file):
			var new_ext = "tres"
			if file.get_extension().to_lower() == "scn":
				new_ext = "tscn"
			var err = ResourceCompatLoader.to_text(file, get_output_file_name(file, output_dir, dir_structure, new_ext, rel_base))
			if err:
				errs.append("Failed to export file: " + file + "\n" + "\n".join(GDRESettings.get_errors()))
		else:
			# extract the file
			var err = extract_file(file, output_dir, dir_structure, rel_base)
			if not err.is_empty():
				errs.append(err)
	return errs

func _on_export_resources_confirmed(output_dir: String):
	var files: PackedStringArray = []
	var errs: PackedStringArray = []
	for item: TreeItem in prev_items:
		files.append(FILE_TREE._get_path(item))
	prev_items = []
	var rel_base = REL_BASE_DIR
	REL_BASE_DIR = "res://"
	var options = %ExportResDirDialog.get_selected_options()
	var dir_structure = DirStructure.FLAT
	if options.size() > 0:
		dir_structure = options[DIR_STRUCTURE_OPTION_NAME]

	errs = _export_files(files, output_dir, dir_structure, rel_base)

	if errs.size() > 0:
		popup_error_box("\n".join(errs), "Error")
	else:
		popup_error_box("Successfully exported resources", "Success")




func _on_extract_resources_dir_selected(path: String):
	var options = %ExtractResDirDialog.get_selected_options()
	print("OPTIONS: ", options)
	var dir_structure = DirStructure.FLAT
	if options.size() > 0:
		dir_structure = options[DIR_STRUCTURE_OPTION_NAME]
	var files: PackedStringArray = []
	var errs: PackedStringArray = []
	for item in prev_items:
		files.append(FILE_TREE._get_path(item))
	print("FILES: ", files)
	prev_items = []
	var rel_base = REL_BASE_DIR
	REL_BASE_DIR = "res://"
	files = _get_all_files(files)
	for file in files:
		if DirAccess.dir_exists_absolute(file):
			continue
		var err = extract_file(file, path, dir_structure, rel_base)
		if not err.is_empty():
			errs.append(err)
	if errs.size() > 0:
		popup_error_box("\n".join(errs), "Error")
	else:
		popup_error_box("Successfully extracted resources", "Success")

func _determine_rel_base_dir(selected_items: Array) -> String:
	var base_dirs: Dictionary = {}
	if selected_items.size() == 0:
		return "res://"
	for item in selected_items:
		var path = FILE_TREE._get_path(item)
		var base_dir = path.get_base_dir()
		#short circuit if the path is in the root
		if base_dir == "res://":
			return "res://"
		base_dirs[base_dir] = true
	if base_dirs.size() > 1:
		var keys = base_dirs.keys()
		# get the shortest path
		keys.sort_custom(func(a, b): return a.length() < b.length())
		var shortest_path = keys[0]
		for base_dir in keys:
			if base_dir == shortest_path:
				continue
			if not base_dir.begins_with(shortest_path):
				return "res://"
		return shortest_path
	return base_dirs.keys()[0]


func _determine_default_dir_structure(selected_items: Array) -> Array:
	var base_dirs: Dictionary = {}
	var had_folder = false
	var rel_base_dir = "res://"
	for item in selected_items:
		if FILE_TREE.item_is_folder(item):
			had_folder = true
			base_dirs[FILE_TREE._get_path(item)] = true
		else:
			var path = FILE_TREE._get_path(item)
			var base_dir = path.get_base_dir()
			base_dirs[base_dir] = true
	if base_dirs.size() > 1 or had_folder:
		rel_base_dir = _determine_rel_base_dir(selected_items)
		if rel_base_dir.is_empty() or rel_base_dir == "res://":
			return [DirStructure.ABSOLUTE_HIERARCHICAL, rel_base_dir]
		else:
			return [DirStructure.RELATIVE_HIERARCHICAL, rel_base_dir]
	return [DirStructure.FLAT, ""]


func _on_export_resources_pressed(_selected_items):
	prev_items = _selected_items
	var ret = _determine_default_dir_structure(_selected_items)
	var default_dir_structure = ret[0]
	REL_BASE_DIR = ret[1]
	open_export_resources_dir_dialog(default_dir_structure)

func _on_extract_resources_pressed(_selected_items):
	prev_items = _selected_items
	var ret = _determine_default_dir_structure(_selected_items)
	var default_dir_structure = ret[0]
	REL_BASE_DIR = ret[1]
	open_extract_resources_dir_dialog(default_dir_structure)


func open_export_resources_dir_dialog(default_dir_structure: DirStructure):
	%ExportResDirDialog.set_current_dir(DIRECTORY.text.get_base_dir())
	var _name = %ExportResDirDialog.get_option_name(0)
	print("NAME: ", _name)
	%ExportResDirDialog.set_option_default(0, int(default_dir_structure))
	open_subwindow(%ExportResDirDialog)

func open_extract_resources_dir_dialog(default_dir_structure: DirStructure):
	var file_dialog: FileDialog = %ExtractResDirDialog
	var _name = file_dialog.get_option_name(0)
	print("NAME: ", _name)
	file_dialog.set_current_dir(DIRECTORY.text.get_base_dir())
	file_dialog.set_option_default(0, int(default_dir_structure))
	open_subwindow(file_dialog)

func setup_export_resources_dir_dialog():
	%ExportResDirDialog.connect("dir_selected", self._on_export_resources_confirmed)

func setup_extract_resources_dir_dialog():
	%ExtractResDirDialog.connect("dir_selected", self._on_extract_resources_dir_selected)



func _ready():
	FILE_TREE =      %FileTree
	EXTRACT_ONLY =   %ExtractOnly
	RECOVER =        %FullRecovery
	VERSION_TEXT =   %VersionText
	INFO_TEXT =      %InfoText
	RECOVER_WINDOW = self #$Control/RecoverWindow
	DIRECTORY = %Directory
	RESOURCE_PREVIEW = %GdreResourcePreview
	HSPLIT_CONTAINER = %HSplitContainer
	SHOW_PREVIEW_BUTTON = %ShowResourcePreview

	if isHiDPI:
		# get_viewport().size *= 2.0
		# get_viewport().content_scale_factor = 2.0
		#ThemeDB.fallback_base_scale = 2.0
		RECOVER_WINDOW.content_scale_factor = 2.0
		RECOVER_WINDOW.size *= 2.0

	clear()
	SHOW_PREVIEW_BUTTON.set_pressed_no_signal(true)
	SHOW_PREVIEW_BUTTON.toggled.emit(true)

	var file_list: Tree = FILE_TREE
	file_list.connect("item_edited", self._on_item_edited)
	setup_extract_dir_dialog()
	setup_export_resources_dir_dialog()
	setup_extract_resources_dir_dialog()
	DIRECTORY.text = DESKTOP_DIR
	FILE_TREE.add_custom_right_click_item("Extract Selected...", self._on_extract_resources_pressed)
	FILE_TREE.add_custom_right_click_item("Export Selected...", self._on_export_resources_pressed)
	load_test()

func add_project(paths: PackedStringArray) -> int:
	if GDRESettings.is_pack_loaded():
		GDRESettings.unload_project()
	clear()
	var err = GDRESettings.load_project(paths)
	if (err != OK):
		return err
	var pckdump = PckDumper.new()
	var skipped = false
	err = pckdump.check_md5_all_files()
	if ERR_SKIP == err:
		skipped = true
		err = OK
	VERSION_TEXT.text = GDRESettings.get_version_string()
	var arr: Array = GDRESettings.get_file_info_array()
	FILE_TREE.add_files_from_packed_infos(arr, skipped)
	INFO_TEXT.text = "Total files: " + String.num_int64(FILE_TREE.num_files)# +
	if FILE_TREE.num_broken > 0 or FILE_TREE.num_malformed > 0:
		INFO_TEXT.text += "   Broken files: " + String.num_int64(FILE_TREE.num_broken) + "    Malformed paths: " + String.num_int64(FILE_TREE.num_malformed)
	DIRECTORY.text = DESKTOP_DIR.path_join(paths[0].get_file().get_basename())
	return OK

func load_test():
	#const path = "/Users/nikita/Workspace/godot-ws/godot-test-bins/satryn.apk"
	const path = '/Users/nikita/Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam/steamapps/common/CRUEL/Cruel.pck'
	add_project([path])
	show_win()

func cancel_extract():
	pass


func hide_win():
	self.hide()


func open_subwindow(window: Window):
	window.set_transient(true)
	window.set_exclusive(true)
	window.popup_centered()

func open_extract_dir_dialog():
	_file_dialog.set_current_dir(DIRECTORY.text.get_base_dir())
	open_subwindow(_file_dialog)

func _dir_selected(path: String):
	DIRECTORY.text = path

func setup_extract_dir_dialog():
	_file_dialog = $ExtractDirDialog
	_file_dialog.connect("dir_selected", self._dir_selected)


func _on_filter_text_changed(new_text: String) -> void:
	FILE_TREE.filter(new_text)

func _on_check_all_pressed() -> void:
	FILE_TREE.check_all_shown(true)


func _on_uncheck_all_pressed() -> void:
	FILE_TREE.check_all_shown(false)


func close():
	if GDRESettings.is_pack_loaded():
		GDRESettings.unload_project()
	RESOURCE_PREVIEW.reset()
	hide_win()
	emit_signal("recovery_done")

func clear():
	FILE_TREE._clear()

func _go():
	hide_win()
	emit_signal("recovery_confirmed", FILE_TREE.get_checked_files(), DIRECTORY.text, EXTRACT_ONLY.is_pressed())


func confirm():
	RESOURCE_PREVIEW.reset()
	if (not EXTRACT_ONLY.is_pressed() and GDREConfig.get_setting("ask_for_download", true)):
		for file in FILE_TREE.get_checked_files():
			var ext = file.get_extension().to_lower()
			if ext == "gdextension" or ext == "gdnlib":
				%DownloadConfirmDialog.popup_centered()
				return
	_go()


func cancelled():
	close()

func _on_directory_button_pressed() -> void:
	open_extract_dir_dialog()

func _on_file_tree_item_selected() -> void:
	if not RESOURCE_PREVIEW.is_visible_in_tree():
		return
	var item = FILE_TREE.get_selected()
	if item:
		var path = item.get_metadata(0)
		if not path.is_empty():
			RESOURCE_PREVIEW.load_resource(path)


func _on_show_resource_preview_toggled(toggled_on: bool) -> void:
	if toggled_on:
		RESOURCE_PREVIEW.visible = true
		# get the current size of the window
		# set the split offset to 50% of the window size
		HSPLIT_CONTAINER.set_split_offset(self.size.x / 2)
		SHOW_PREVIEW_BUTTON.text = "Hide Resource Preview"
		_on_file_tree_item_selected()
	else:
		RESOURCE_PREVIEW.visible = false
		HSPLIT_CONTAINER.set_split_offset(0)
		SHOW_PREVIEW_BUTTON.text = "Show Resource Preview..."
		RESOURCE_PREVIEW.reset()


func _on_download_confirm_dialog_canceled() -> void:
	GDREConfig.set_setting("ask_for_download", not %DontAskAgainCheck.is_pressed())
	GDREConfig.set_setting("download_plugins", false)
	_go()

func _on_download_confirm_dialog_confirmed() -> void:
	GDREConfig.set_setting("ask_for_download", not %DontAskAgainCheck.is_pressed())
	GDREConfig.set_setting("download_plugins", true)
	_go()
