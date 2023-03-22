extends Control

var ver_major = 0
var ver_minor = 0
var main : GDRECLIMain

var input_box_js_interface = null
var input_id = null
var last_rect : Rect2
var is_clickable : bool = false



var jscode = """
var _HTML5FileExchange = {};
_HTML5FileExchange.make_input_box = function(x, y, width, height, remove_self_on_upload = true) {
	var input = document.createElement('INPUT', {type: 'file', accept: 'application/pck,application/exe,application/apk,application/zip'});
	var input_id = 'UPLOAD_INPUT'; // TODO: make unique
	input.id = input_id;
	input.type = 'file';
	input.className = 'manual-file-chooser';
	input.ariaLabel = 'Choose a file';
	input.style.position = 'absolute';
	input.style.visibility = 'visible';
	input.style.display = 'block';
	input.style.top = y + 'px';
	input.style.left = x + 'px';
	input.style.width = width + 'px';
	input.style.height = height + 'px';
	input.style.zIndex = 1000;
	input.style.cursor = 'pointer';
	input.style.opacity = 0.0001;
	input.addEventListener('change', event => {
		if (event.target.files.length > 0){
			canceled = false;
		} else {
			canceled = true;
			return;
		}
		var file = event.target.files[0];
		var reader = new FileReader();
		this.fileType = file.type;
		var dttransfer = new DataTransfer();
		var item = dttransfer.items.add(file);
		item.__proto__.webkitGetAsEntry = () =>{
			return {
				isFile: true,
				file: (callback) => {
					callback(item.getAsFile());
				}
			};
		}
		dttransfer.items[0] = item;
		var drop_event = new DragEvent('drop', {dataTransfer: dttransfer, isTrusted: true});
		drop_event.isTrusted = true;
		document.getElementById('canvas').dispatchEvent(drop_event)
		if (remove_self_on_upload) {
			document.getElementById(input_id).remove();
		}
		});
	document.body.appendChild(input);
	return input_id;
}
_HTML5FileExchange.resize = function(input_id, x, y, width, height){
	var id = document.getElementById(input_id);
	if (id){
		id.style.top = y + 'px';
		id.style.left = x + 'px';
		id.style.width = width + 'px';
		id.style.height = height + 'px';
	}
}
_HTML5FileExchange.isPresent = function(inputId){
	return document.getElementById(inputId) != null;
}
_HTML5FileExchange.destroy = function(input_id){
	var id = document.getElementById(input_id);
	if (id){
		document.getElementById(input_id).remove();
	}
}
"""


func test_text_to_bin(txt_to_bin: String, output_dir: String):
	var importer:ImportExporter = ImportExporter.new()
	var dst_file = txt_to_bin.get_file().replace(".tscn", ".scn").replace(".tres", ".res")
	importer.convert_res_txt_2_bin(output_dir, txt_to_bin, dst_file)
	importer.convert_res_bin_2_txt(output_dir, output_dir.path_join(dst_file), dst_file.replace(".scn", ".tscn").replace(".res", ".tres"))

func _on_re_editor_standalone_write_log_message(message):
	$log_window.text += message
	$log_window.scroll_to_line($log_window.get_line_count() - 1)

func _on_version_lbl_pressed():
	OS.shell_open("https://github.com/bruvzg/gdsdecomp")

func _set_drag_drop_icon_visible(visible: bool):
	if OS.get_name() == "Web":
		$drag_drop_icon.visible = visible

func _setup_input_box_js_interface():
	if OS.get_name() == "Web":
		Engine.get_singleton("JavaScriptBridge").eval(jscode, true)
		input_box_js_interface = Engine.get_singleton("JavaScriptBridge").get_interface("_HTML5FileExchange")

func _set_drag_drop_icon_clickable(clickable: bool):
	if OS.get_name() == "Web":
		if clickable:
			var rect = $drag_drop_icon.get_rect()
			if input_id != null:
				input_box_js_interface.destroy(input_id)
			input_id = input_box_js_interface.make_input_box(rect.position.x, rect.position.y, rect.size.x, rect.size.y)
			last_rect = rect
			is_clickable = true
		else:
			if input_id != null:
				input_box_js_interface.destroy(input_id)
				input_id = null
			last_rect = Rect2()
			is_clickable = false

