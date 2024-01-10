extends Control

var ver_major = 0
var ver_minor = 0

var isHiDPI = DisplayServer.screen_get_dpi() >= 240
#var isHiDPI = false

var gdre_recover = preload("res://gdre_recover.tscn")
var RECOVERY_DIALOG: Control = null
var _file_dialog: Window = null
var REAL_ROOT_WINDOW = null

func test_text_to_bin(txt_to_bin: String, output_dir: String):
	var importer:ImportExporter = ImportExporter.new()
	var dst_file = txt_to_bin.get_file().replace(".tscn", ".scn").replace(".tres", ".res")
	importer.convert_res_txt_2_bin(output_dir, txt_to_bin, dst_file)
	importer.convert_res_bin_2_txt(output_dir, output_dir.path_join(dst_file), dst_file.replace(".scn", ".tscn").replace(".res", ".tres"))

func popup_error_box(message: String, title: String, parent_window: Window) -> AcceptDialog:
	var dialog = AcceptDialog.new()
	dialog.set_text(message)
	dialog.set_title(title)
	get_tree().get_root().add_child(dialog)	
	dialog.connect("confirmed", parent_window.show)
	dialog.connect("canceled", parent_window.show)
	dialog.popup_centered()
	return dialog

const ERR_SKIP = 45

func _on_recovery_done():
	if RECOVERY_DIALOG:
		RECOVERY_DIALOG.hide_win()
		get_tree().get_root().remove_child(RECOVERY_DIALOG)
		RECOVERY_DIALOG = null
	else:
		print("Recovery dialog not instantiated!!!")

var _last_path = ""

func _retry_recover():
	var path = _last_path
	_last_path = ""
	_on_recover_project_file_selected(path)

func _on_recover_project_file_selected(path):
	# open the recover dialog
	if _file_dialog:
		_last_path = path
		_file_dialog.connect("tree_exited", self._retry_recover)
		_file_dialog.hide()
		_file_dialog.queue_free()
		_file_dialog = null
		return
	assert(gdre_recover.can_instantiate())
	RECOVERY_DIALOG = gdre_recover.instantiate()
	RECOVERY_DIALOG.set_root_window(REAL_ROOT_WINDOW)
	REAL_ROOT_WINDOW.add_child(RECOVERY_DIALOG)
	REAL_ROOT_WINDOW.move_child(RECOVERY_DIALOG, self.get_index() -1)
	RECOVERY_DIALOG.add_pack(path)
	RECOVERY_DIALOG.connect("recovery_done", self._on_recovery_done)
	RECOVERY_DIALOG.show_win()
	
func _on_recover_project_dir_selected(path):
	# just check if the dir path ends in ".app"
	if path.ends_with(".app"):
		_on_recover_project_file_selected(path)
	else:
		# pop up an accept dialog
		popup_error_box("Invalid Selection!!", "Error", REAL_ROOT_WINDOW)
		return


func open_about_window():
	$LegalNoticeWindow.popup_centered()

func open_setenc_window():
	$SetEncryptionKeyWindow.popup_centered()
	
func open_recover_file_dialog():

	# pop open a file dialog
	_file_dialog = FileDialog.new()
	_file_dialog.set_use_native_dialog(true)
	# This is currently broken in Godot, so we use the native dialogs
	#var prev_size = _file_dialog.size
	if isHiDPI:
		_file_dialog.size *= 2.0
		#_file_dialog.min_size = _file_dialog.size
		#d_viewport.content_scale_factor = 2.0
	_file_dialog.set_access(FileDialog.ACCESS_FILESYSTEM)
	_file_dialog.file_mode = FileDialog.FILE_MODE_OPEN_ANY #FileDialog.FILE_MODE_OPEN_FILE
	#_file_dialog.filters = ["*"]
	_file_dialog.filters = ["*.exe,*.bin,*.32,*.64,*.x86_64,*.x86,*.arm64,*.universal,*.pck,*.apk,*.app;Supported files"]
	#_file_dialog.filters = ["*.exe,*.bin,*.32,*.64,*.x86_64,*.x86,*.arm64,*.universal;Self contained executable files", "*.pck;PCK files", "*.apk;APK files", "*;All files"]
	## TODO: remove this
	_file_dialog.current_dir = "/Users/nikita/Workspace/godot-test-bins"
	if (_file_dialog.current_dir.is_empty()):
		_file_dialog.current_dir = GDRESettings.get_exec_dir()
	_file_dialog.connect("file_selected", self._on_recover_project_file_selected)
	_file_dialog.connect("dir_selected", self._on_recover_project_dir_selected)

	get_tree().get_root().add_child(_file_dialog)
	_file_dialog.popup_centered()

	

