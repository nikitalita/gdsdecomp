extends GodotREEditorStandalone

var last_error = ""

const ERR_SKIP = 45

func get_glob_files(glob: String) -> PackedStringArray:
	var files: PackedStringArray = Glob.rglob(glob, true)
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
	if not GDRESettings.is_headless():
		print("CLI Only")
		get_tree().quit(1)
		return
	# If CLI arguments were passed in, just quit
	var args = get_sanitized_args()
	var ret =  handle_cli(args)
	get_tree().quit(ret)

# CLI stuff below

func dequote(arg):
	if arg.begins_with("\"") and arg.ends_with("\""):
		return arg.substr(1, arg.length() - 2)
	if arg.begins_with("'") and arg.ends_with("'"):
		return arg.substr(1, arg.length() - 2)
	return arg

func get_arg_value(arg):
	var split_args = arg.split("=", false, 1)
	if split_args.size() < 2:
		last_error = "Error: args have to be in the format of --key=value (with equals sign)"
		return ""
	return dequote(split_args[1])

func export_imports(output_dir:String, files: PackedStringArray):
	var importer:ImportExporter = ImportExporter.new()
	importer.export_imports(output_dir, files)
	importer.reset()


func dump_files(output_dir:String, files: PackedStringArray, ignore_checksum_errors: bool = false) -> int:
	var err:int = OK;
	var pckdump = PckDumper.new()
	# var start_time = Time.get_ticks_msec()
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

var MAIN_COMMANDS = ["--extract-translation", "--replace-translation"]
var MAIN_CMD_NOTES = """Main commands:
--extract-translation=<GAME_PCK/EXE/APK/DIR>    Extract translations csv on the specified PCK, APK, EXE.
--replace-translation=<GAME_PCK/EXE/APK>        Replace and add translations on the specified PCK, APK, or EXE.
"""

# todo: handle --key option
var COMPILE_OPTS_NOTES = """Decompile/Compile Options:
--bytecode=<COMMIT_OR_VERSION>          Either the commit hash of the bytecode revision (e.g. 'f3f05dc'),
										   or the version of the engine (e.g. '4.3.0')
--output=<DIR>                          Directory where compiled files will be output to.
										  - If not specified, compiled files will be output to the same location
										  (e.g. '<PROJ_DIR>/main.gd' -> '<PROJ_DIR>/main.gdc')
"""

var EXTRACT_TRANSLATION_NOTES = """Extract Translations Options:
--output=<DIR>                 Output directory, defaults to <NAME_extracted>, or the project directory if one of specified
--translation-hint-file=<FILE> Hint file to recover translation keys
--old-translation-csv=<FILE>   Old translation csv file to sort keys and translation keys (can be repeated)
"""

var REPLACE_TRANSLATION_NOTES = """Replace Translations Options:
--translation-csv=<SRC_FILE>=<DEST_FILE>    The csv file to replace/add the translation (e.g. "/path/to/file.csv=res://file.csv") (can be repeated)
--patch-file=<SRC_FILE>=<DEST_FILE>      	The file to patch the PCK with (e.g. "/path/to/file.ttf=res://file.ttf") (can be repeated)
"""

func print_usage():
	print("Godot Engine Translation Tool")
	print("")
	print("\nGeneral options:")
	print("  -h, --help: Display this help message")
	print("\nTranslation options:")
	print("Usage: gdtr-tools.exe --headless <main_command> [options]")
	print(MAIN_CMD_NOTES)
	print(EXTRACT_TRANSLATION_NOTES)
	print(REPLACE_TRANSLATION_NOTES)

