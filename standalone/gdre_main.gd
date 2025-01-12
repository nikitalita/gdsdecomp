extends Control

var ver_major = 0
var ver_minor = 0
var scripts_only = false
var disable_multi_threading = false
var config: ConfigFile = null
var last_error = ""
var CONFIG_PATH = "user://gdre_settings.cfg"

# var isHiDPI = DisplayServer.screen_get_dpi() >= 240
var isHiDPI = false

var gdre_recover = preload("res://gdre_recover.tscn")
var RECOVERY_DIALOG: Control = null
var _file_dialog: Window = null
var REAL_ROOT_WINDOW = null

func test_text_to_bin(txt_to_bin: String, output_dir: String):
	var importer:ImportExporter = ImportExporter.new()
	var dst_file = txt_to_bin.get_file().replace(".tscn", ".scn").replace(".tres", ".res")
	importer.convert_res_txt_2_bin(output_dir, txt_to_bin, dst_file)
	importer.convert_res_bin_2_txt(output_dir, output_dir.path_join(dst_file), dst_file.replace(".scn", ".tscn").replace(".res", ".tres"))

func dequote(arg):
	if arg.begins_with("\"") and arg.ends_with("\""):
		return arg.substr(1, arg.length() - 2)
	if arg.begins_with("'") and arg.ends_with("'"):
		return arg.substr(1, arg.length() - 2)
	return arg

func _on_re_editor_standalone_dropped_files(files: PackedStringArray):
	if files.size() == 0:
		return
	var new_files = []
	for file in files:
		new_files.append(dequote(file))
	$re_editor_standalone.pck_select_request(new_files)

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

func register_dropped_files():
	pass
	var window = get_viewport()
	var err = window.files_dropped.connect(_on_re_editor_standalone_dropped_files)
	if err != OK:
		print("Error: failed to connect window to files_dropped signal")
		print("Type: " + self.get_class())
		print("name: " + str(self.get_name()))


var repo_url = "https://github.com/bruvzg/gdsdecomp"
var latest_release_url = "https://github.com/bruvzg/gdsdecomp/releases/latest"

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
	OS.shell_open(repo_url)

func is_dev_version()-> bool:
	var version = GDRESettings.get_gdre_version()
	if "-dev" in version:
		return true
	return false

func check_version() -> bool:
	# check the version
	var http = HTTPRequest.new()
	# add it to the tree so it doesn't get deleted
	add_child(http)
	http.request_completed.connect(_on_version_check_completed)
	http.request("https://api.github.com/repos/bruvzg/gdsdecomp/releases/latest")
	return true
	
func is_new_version(new_version: String):
	var curr_version = GDRESettings.get_gdre_version()
	if curr_version == new_version:
		return false
	var curr_semver = SemVer.parse_semver(curr_version)
	var new_semver = SemVer.parse_semver(new_version)
	if curr_semver == null or new_semver == null:
		print("Error: invalid semver format")
		print("Current version: " + curr_version)
		print("New version: " + new_version)
		return false
	if new_semver.gt(curr_semver):
		return true
	return false


func _on_version_check_completed(_result, response_code, _headers, body):
	if response_code != 200:
		print("Error: failed to check for latest version")
		return
	var json = JSON.parse_string(body.get_string_from_utf8())
	var checked_version = json["tag_name"].strip_edges()
	var draft = json["draft"]
	var prerelease = json["prerelease"]
	var curr_version = GDRESettings.get_gdre_version()
	
	if draft or (prerelease and not ("-" in curr_version)) or not is_new_version(checked_version):
		return
	
	var update_str = "Update available! Click here! " + curr_version
	repo_url = latest_release_url
	$version_lbl.text = update_str
	print("New version of GDRE available: " + checked_version)
	print("Get it here: " + repo_url)

func _make_new_config():
	config = ConfigFile.new()
	set_showed_disclaimer(false)
	save_config()