func _on_REToolsMenu_item_selected(index):
	match index:
		0:
			# Recover Project...
			open_recover_file_dialog()
		1:  # set key
			# Open the set key dialog
			open_setenc_window()
		2:  # about
			open_about_window()
		3:  # Report a bug
			OS.shell_open("https://github.com/bruvzg/gdsdecomp/issues/new?assignees=&labels=bug&template=bug_report.yml&sys_info=" + GDRESettings.get_sys_info_string())
		4:  # Quit
			get_tree().quit()
			

	
func _on_re_editor_standalone_write_log_message(message):
	$log_window.text += message
	$log_window.scroll_to_line($log_window.get_line_count() - 1)

func _on_setenc_key_ok_pressed():
	# get the current text in the line edit
	var keytextbox = $SetEncryptionKeyWindow/VBoxContainer/KeyText
	var key:String = keytextbox.text
	if key.length() == 0:
		GDRESettings.reset_encryption_key()
	# set the key
	else:
		var err:int = GDRESettings.set_encryption_key_string(key)
		if (err != OK):
			keytextbox.text = ""
			# pop up an accept dialog
			popup_error_box("Invalid key!\nKey must be a hex string with 64 characters", "Error", $SetEncryptionKeyWindow)
			return
	# close the window
	$SetEncryptionKeyWindow.hide()
	
func _on_setenc_key_cancel_pressed():
	$SetEncryptionKeyWindow.hide()


func _on_version_lbl_pressed():
	OS.shell_open("https://github.com/bruvzg/gdsdecomp")

	#readd_items($MenuContainer/REToolsMenu
func _resize_menu_times(menu_container:HBoxContainer):
	for menu_btn: MenuButton in menu_container.get_children():

		var popup : PopupMenu = menu_btn.get_popup()
		# broken
		#popup.visible = true
		#var size = popup.size * 2
		#popup.size = size
		#popup.min_size = size
		#popup.max_size = size * 2
		#popup.get_viewport().content_scale_factor = 2.0
		#popup.visible = false
		# readd_items(menu_btn)
		if isHiDPI:
			var old_theme = popup.theme
			#menu_btn.connect("theme_changed", self._on_menu_btn_theme_changed)
			var new_theme = old_theme
			if !new_theme:
				new_theme = Theme.new()
			var font_size = new_theme.get_font_size("", "")
			#new_theme.set_font_size("", "", font_size * 2)
			new_theme.set_default_font_size(font_size * 2)
			popup.theme = new_theme
		var item_count = menu_btn.get_popup().get_item_count()
		for i in range(item_count):
			var icon: Texture2D = popup.get_item_icon(i)
			if icon:
				var icon_size = icon.get_size()
				popup.set_item_icon_max_width(i, icon_size.x * (2.0 if isHiDPI else 0.5))

func _ready():
	if handle_cli():
		get_tree().quit()
	REAL_ROOT_WINDOW = get_window()
	var popup_menu_gdremenu:PopupMenu = $MenuContainer/REToolsMenu.get_popup()
	popup_menu_gdremenu.connect("id_pressed", self._on_REToolsMenu_item_selected)
	GDRESettings.connect("write_log_message", self._on_re_editor_standalone_write_log_message)

	$version_lbl.text = GDRESettings.get_gdre_version()
	# If CLI arguments were passed in, just quit
	# check if the current screen is hidpi
	$LegalNoticeWindow/OkButton.connect("pressed", $LegalNoticeWindow.hide)
	$LegalNoticeWindow.connect("close_requested", $LegalNoticeWindow.hide)
	if isHiDPI:
		# set the content scaling factor to 2x
		ThemeDB.fallback_base_scale = 2.0
		get_viewport().content_scale_factor = 2.0
		get_viewport().size *= 2
		$SetEncryptionKeyWindow.content_scale_factor = 2.0
		$SetEncryptionKeyWindow.size *= 2
		$LegalNoticeWindow.content_scale_factor = 2.0
		$LegalNoticeWindow.size *=2
	_resize_menu_times($MenuContainer)

