extends Node

const file_icon: Texture2D = preload("res://gdre_icons/gdre_File.svg")
const file_ok: Texture2D = preload("res://gdre_icons/gdre_FileOk.svg")
const file_broken: Texture2D = preload("res://gdre_icons/gdre_FileBroken.svg")

var NOTE_TREE : Tree = null
var TOTALS_TREE: Tree = null
var REPORT_WINDOW :Window = null
var POPUP_PARENT_WINDOW : Window = null
var EDITOR_MESSAGE_LABEL: RichTextLabel = null
var LOG_FILE_LABEL: RichTextLabel = null
var recovery_folder: String = ""

# var isHiDPI = DisplayServer.screen_get_dpi() >= 240
var isHiDPI = false
var root: TreeItem = null
var userroot: TreeItem = null
var num_files:int = 0
var num_broken:int = 0
var num_malformed:int = 0
var _is_test:bool = true

signal report_done()


# MUST CALL set_root_window() first!!!
# Called when the node enters the scene tree for the first time.
func _ready():
	NOTE_TREE =      $NoteTree
	REPORT_WINDOW =  $ReportWindow
	NOTE_TREE = 	 $NoteTree
	TOTALS_TREE =    $TotalsTree
	EDITOR_MESSAGE_LABEL = $EditorMessageLabel
	LOG_FILE_LABEL = $LogFileLabel
	if !_is_test:
		assert(POPUP_PARENT_WINDOW)
	else:
		POPUP_PARENT_WINDOW = get_window()

	if isHiDPI:
		# get_viewport().size *= 2.0
		# get_viewport().content_scale_factor = 2.0
		#ThemeDB.fallback_base_scale = 2.0
		REPORT_WINDOW.content_scale_factor = 2.0
		REPORT_WINDOW.size *= 2.0
	# This is a hack to get around not being able to open multiple scenes
	# unless they're attached to windows
	# The children are not already in the window for ease of GUI creation
	var children: Array[Node] = self.get_children()
	for child in children:
		# remove the child from root and add it to the window
		# check if it is the recover window
		if child.get_name() == "ReportWindow":
			continue
		self.remove_child(child)
		REPORT_WINDOW.add_child(child)
	clear()
	
	if _is_test:
		load_test()
	
	pass # Replace with function body.

func _on_click_uri(meta):
	OS.shell_open(meta)
	
func ver_to_tag(ver:GodotVer):
	var tag_str = String.num(ver.major) + "." + String.num(ver.minor)
	if (ver.patch != 0):
		tag_str += "." + String.num(ver.patch)
	if !ver.prerelease.is_empty() && ver.is_valid_semver():
		tag_str += "-" + ver.prerelease
	else:
		tag_str += "-stable"
	return tag_str

func path_to_uri(path:String):
	var uri = "file://"
	if (!path.simplify_path().begins_with("/")):
		uri += "/"
	uri += path.simplify_path()
	return uri
	
func get_url_for_tag(tag: String, is_steam_release: bool = false):
	if is_steam_release: # don't bother with the tag here
		return "https://github.com/CoaguCo-Industries/GodotSteam/releases"
	else:
		return "https://github.com/godotengine/godot-builds/releases/tag/" + tag
		
func load_test():
	const path = "/Users/nikita/Workspace/godot-test-bins/megaloot/Megaloot.exe"
	# const path = "/Users/nikita/Workspace/godot-test-bins/satryn.apk"
	# const output_dir = "/Users/nikita/Workspace/godot-test-bins/test_satyrn_extract"
	const output_dir = "/Users/nikita/Workspace/godot-test-bins/test_megaloot"
	var _log_path = "/Users/nikita/Workspace/godot-test-bins/test_satyrn_extract/gdre_export.log" 
	# convert log_path to URI
	var err = GDRESettings.load_project([path])
	assert(err == OK)
	var pckdump = PckDumper.new()
	err = pckdump.check_md5_all_files()
	GDRESettings.open_log_file(output_dir)
	err = pckdump.pck_dump_to_dir(output_dir)
	var import_exporter = ImportExporter.new()
	import_exporter.export_imports(output_dir)
	var report = import_exporter.get_report()
	add_report(report)
	show_win()