func _load_config():
	config = ConfigFile.new()
	if config.load(CONFIG_PATH) != OK:
		_make_new_config()
	return true

func should_show_disclaimer():
	var curr_version = GDRESettings.get_gdre_version()
	var last_showed = config.get_value("General", "last_showed_disclaimer", "<NONE>")
	if last_showed == "<NONE>":
		return true
	if last_showed == curr_version:
		return false
	var curr_semver = SemVer.parse_semver(curr_version)
	var last_semver = SemVer.parse_semver(last_showed)
	if curr_semver == null or last_semver == null:
		return true
	return not (curr_semver.major == last_semver.major and curr_semver.minor == last_semver.minor)

func set_showed_disclaimer(setting: bool):
	var version = "<NONE>"
	if setting:
		version = GDRESettings.get_gdre_version()
	config.set_value("General", "last_showed_disclaimer", version)

func save_config():
	if config == null:
		return ERR_DOES_NOT_EXIST
	if GDRESettings.is_pack_loaded():
		return ERR_FILE_CANT_WRITE
	return config.save(CONFIG_PATH)

func handle_quit(save_cfg = true):
	if GDRESettings.is_pack_loaded():
		GDRESettings.unload_project()
	if save_cfg:
		var ret = save_config()
		if ret != OK and ret != ERR_DOES_NOT_EXIST:
			print("Couldn't save config file!")


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


func _notification(what: int) -> void:
	if what == NOTIFICATION_EXIT_TREE:
		handle_quit()
	elif what == NOTIFICATION_WM_ABOUT:
		$re_editor_standalone.show_about_dialog()
	


func get_glob_files(glob: String) -> PackedStringArray:
	var files: PackedStringArray = Glob.rglob(glob)
	# doing this because non-windows platforms can have '?' and '[' in filenames
	if files.size() == 0 and FileAccess.file_exists(glob):
		files.append(glob)
	return files

func get_globs_files(globs: PackedStringArray) -> PackedStringArray:
	var files: PackedStringArray = []
	for glob in globs:
		files.append_array(get_glob_files(glob))
	return files
func _ready():
	$version_lbl.text = GDRESettings.get_gdre_version()
	# If CLI arguments were passed in, just quit
	var args = get_sanitized_args()
	if handle_cli(args):
		get_tree().quit()
		return
	_load_config()
	var show_disclaimer = should_show_disclaimer()
	show_disclaimer = show_disclaimer and len(args) == 0
	if show_disclaimer:
		set_showed_disclaimer(true)
		save_config()
	register_dropped_files()
	check_version()
	if show_disclaimer:
		$re_editor_standalone.show_about_dialog()
	if len(args) > 0:
		var window = get_viewport()
		window.emit_signal("files_dropped", args)
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
		last_error = "Error: args have to be in the format of --key=value (with equals sign)"
		return ""
	return dequote(split_args[1])

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
	importer.set_multi_thread(not disable_multi_threading)
	importer.export_imports(output_dir, files)
	importer.reset()
				
	
func dump_files(output_dir:String, files: PackedStringArray, ignore_checksum_errors: bool = false) -> int:
	var err:int = OK;
	var pckdump = PckDumper.new()
	# var start_time = Time.get_ticks_msec()
	pckdump.set_multi_thread(not disable_multi_threading)
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
	# var end_time = Time.get_ticks_msec()
	# var secs_taken = (end_time - start_time) / 1000
	# print("Extraction complete in %02dm%02ds" % [(secs_taken) / 60, (secs_taken) % 60])
	return err;