# CLI stuff below

func get_arg_value(arg):
	var split_args = arg.split("=")
	if split_args.size() < 2:
		print("Error: args have to be in the format of --key=value (with equals sign)")
		get_tree().quit()
	return split_args[1]

func normalize_path(path: String):
	return path.replace("\\","/")

func test_decomp(fname):
	var decomp = GDScriptDecomp_ed80f45.new()
	var f = fname
	if f.get_extension() == "gdc":
		print("decompiling " + f)
		#
		#if decomp.decompile_byte_code(output_dir.path_join(f)) != OK: 
		if decomp.decompile_byte_code(f) != OK: 
			print("error decompiling " + f)
		else:
			var text = decomp.get_script_text()
			var gdfile:FileAccess = FileAccess.open(f.replace(".gdc",".gd"), FileAccess.WRITE)
			if gdfile == null:
				gdfile.store_string(text)
				gdfile.close()
				#da.remove(f)
				print("successfully decompiled " + f)
			else:
				print("error failed to save "+ f)

func export_imports(output_dir:String, files: PackedStringArray):
	var importer:ImportExporter = ImportExporter.new()
	importer.export_imports(output_dir, files)
	importer.reset()
				
	
func dump_files(output_dir:String, files: PackedStringArray, ignore_checksum_errors: bool = false) -> int:
	var err:int = OK;
	var pckdump = PckDumper.new()
	if err == OK:
		err = pckdump.check_md5_all_files()
		if err != OK:
			if (err != ERR_SKIP and not ignore_checksum_errors):
				print("MD5 checksum failed, not proceeding...")
				return err
			elif (ignore_checksum_errors):
				print("MD5 checksum failed, but --ignore_checksum_errors specified, proceeding anyway...")
		err = pckdump.pck_dump_to_dir(output_dir, files)
		if err != OK:
			print("error dumping to dir")
			return err
	else:
		print("ERROR: failed to load exe")
	return err;

func print_usage():
	print("Godot Reverse Engineering Tools")
	print("")
	print("Without any CLI options, the tool will start in GUI mode")
	print("\nGeneral options:")
	print("  -h, --help: Display this help message")
	print("\nFull Project Recovery options:")
	print("Usage: GDRE_Tools.exe --headless --recover=<PCK_OR_EXE_OR_EXTRACTED_ASSETS_DIR> [options]")
	print("")
	print("--recover=<GAME_PCK/EXE/APK_OR_EXTRACTED_ASSETS_DIR>\t\tThe PCK, APK, EXE, or extracted project directory to perform full project recovery on")
	print("--extract=<GAME_PCK/EXE/APK_OR_EXTRACTED_ASSETS_DIR>\t\tThe PCK, APK, or EXE to extract")

	print("\nOptions:\n")
	print("--key=<KEY>\t\tThe Key to use if project is encrypted (hex string)")
	print("--output-dir=<DIR>\t\tOutput directory, defaults to <NAME_extracted>, or the project directory if one of specified")
	print("--ignore-checksum-errors\t\tIgnore MD5 checksum errors when extracting/recovering")
	print("--translation-only\t\tOnly extract translation files")

# TODO: remove this hack
var translation_only = false

func copy_dir(src:String, dst:String) -> int:
	var da:DirAccess = DirAccess.open(src)
	if !da.dir_exists(src):
		print("Error: " + src + " does not appear to be a directory")
		return ERR_FILE_NOT_FOUND
	da.make_dir_recursive(dst)
	da.copy_dir(src, dst)
	return OK

func get_cli_abs_path(path:String) -> String:
	if path.is_absolute_path():
		return path
	var exec_path = GDRESettings.get_exec_dir()
	var abs_path = exec_path.path_join(path)
	return abs_path