func get_cli_abs_path(path:String) -> String:
	path = path.simplify_path()
	if path.is_absolute_path():
		return path
	var exec_path = GDRESettings.get_exec_dir()
	if path.begins_with('~/'):
		path = GDRESettings.get_home_dir() + path.trim_prefix('~')
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
				output_dir:String):
	var _new_files = []
	for file in input_files:
		file = get_cli_abs_path(file)
		var _files = get_glob_files(file)
		if _files.size() > 0:
			_new_files.append_array(_files)
		else:
			print_usage()
			print("Error: failed to locate " + file)
			return 3
	print("Input files: ", str(_new_files))
	input_files = _new_files
	var input_file = input_files[0]
	var da:DirAccess
	var is_dir:bool = false
	var err: int = OK
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
		return 3
	#directory
	if da.dir_exists(input_file):
		if input_files.size() > 1:
			print_usage()
			print("Error: cannot specify multiple directories")
			return 3
		if input_file.get_extension().to_lower() == "app":
			is_dir = false
		elif !da.dir_exists(input_file.path_join(".import")) && !da.dir_exists(input_file.path_join(".godot")):
			print_usage()
			print("Error: " + input_file + " does not appear to be a project directory")
			return 3
		else:
			is_dir = true
	#PCK/APK
	elif not da.file_exists(input_file):
		print_usage()
		print("Error: failed to locate " + input_file)
		return 3

	GDRESettings.open_log_file(output_dir)

	err = GDRESettings.load_project(input_files, false)
	if (err != OK):
		print_usage()
		print("Error: failed to open ", (input_files))
		return err

	print("Successfully loaded PCK!")
	var version:String = GDRESettings.get_version_string()
	print("Version: " + version)

	var files: PackedStringArray = []
	# remove all the non ".translation" files
	for file in GDRESettings.get_file_list():
		if (file.get_extension().to_lower() == "translation"):
			files.append(file)
	print("Translation only mode, only extracting translation files")

	if output_dir != input_file and not is_dir:
		if (da.file_exists(output_dir)):
			print("Error: output dir appears to be a file, not extracting...")
			return 3
	if is_dir:
		if output_dir.simplify_path() != input_file.simplify_path() and GDRECommon.copy_dir(input_file, output_dir) != OK:
			print("Error: failed to copy " + input_file + " to " + output_dir)
			return 3
	else:
		err = dump_files(output_dir, files, false)
		if (err != OK):
			print("Error: failed to extract PAK file, not exporting assets")
			return 3
	var end_time;
	var secs_taken;
	export_imports(output_dir, files)
	end_time = Time.get_ticks_msec()
	secs_taken = (end_time - start_time) / 1000
	print("Recovery complete in %02dm%02ds" % [(secs_taken) / 60, (secs_taken) % 60])
	return OK


func load_pck(input_files: PackedStringArray, extract_only: bool):
	var _new_files = []
	for file in input_files:
		file = get_cli_abs_path(file)
		var _files = get_glob_files(file)
		if _files.size() > 0:
			_new_files.append_array(_files)
		else:
			print_usage()
			print("Error: failed to locate " + file)
			return []
	print("Input files: ", str(_new_files))
	input_files = _new_files
	var input_file = input_files[0]
	var da:DirAccess
	var err: int = OK
	da = DirAccess.open(input_file.get_base_dir())

	# check if da works
	if da == null:
		print_usage()
		print("Error: failed to locate parent dir for " + input_file)
		return []
	#directory
	if da.dir_exists(input_file):
		if input_files.size() > 1:
			print_usage()
			print("Error: cannot specify multiple directories")
			return []
		if input_file.get_extension().to_lower() == "app":
			pass
		elif !da.dir_exists(input_file.path_join(".import")) && !da.dir_exists(input_file.path_join(".godot")):
			print_usage()
			print("Error: " + input_file + " does not appear to be a project directory")
			return []
	#PCK/APK
	elif not da.file_exists(input_file):
		print_usage()
		print("Error: failed to locate " + input_file)
		return []

	err = GDRESettings.load_project(input_files, extract_only)
	if (err != OK):
		print_usage()
		print("Error: failed to open ", (input_files))
		return []

	var files: PackedStringArray = []
	files = GDRESettings.get_file_list()

	return files



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

func get_sanitized_args():
	var args = OS.get_cmdline_args()
	#var scene_path = get_tree().root.scene_file_path
	#var scene_path = "res://gdtr_main.tscn"

	if args.size() > 0 and args[0] == "--scene":
		args = args.slice(1)
	if args.size() > 0 and args[0].begins_with("res://gdre") and args[0].ends_with(".tscn"):
		args = args.slice(1)
	return args