func _update_drag_drop_icon():
	if OS.get_name() == "Web":
		if input_id != null:
			if is_clickable:
				# check to see if the input box is still there
				# if not, that means it's destroyed itself upon upload, clear out the input_id and is_clickable
				# TODO: possibly emit event
				# [connection signal="input_box_destroyed" from="." to="re_editor_standalone" method="_on_input_box_destroyed"]
				if not input_box_js_interface.isPresent(input_id):
					input_id = null
					last_rect = Rect2()
					is_clickable = false
					return
				# check to see if the rect has changed
				var rect = $drag_drop_icon.get_rect()
				if rect != last_rect:
					input_id = input_box_js_interface.resize(input_id, rect.position.x, rect.position.y, rect.size.x, rect.size.y)
					last_rect = rect
					return
			else:
				input_box_js_interface.destroy(input_id)
			



func _process(delta):
	if OS.get_name() == "Web":
		_update_drag_drop_icon()

func _ready():
	$version_lbl.text = $re_editor_standalone.get_version()
	if OS.get_name() == "Web":
		_set_drag_drop_icon_visible(true)
		_setup_input_box_js_interface()
	# If CLI arguments were passed in, just quit
	if handle_cli():
		get_tree().quit()
	
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

func export_imports(output_dir:String):
	var importer:ImportExporter = ImportExporter.new()
	importer.export_imports(output_dir)
	importer.reset()

func dump_files(output_dir:String) -> int:
	var err:int = OK;
	var pckdump = PckDumper.new()
	if err == OK:
		err = pckdump.check_md5_all_files()
		if err != OK and err != ERR_SKIP:
			print("Error md5")
			return err
		err = pckdump.pck_dump_to_dir(output_dir)
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

func recovery(input_file:String, output_dir:String, enc_key:String, extract_only: bool):
	var da:DirAccess
	var is_dir:bool = false
	var err: int = OK
	input_file = main.get_cli_abs_path(input_file)
	if output_dir == "":
		output_dir = input_file.get_basename()
		if output_dir.get_extension():
			output_dir += "_recovery"
	else:
		output_dir = main.get_cli_abs_path(output_dir)

	da = DirAccess.open(input_file.get_base_dir())

	#directory
	if da.dir_exists(input_file):
		if !da.dir_exists(input_file.path_join(".import")):
			print("Error: " + input_file + " does not appear to be a project directory")
			return
		else:
			is_dir = true
	#PCK/APK
	elif not da.file_exists(input_file):
		print("Error: failed to locate " + input_file)
		return

	main.open_log(output_dir)
	if (enc_key != ""):
		err = main.set_key(enc_key)
		if (err != OK):
			print("Error: failed to set key!")
			return
	
	err = main.load_pack(input_file)
	if (err != OK):
		print("Error: failed to open " + input_file)
		return

	print("Successfully loaded PCK!") 
	ver_major = main.get_engine_version().split(".")[0].to_int()
	ver_minor = main.get_engine_version().split(".")[1].to_int()
	var version:String = main.get_engine_version()
	print("Version: " + version)

	if output_dir != input_file and not is_dir: 
		if (da.file_exists(output_dir)):
			print("Error: output dir appears to be a file, not extracting...")
			return
	if is_dir:
		if extract_only:
			print("Why did you open a folder to extract it??? What's wrong with you?!!?")
			return
		if main.copy_dir(input_file, output_dir) != OK:
			print("Error: failed to copy " + input_file + " to " + output_dir)
			return
	else:
		err = dump_files(output_dir)
		if (err != OK):
			print("Error: failed to extract PAK file, not exporting assets")
			return
	if (extract_only):
		return
	export_imports(output_dir)

func print_version():
	print("Godot RE Tools " + main.get_gdre_version())

func handle_cli() -> bool:
	var args = OS.get_cmdline_args()
	var input_extract_file:String = ""
	var input_file:String = ""
	var output_dir: String = ""
	var enc_key: String = ""
	var txt_to_bin: String = ""
	if (args.size() == 0 or (args.size() == 1 and args[0] == "res://gdre_main.tscn")):
		return false
	main = GDRECLIMain.new()
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
	if input_file != "":
		recovery(input_file, output_dir, enc_key, false)
		main.clear_data()
		main.close_log()
		get_tree().quit()
	elif input_extract_file != "":
		recovery(input_extract_file, output_dir, enc_key, true)
		main.clear_data()
		main.close_log()
		get_tree().quit()
	elif txt_to_bin != "":
		txt_to_bin = main.get_cli_abs_path(txt_to_bin)
		output_dir = main.get_cli_abs_path(output_dir)
		test_text_to_bin(txt_to_bin, output_dir)
		get_tree().quit()
	else:
		print("ERROR: invalid option")

		print_usage()
		get_tree().quit()
	return true