var MAIN_COMMANDS = ["--recover", "--extract", "--compile", "--list-bytecode-versions"]
var MAIN_CMD_NOTES = """Main commands:
--recover=<GAME_PCK/EXE/APK/DIR>   Perform full project recovery on the specified PCK, APK, EXE, or extracted project directory.
--extract=<GAME_PCK/EXE/APK>       Extract the specified PCK, APK, or EXE.
--compile=<GD_FILE>                Compile GDScript files to bytecode (can be repeated and use globs, requires --bytecode)
--decompile=<GDC_FILE>             Decompile GDC files to text (can be repeated and use globs)
--list-bytecode-versions           List all available bytecode versions
--txt-to-bin=<FILE>                Convert text-based scene or resource files to binary format (can be repeated)
--bin-to-txt=<FILE>                Convert binary scene or resource files to text-based format (can be repeated)
"""

var GLOB_NOTES = """Notes on Include/Exclude globs:
	- Recursive patterns can be specified with '**'
		- Example: 'res://**/*.gdc' matches 'res://main.gdc', 'res://scripts/script.gdc', etc.)
	- Globs should be rooted to 'res://' or 'user://'
		- Example: 'res://*.gdc' will match all .gdc files in the root of the project, but not any of the subdirectories.
	- If not rooted, globs will be rooted to 'res://'
		- Example: 'addons/plugin/main.gdc' is equivalent to 'res://addons/plugin/main.gdc'
	- As a special case, if the glob has a wildcard and does contain a directory, it will be assumed to be a recursive pattern.
		- Example: '*.gdc' would be equivalent to 'res://**/*.gdc'
	- Include/Exclude globs will only match files that are actually in the project PCK/dir, not any non-present resource source files.
		Example: 
			- A project contains the file "res://main.gdc". 'res://main.gd' is the source file of 'res://main.gdc',
			  but is not included in the project PCK.
			- Performing project recovery with the include glob 'res://main.gd' would not recover 'main.gd'.
			- Performing project recovery with the include glob 'res://main.gdc' would recover 'res://main.gd'
"""

var RECOVER_OPTS_NOTES = """Recover/Extract Options:

--key=<KEY>                 The Key to use if project is encrypted as a 64-character hex string,
							e.g.: '000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F'
--output-dir=<DIR>          Output directory, defaults to <NAME_extracted>, or the project directory if one of specified
--scripts-only              Only extract/recover scripts
--include=<GLOB>            Include files matching the glob pattern (can be repeated)
--exclude=<GLOB>            Exclude files matching the glob pattern (can be repeated)
--ignore-checksum-errors    Ignore MD5 checksum errors when extracting/recovering
"""
# todo: handle --key option
var COMPILE_OPTS_NOTES = """Decompile/Compile Options:
--bytecode=<COMMIT_OR_VERSION>          Either the commit hash of the bytecode revision (e.g. 'f3f05dc'),
										   or the version of the engine (e.g. '4.3.0')
--output-dir=<DIR>                      Directory where compiled files will be output to. 
										  - If not specified, compiled files will be output to the same location 
										  (e.g. '<PROJ_DIR>/main.gd' -> '<PROJ_DIR>/main.gdc')
"""
func print_usage():
	print("Godot Reverse Engineering Tools")
	print("")
	print("Without any CLI options, the tool will start in GUI mode")
	print("\nGeneral options:")
	print("  -h, --help: Display this help message")
	print("\nFull Project Recovery options:")
	print("Usage: GDRE_Tools.exe --headless <main_command> [options]")
	print(MAIN_CMD_NOTES)
	print(RECOVER_OPTS_NOTES)
	print(GLOB_NOTES)
	print(COMPILE_OPTS_NOTES)


# TODO: remove this hack
var translation_only = false
var SCRIPTS_EXT = ["gd", "gdc", "gde"]
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
	var abs_path = exec_path.path_join(path).simplify_path()
	return abs_path