func add_ver_string(ver_string: String):
	var ver = GodotVer.parse_godotver(ver_string)
	var tag = ver_to_tag(ver)
	EDITOR_MESSAGE_LABEL.text = EDITOR_MESSAGE_LABEL.text.replace("<GODOT_VER>", "[url=" + get_url_for_tag(tag) + "]"+ ver_string + "[/url]")
	
func add_log_file(log_path: String):
	recovery_folder = log_path.get_base_dir()
	var uri = path_to_uri(log_path)
	LOG_FILE_LABEL.text = LOG_FILE_LABEL.text.replace("<LOG_FILE_URI>", "[url=" + uri + "]" + log_path + "[/url]")

# called before _ready
func set_root_window(window: Window):
	POPUP_PARENT_WINDOW = window
	_is_test = false

func show_win():
	# get the screen size
	var safe_area: Rect2i = DisplayServer.get_display_safe_area()
	var center = (safe_area.position + safe_area.size - REPORT_WINDOW.size) / 2
	REPORT_WINDOW.set_position(center)
	REPORT_WINDOW.show()

func hide_win():
	REPORT_WINDOW.hide()

func get_note_header_item_icon(_key: String) -> Texture2D:
	# TODO: add warning and info icons
	#var warning_icon = POPUP_PARENT_WINDOW.get_theme_icon("Warning", "EditorIcons")
	#var info_icon = POPUP_PARENT_WINDOW.get_theme_icon("Info", "EditorIcons")
	#if key == "unsupported_types":
		#return warning_icon
	#elif key == "translation_export_message":
		#return warning_icon
	#elif key == "failed_plugins":
		#return warning_icon
	#elif key == "godot_2_assets":
		#return info_icon
	return null

func add_report(report: ImportExporterReport) -> int:
	add_ver_string(report.get_ver())
	add_log_file(report.get_log_file_location())
	var notes = report.get_session_notes()
	var report_sections = report.get_report_sections()
	# iterate over all the keys in the notes
	# add fake root
	var note_root = NOTE_TREE.create_item(null)
	var report_root = TOTALS_TREE.create_item(null)
	for key in notes.keys():
		var note_dict = notes[key]
		var header_item = NOTE_TREE.create_item(note_root)
		header_item.set_text(0, note_dict["title"])
		var message_item = NOTE_TREE.create_item(header_item)
		message_item.set_text(0, note_dict["message"])
		if note_dict.has("details"):
			for item in note_dict["details"]:
				var subitem = NOTE_TREE.create_item(header_item)
				subitem.set_text(0, item)
	for key in report_sections.keys():
		var section:Variant = report_sections[key]
		var header_item = TOTALS_TREE.create_item(report_root)
		header_item.set_text(0, key)
		# check that section is actually a dictionary
		if typeof(section) == TYPE_DICTIONARY:
			var dict :Dictionary = section
			header_item.set_text(1, String.num(dict.keys().size()))
			# iterate over all the keys in the section
			for subkey in dict.keys():
				var subitem = TOTALS_TREE.create_item(header_item)
				subitem.set_text(1, subkey)
				subitem.set_text(0, dict[subkey])
		elif typeof(section) == TYPE_PACKED_STRING_ARRAY:
			var arr: PackedStringArray = section
			header_item.set_text(1, String.num(arr.size()))
			for i in range(arr.size()):
				var subitem = TOTALS_TREE.create_item(header_item)
				subitem.set_text(0, arr[i])
				subitem.set_text(1, "")
		else:
			print("Unknown type for section: " + String.num(typeof(section)))
			continue
		header_item.set_collapsed_recursive(true)

			
	return OK

	
func clear():
	pass
	
func close():
	_exit_tree()
	emit_signal("report_done")

func cancel_extract():
	close()
	
func _open_folder():
	OS.shell_open(path_to_uri(recovery_folder))

func _ok_pressed():
	close()

func _close_requested():
	close()

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(_delta):
	pass

func _exit_tree():
	hide_win()