func handle_cli(args: PackedStringArray) -> int:
	var input_file:PackedStringArray = []
	var patch_map: Dictionary[String, String] = {}
	var output_dir: String = ""
	var main_cmds = {}
	var replace_translation_pck: String = ""
	var translation_map: Dictionary[String, String] = {}
	var ret: int = OK
	if (args.size() == 0):
		print_usage()
		print("ERROR: no command specified")
		return 2
	var any_commands = false
	for i in range(args.size()):
		var arg:String = args[i]
		if arg.begins_with("--"):
			any_commands = true
			break
	if any_commands == false:
		print_usage()
		print("ERROR: no command specified")
		return 2
	for i in range(args.size()):
		var arg:String = args[i]
		if arg == "--help" || arg == "--gdre-help":
			print_version()
			print_usage()
			return 0
		elif arg.begins_with("--version") || arg.begins_with("--gdre-version"):
			print_version()
			return 0
		elif arg.begins_with("--extract-translation"):
			input_file.append(get_arg_value(arg).simplify_path())
			main_cmds["extract-translation"] = true
		elif arg.begins_with("--replace-translation"):
			replace_translation_pck = get_cli_abs_path(get_arg_value(arg))
			main_cmds["replace-translation"] = true
		elif arg.begins_with("--translation-csv"):
			var parsed_arg = get_arg_value(arg)
			var translation_csvs = parsed_arg.split("=", false, 2)
			if translation_csvs.size() != 2:
				print_usage()
				print("ERROR: invalid --translation-csv format: must be <src_file>=<dest_file>")
				print(arg)
				print(parsed_arg)
				print(translation_csvs)
				return 2
			translation_map[get_cli_abs_path(dequote(translation_csvs[0]).strip_edges())] = dequote(translation_csvs[1]).strip_edges()
		elif arg.begins_with("--output") or arg.begins_with("--output-dir"):
			output_dir = get_arg_value(arg).simplify_path()
		elif arg.begins_with("--translation-hint-file"):
			var fpath = get_arg_value(arg).simplify_path()
			if not FileAccess.file_exists(fpath):
				print_usage()
				print("ERROR: translation hint file does not exist: " + fpath)
				return 2
			GDRESettings.set_translation_hint_file_path(fpath)
		elif arg.begins_with("--old-translation-csv"):
			var fpath = get_arg_value(arg).simplify_path()
			if not FileAccess.file_exists(fpath):
				print_usage()
				print("ERROR: old translation csv does not exist: " + fpath)
				return 2
			GDRESettings.add_old_translation_csv_path(fpath)
		elif arg.begins_with("--patch-file"):
			var parsed_arg = get_arg_value(arg)
			var patch_files = parsed_arg.split("=", false, 2)
			if patch_files.size() != 2:
				print_usage()
				print("ERROR: invalid --patch-file format: must be <src_file>=<dest_file>")
				print(arg)
				print(parsed_arg)
				print(patch_files)
				return 2
			patch_map[get_cli_abs_path(dequote(patch_files[0]).strip_edges())] = dequote(patch_files[1]).strip_edges()
		else:
			print_usage()
			print("ERROR: invalid option '" + arg + "'")
			print("Args: " + str(args))
			return 2
		if last_error != "":
			print_usage()
			print(last_error)
			return 2
	if main_cmds.size() > 1:
		print_usage()
		print("ERROR: invalid option! Must specify only one of " + ", ".join(MAIN_COMMANDS))
		return 2
	elif not input_file.is_empty():
		ret = recovery(input_file, output_dir)
		GDRESettings.unload_project()
		close_log()
	elif not replace_translation_pck.is_empty():
		var out_temp_da = DirAccess.create_temp("gdtr")
		var out_temp_path = out_temp_da.get_current_dir().simplify_path()
		ret = recovery([replace_translation_pck], out_temp_path)
		GDRESettings.unload_project()
		close_log()
		if ret != OK:
			return ret

		var config = ProjectConfigLoader.new()
		var ver_major = GDRESettings.get_ver_major()
		var project_path = out_temp_path.path_join("project.godot")
		if config.load_cfb(project_path, ver_major, 0) != OK:
			printerr("Error: failed to load project.godot from " + out_temp_path)
			return 2

		var copied_translation_map:Dictionary[String, String] = {}
		for key in translation_map.keys():
			var dest_temp = out_temp_path.path_join(translation_map[key].trim_prefix("res://")).simplify_path()
			var dest_temp_dir = dest_temp.get_base_dir()
			if GDRECommon.ensure_dir(dest_temp_dir) != OK:
				printerr("Error: failed to create directory " + dest_temp_dir)
				return 2
			if DirAccess.copy_absolute(key, dest_temp) != OK:
				printerr("Error: failed to copy " + key + " to " + key)
				return 2
			copied_translation_map[dest_temp] = translation_map[key]

		var add_patch_map = convert_translation_files(config, copied_translation_map)

		var out_project_path = out_temp_path.path_join("project.binary")
		if config.save_custom_binary(out_project_path, ver_major, 0) != OK:
			printerr("Error: failed to save project.binary to " + out_project_path)
			return 2
		add_patch_map[out_project_path] = "res://project.binary"

		patch_map.merge(add_patch_map)

		ret = patch_pck(replace_translation_pck, output_dir, patch_map)
		GDRESettings.unload_project()
	else:
		print_usage()
		print("ERROR: invalid option! Must specify one of " + ", ".join(MAIN_COMMANDS))
		return 1
	return ret