func normalize_cludes(cludes: PackedStringArray, dir = "res://") -> PackedStringArray:
	var new_cludes: PackedStringArray = []
	if dir != dir.get_base_dir() and dir.ends_with("/"):
		dir = dir.substr(0, dir.length() - 1)
	for clude in cludes:
		clude = clude.replace("\\", "/")
		if not "**" in clude and "*" in clude and not "/" in clude:
			new_cludes.append("res://**/" + clude)
			# new_cludes.append("user://**/" + clude)
			continue
		if clude.begins_with("/") and dir == "res://":
			clude = clude.substr(1, clude.length() - 1)
		if not clude.is_absolute_path():
			clude = dir.path_join(clude)
		elif dir != "res://":
			clude = clude.replace("res:/", dir)
		elif clude.begins_with("/"):
			clude = dir + clude.substr(1, clude.length() - 1)
		new_cludes.append(clude.simplify_path())
	return new_cludes


func recovery(  input_files:PackedStringArray,
				output_dir:String,
				enc_key:String,
				extract_only: bool,
				ignore_checksum_errors: bool = false,
				excludes: PackedStringArray = [],
				includes: PackedStringArray = []):
	var _new_files = []
	for file in input_files:
		file = get_cli_abs_path(file)
		var _files = get_glob_files(file)
		if _files.size() > 0:
			_new_files.append_array(_files)
		else:
			print_usage()
			print("Error: failed to locate " + file)
			return
	print("Input files: ", str(_new_files))
	input_files = _new_files
	var input_file = input_files[0]
	var da:DirAccess
	var is_dir:bool = false
	var err: int = OK
	var parent_dir = "res://"
	# get the current time
	var start_time = Time.get_ticks_msec()
	if output_dir == "":
		output_dir = input_file.get_basename()
		if output_dir.get_extension():
			output_dir += "_recovery"
	else:
		output_dir = get_cli_abs_path(output_dir)

	da = DirAccess.open(input_file.get_base_dir())

	# check if da works
	if da == null:
		print_usage()
		print("Error: failed to locate parent dir for " + input_file)
		return
	#directory
	if da.dir_exists(input_file):
		if input_files.size() > 1:
			print_usage()
			print("Error: cannot specify multiple directories")
			return
		if input_file.get_extension().to_lower() == "app":
			is_dir = false
		elif !da.dir_exists(input_file.path_join(".import")) && !da.dir_exists(input_file.path_join(".godot")):
			print_usage()
			print("Error: " + input_file + " does not appear to be a project directory")
			return
		else:
			parent_dir = input_file
			is_dir = true
	#PCK/APK
	elif not da.file_exists(input_file):
		print_usage()
		print("Error: failed to locate " + input_file)
		return

	GDRESettings.open_log_file(output_dir)
	if (enc_key != ""):
		err = GDRESettings.set_encryption_key_string(enc_key)
		if (err != OK):
			print_usage()
			print("Error: failed to set key!")
			return
	
	err = GDRESettings.load_project(input_files, extract_only)
	if (err != OK):
		print_usage()
		print("Error: failed to open ", (input_files))
		return

	print("Successfully loaded PCK!") 
	ver_major = GDRESettings.get_ver_major()
	ver_minor = GDRESettings.get_ver_minor()
	var version:String = GDRESettings.get_version_string()
	print("Version: " + version)
	var files: PackedStringArray = []
	if translation_only and scripts_only:
		print("Error: cannot specify both --translation-only and --scripts-only")
		return
	elif ((translation_only or scripts_only) and (includes.size() > 0 or excludes.size() > 0)):
		print("Error: cannot specify both --translation-only or --scripts-only and --include or --exclude")
		return
	if (translation_only):
		var new_files:PackedStringArray = []
		# remove all the non ".translation" files
		for file in GDRESettings.get_file_list():
			if (file.get_extension().to_lower() == "translation"):
				new_files.append(file)
		files.append_array(new_files)
		print("Translation only mode, only extracting translation files")
	elif scripts_only:
		var new_files:PackedStringArray = []
		# remove all the non ".gd" files
		for file in GDRESettings.get_file_list():
			if (file.get_extension().to_lower() in SCRIPTS_EXT):
				new_files.append(file)
		files.append_array(new_files)
		print("Scripts only mode, only extracting scripts")
	else:
		if includes.size() > 0:
			includes = normalize_cludes(includes, parent_dir)
			files = get_globs_files(includes)
			if len(files) == 0:
				print("Error: no files found that match includes")
				print("Includes: " + str(includes))
				print(GLOB_NOTES)
				return
		else:
			files = GDRESettings.get_file_list()
		if excludes.size() > 0:
			excludes = normalize_cludes(excludes, parent_dir)
			var result = Glob.fnmatch_list(files, excludes)
			for file in result:
				files.remove_at(files.rfind(file))

		if (includes.size() > 0 or excludes.size() > 0) and files.size() == 0:
			print("Error: no files to extract after filtering")
			if len(includes) > 0:
				print("Includes: " + str(includes))
			if len(excludes) > 0:
				print("Excludes: " + str(excludes))
			print(GLOB_NOTES)
			return

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
	var end_time;
	var secs_taken;
	if (extract_only):
		end_time = Time.get_ticks_msec()
		secs_taken = (end_time - start_time) / 1000
		print("Extraction operation complete in %02dm%02ds" % [(secs_taken) / 60, (secs_taken) % 60])
		return
	export_imports(output_dir, files)
	end_time = Time.get_ticks_msec()
	secs_taken = (end_time - start_time) / 1000
	print("Recovery complete in %02dm%02ds" % [(secs_taken) / 60, (secs_taken) % 60])