func recovery(  input_file:String,
				output_dir:String,
				enc_key:String,
				extract_only: bool,
				ignore_checksum_errors: bool = false):
	var da:DirAccess
	var is_dir:bool = false
	var err: int = OK
	input_file = get_cli_abs_path(input_file)
	if output_dir == "":
		output_dir = input_file.get_basename()
		if output_dir.get_extension():
			output_dir += "_recovery"
	else:
		output_dir = get_cli_abs_path(output_dir)

	da = DirAccess.open(input_file.get_base_dir())

	# check if da works
	if da == null:
		print("Error: failed to locate parent dir for " + input_file)
		return
	#directory
	if da.dir_exists(input_file):
		if input_file.get_extension().to_lower() == "app":
			is_dir = false
		elif !da.dir_exists(input_file.path_join(".import")) && !da.dir_exists(input_file.path_join(".godot")):
			print("Error: " + input_file + " does not appear to be a project directory")
			return
		else:
			is_dir = true
	#PCK/APK
	elif not da.file_exists(input_file):
		print("Error: failed to locate " + input_file)
		return

	GDRESettings.open_log_file(output_dir)
	if (enc_key != ""):
		err = GDRESettings.set_encryption_key_string(enc_key)
		if (err != OK):
			print("Error: failed to set key!")
			return
	
	err = GDRESettings.load_pack(input_file)
	if (err != OK):
		print("Error: failed to open " + input_file)
		return

	print("Successfully loaded PCK!") 
	ver_major = GDRESettings.get_ver_major()
	ver_minor = GDRESettings.get_ver_minor()
	var version:String = GDRESettings.get_version_string()
	print("Version: " + version)
	var files: PackedStringArray = []
	if (translation_only):
		files = GDRESettings.get_file_list()
		var new_files:PackedStringArray = []
		# remove all the non ".translation" files
		for i in range(files.size()-1, -1, -1):
			if (files[i].get_extension() == "translation"):
				new_files.append(files[i])
		files = new_files
		print("Translation only mode, only extracting translation files")

	if output_dir != input_file and not is_dir: 
		if (da.file_exists(output_dir)):
			print("Error: output dir appears to be a file, not extracting...")
			return
	if is_dir:
		if extract_only:
			print("Why did you open a folder to extract it??? What's wrong with you?!!?")
			return
		if copy_dir(input_file, output_dir) != OK:
			print("Error: failed to copy " + input_file + " to " + output_dir)
			return
	else:
		err = dump_files(output_dir, files, ignore_checksum_errors)
		if (err != OK):
			print("Error: failed to extract PAK file, not exporting assets")
			return
	if (extract_only):
		return
	export_imports(output_dir, files)

func print_version():
	print("Godot RE Tools " + GDRESettings.get_gdre_version())

func close_log():
	var path = GDRESettings.get_log_file_path()
	if path == "":
		return
	GDRESettings.close_log_file()
	print("Log file written to: " + path)
	print("Please include this file when reporting issues!")

func handle_cli() -> bool:
	var args = OS.get_cmdline_args()
	var input_extract_file:String = ""
	var input_file:String = ""
	var output_dir: String = ""
	var enc_key: String = ""
	var txt_to_bin: String = ""
	var ignore_md5: bool = false
	if (args.size() == 0 or (args.size() == 1 and args[0] == "res://gdre_main.tscn")):
		return false
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help":
			print_version()
			print_usage()
			get_tree().quit()
		if arg.begins_with("--version"):
			print_version()
			get_tree().quit()
		if arg.begins_with("--extract"):
			input_extract_file = normalize_path(get_arg_value(arg))
		if arg.begins_with("--recover"):
			input_file = normalize_path(get_arg_value(arg))
		if arg.begins_with("--txt-to-bin"):
			txt_to_bin = normalize_path(get_arg_value(arg))	
		elif arg.begins_with("--output-dir"):
			output_dir = normalize_path(get_arg_value(arg))
		elif arg.begins_with("--key"):
			enc_key = get_arg_value(arg)
		elif arg.begins_with("--ignore-checksum-errors"):
			ignore_md5 = true
		elif arg.begins_with("--translation-only"):
			translation_only = true

	if input_file != "":
		recovery(input_file, output_dir, enc_key, false, ignore_md5)
		GDRESettings.unload_pack()
		close_log()
		get_tree().quit()
	elif input_extract_file != "":
		recovery(input_extract_file, output_dir, enc_key, true, ignore_md5)
		GDRESettings.unload_pack()
		close_log()
		get_tree().quit()
	elif txt_to_bin != "":
		txt_to_bin = get_cli_abs_path(txt_to_bin)
		output_dir = get_cli_abs_path(output_dir)
		test_text_to_bin(txt_to_bin, output_dir)
		get_tree().quit()
	else:
		print("ERROR: invalid option")

		print_usage()
		get_tree().quit()
	return true