func convert_translation_files(config: ProjectConfigLoader, translation_map: Dictionary):
	var add_patch_map:Dictionary[String, String] = {}

	var translations: PackedStringArray = config.get_setting("internationalization/locale/translations", [])

	for key in translation_map.keys():
		var dest_dir = translation_map[key].get_base_dir()
		var gen_file_list = TranslationConverter.convert_translation_file(key)
		for fpath in gen_file_list:
			var dest_path = dest_dir.path_join(fpath.get_file())
			add_patch_map[fpath] = dest_path
			if not translations.has(dest_path):
				translations.append(dest_path)
				print("Adding translation file: " + dest_path)
		print("Convert translation file: " + key)

	config.set_setting("internationalization/locale/translations", translations)

	return add_patch_map

func _start_patch_pck(dest_pck: String, pack_info: PackInfo, embed_pck: String = ""):
	var engine_version: GodotVer = pack_info.get_version()
	var encrypted = pack_info.is_encrypted()
	var embed = false
	if not embed_pck.is_empty():
		embed = true
	var pck_creator = PckCreator.new()
	pck_creator.start_pck(dest_pck,
							pack_info.get_fmt_version(),
							engine_version.major,
							engine_version.minor,
							engine_version.patch,
							encrypted,
							embed,
							embed_pck)
	return pck_creator

func patch_pck(src_file: String, dest_pck:String, patch_file_map: Dictionary, embed_pck: String = "") -> int:
	if (src_file.is_empty()):
		print_usage()
		print("Error: --pck-patch is required")
		return 4
	if (dest_pck.is_empty()):
		print_usage()
		print("Error: --output is required")
		return 4
	src_file = get_cli_abs_path(src_file)
	if (not FileAccess.file_exists(src_file)):
		print("Error: PCK file '" + src_file + "' does not exist")
		return 4
	var existing_pck_files = load_pck([src_file], true)
	if (existing_pck_files.size() == 0):
		print("Error: failed to load PCK file")
		return 4
	var pack_infos = GDRESettings.get_pack_info_list()
	if (pack_infos.is_empty()):
		print("Error: no PCK existing_pck_files loaded")
		return 4
	if (pack_infos.size() > 1):
		print("Error: multiple PCK existing_pck_files loaded, specify which one to patch")
		return 4

	if (pack_infos[0].get_type() != 0 and pack_infos[0].get_type() != 4):
		print("Error: file is not a PCK or EXE")
		return 4

	var reverse_map:Dictionary[String, String] = {}
	for key in patch_file_map.keys():
		reverse_map[patch_file_map[key]] = key
	for pck_file in existing_pck_files:
		if (reverse_map.has(pck_file) or reverse_map.has(pck_file.trim_prefix("res://"))):
			continue
		if (pck_file.is_relative_path()):
			pck_file = "res://" + pck_file
		patch_file_map[pck_file] = pck_file
	var pck_patcher = _start_patch_pck(dest_pck, pack_infos[0], embed_pck)
	var err = pck_patcher.add_files(patch_file_map)
	if (err != OK):
		print("Error: failed to add files to patch PCK: " + pck_patcher.get_error_message())
		return 4
	err = pck_patcher.finish_pck()
	GDRESettings.unload_project()
	if err == ERR_PRINTER_ON_FIRE: # rename file
		var tmp_path = pck_patcher.get_error_message()
		err = DirAccess.remove_absolute(dest_pck)
		if (err != OK):
			print("Error: failed to remove existing PCK: " + pck_patcher.get_error_message())
			return 4
		err = DirAccess.rename_absolute(tmp_path, dest_pck)

	if (err != OK):
		print("Error: failed to write patching PCK:" + pck_patcher.get_error_message())
		return 4
	print("Patched PCK file: " + dest_pck)
	return OK