func print_version():
	print("Godot RE Tools " + GDRESettings.get_gdre_version())

func close_log():
	var path = GDRESettings.get_log_file_path()
	if path == "":
		return
	GDRESettings.close_log_file()
	print("Log file written to: " + path)
	print("Please include this file when reporting issues!")

func ensure_dir_exists(dir: String):
	var da:DirAccess = DirAccess.open(GDRESettings.get_exec_dir())
	if !da.dir_exists(dir):
		da.make_dir_recursive(dir)

func get_decomp(bytecode_version: String) -> GDScriptDecomp:
	var decomp: GDScriptDecomp = null
	if '.' in bytecode_version:
		decomp = GDScriptDecomp.create_decomp_for_version(bytecode_version)
	else:
		decomp = GDScriptDecomp.create_decomp_for_commit(bytecode_version.hex_to_int())
	if decomp == null:
		print("Error: failed to create decompiler for commit " + bytecode_version + "!\n(run --list-bytecode-versions to see available versions)")
	return decomp

func compile(files: PackedStringArray, bytecode_version: String, output_dir: String):
	# TODO: handle key
	if output_dir == "":
		output_dir = get_cli_abs_path(".") # default to current directory
	if bytecode_version == "":
		print("Error: --bytecode is required for --compile (use --list-bytecode-versions to see available versions)")
		print(COMPILE_OPTS_NOTES)
		return -1
	var decomp: GDScriptDecomp = get_decomp(bytecode_version)
	if decomp == null:
		return -1
	print("Compiling to bytecode version %x (%s)" % [decomp.get_bytecode_rev(), decomp.get_engine_version()])

	var new_files = get_globs_files(files)
	if new_files.size() == 0:
		print("Error: no files found to compile")
		return -1
	ensure_dir_exists(output_dir)
	for file in new_files:
		print("Compiling " + file)
		if file.get_extension() != "gd":
			print("Error: " + file + " is not a GDScript file")
			continue
		var f = FileAccess.open(file, FileAccess.READ)
		var code = f.get_as_text()
		var bytecode: PackedByteArray = decomp.compile_code_string(code)
		if bytecode.is_empty():
			print("Error: failed to compile " + file)
			print(decomp.get_error_message())
			continue
		var out_file = output_dir.path_join(file.get_file().replace(".gd", ".gdc"))
		var out_f = FileAccess.open(out_file, FileAccess.WRITE)
		out_f.store_buffer(bytecode)
		out_f.close()
		print("Compiled " + file + " to " + out_file)		
	print("Compilation complete")
	return 0

func decompile(files: PackedStringArray, bytecode_version: String, output_dir: String, key: String = ""):
	if output_dir == "":
		output_dir = get_cli_abs_path(".") # default to current directory
	if bytecode_version == "":
		print("Error: --bytecode is required for --decompile (use --list-bytecode-versions to see available versions)")
		print(COMPILE_OPTS_NOTES)
		return -1
	var decomp: GDScriptDecomp = get_decomp(bytecode_version)
	if decomp == null:
		return -1
	print("Decompiling from bytecode version %x (%s)" % [decomp.get_bytecode_rev(), decomp.get_engine_version()])
	var new_files = get_globs_files(files)
	if new_files.size() == 0:
		print("Error: no files found to decompile")
		return -1
	ensure_dir_exists(output_dir)

	var err = OK
	if key != "":
		err = GDRESettings.set_encryption_key_string(key)
		if err != OK:
			print("Error: failed to set key!")
			return -1
	for file in new_files:
		var src_ext = file.get_extension().to_lower()
		
		if src_ext != "gdc" and src_ext != "gde":
			print("Error: " + file + " is not a GDScript bytecode file")
			continue
		print("Decompiling " + file)
		err = decomp.decompile_byte_code(file)
		var out_file = file.get_basename() + ".gd"
		if output_dir:
			out_file = output_dir.path_join(out_file.get_file())
		print("Output file: " + out_file)

		if err != OK:
			print("Error: failed to decompile " + file)
			print(decomp.get_error_message())
			continue
		var text = decomp.get_script_text()
		var out_f = FileAccess.open(out_file, FileAccess.WRITE)
		if out_f == null:
			print("Error: failed to open " + out_file + " for writing")
			continue
		out_f.store_string(text)
		out_f.close()
		print("Decompiled " + file + " to " + out_file)		
	print("Decompilation complete")
	return 0

func get_sanitized_args():
	var args = OS.get_cmdline_args()
	#var scene_path = get_tree().root.scene_file_path
	var scene_path = "res://gdre_main.tscn"
	if args.size() > 0 and args[0] == scene_path:
		return args.slice(1)
	return args

func text_to_bin(files: PackedStringArray, output_dir: String):
	var importer:ImportExporter = ImportExporter.new()
	for file in files:
		file = get_cli_abs_path(file)
		var dst_file = file.get_file().replace(".tscn", ".scn").replace(".tres", ".res")
		importer.convert_res_txt_2_bin(output_dir, file, dst_file)

func bin_to_text(files: PackedStringArray, output_dir: String):
	var importer:ImportExporter = ImportExporter.new()
	for file in files:
		file = get_cli_abs_path(file)
		var dst_file = file.get_file().replace(".scn", ".tscn").replace(".res", ".tres")
		importer.convert_res_bin_2_txt(output_dir, file, dst_file)

func handle_cli(args: PackedStringArray) -> bool:
	var input_extract_file:PackedStringArray = []
	var input_file:PackedStringArray = []
	var output_dir: String = ""
	var enc_key: String = ""
	var txt_to_bin = PackedStringArray()
	var bin_to_txt = PackedStringArray()
	var ignore_md5: bool = false
	var decompile_files = PackedStringArray()
	var compile_files = PackedStringArray()
	var bytecode_version: String = ""
	var main_cmds = {}
	var excludes: PackedStringArray = []
	var includes: PackedStringArray = []
	var prepop: PackedStringArray = []
	var set_setting: bool = false
	if (args.size() == 0):
		return false
	var any_commands = false
	for i in range(args.size()):
		var arg:String = args[i]
		if arg.begins_with("--"):
			any_commands = true
			break
	if any_commands == false:
		if not GDRESettings.is_headless():
			# not cli mode, drag-and-drop
			return false
		print_usage()
		print("ERROR: no command specified")
		return true
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help":
			print_version()
			print_usage()
			return true
		elif arg.begins_with("--version"):
			print_version()
			return true
		elif arg.begins_with("--extract"):
			input_extract_file.append(get_arg_value(arg).simplify_path())
			main_cmds["extract"] = true
		elif arg.begins_with("--recover"):
			input_file.append(get_arg_value(arg).simplify_path())
			main_cmds["recover"] = true
		elif arg.begins_with("--txt-to-bin"):
			txt_to_bin.append(get_arg_value(arg).simplify_path())
			main_cmds["txt-to-bin"] = true
		elif arg.begins_with("--bin-to-txt"):
			bin_to_txt.append(get_arg_value(arg).simplify_path())
			main_cmds["bin-to-txt"] = true
		elif arg.begins_with("--output-dir"):
			output_dir = get_arg_value(arg).simplify_path()
		elif arg.begins_with("--scripts-only"):
			scripts_only = true
		elif arg.begins_with("--key"):
			enc_key = get_arg_value(arg)
		elif arg.begins_with("--ignore-checksum-errors"):
			ignore_md5 = true
		elif arg.begins_with("--translation-only"):
			translation_only = true
		elif arg.begins_with("--disable-multithreading"):
			disable_multi_threading = true
		elif arg.begins_with("--enable-experimental-plugin-downloading"):
			GDRESettings.set_setting_download_plugins(true)
			set_setting = true
		elif arg.begins_with("--list-bytecode-versions"):
			var versions = GDScriptDecomp.get_bytecode_versions()
			print("\n--- Available bytecode versions:")
			for version in versions:
				print(version)
			return true
		elif arg.begins_with("--bytecode"):
			bytecode_version = get_arg_value(arg)
		elif arg.begins_with("--decompile"):
			main_cmds["decompile"] = true
			decompile_files.append(get_arg_value(arg))
		elif arg.begins_with("--compile"):
			main_cmds["compile"] = true
			compile_files.append(get_arg_value(arg))
		elif arg.begins_with("--exclude"):
			excludes.append(get_arg_value(arg))
		elif arg.begins_with("--include"):
			includes.append(get_arg_value(arg))
		elif arg.begins_with("--plcache"):
			main_cmds["plcache"] = true
			prepop.append(get_arg_value(arg))
		else:
			print_usage()
			print("ERROR: invalid option '" + arg + "'")
			return true
		if last_error != "":
			print_usage()
			print(last_error)
			return true
	if main_cmds.size() > 1:
		print_usage()
		print("ERROR: invalid option! Must specify only one of " + ", ".join(MAIN_COMMANDS))
		return true
	if prepop.size() > 0:
		var start_time = Time.get_ticks_msec()
		GDRESettings.prepop_plugin_cache(prepop)
		var end_time = Time.get_ticks_msec()
		var secs_taken = (end_time - start_time) / 1000
		print("Prepop complete in %02dm%02ds" % [(secs_taken) / 60, (secs_taken) % 60])
	elif compile_files.size() > 0:
		compile(compile_files, bytecode_version, output_dir)
	elif decompile_files.size() > 0:
		decompile(decompile_files, bytecode_version, output_dir, enc_key)
	elif not input_file.is_empty():
		recovery(input_file, output_dir, enc_key, false, ignore_md5, excludes, includes)
		GDRESettings.unload_project()
		close_log()
	elif not input_extract_file.is_empty():
		recovery(input_extract_file, output_dir, enc_key, true, ignore_md5, excludes, includes)
		GDRESettings.unload_project()
		close_log()
	elif txt_to_bin.is_empty() == false:
		text_to_bin(txt_to_bin, output_dir)
	elif bin_to_txt.is_empty() == false:
		bin_to_text(bin_to_txt, output_dir)
	elif set_setting:
		return false # don't quit
	else:
		print_usage()
		print("ERROR: invalid option! Must specify one of " + ", ".join(MAIN_COMMANDS))
	return true
