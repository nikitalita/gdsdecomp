class_name GDREResourcePreview
extends Control

var code_highlighter: CodeHighlighter = preload("res://gdre_code_highlighter.tres")
var resource_highlighter: CodeHighlighter = preload("res://gdre_gdresource_highlighter.tres")

var CODE_VIWER_OPTIONS: MenuButton = null
var CODE_VIWER_OPTIONS_POPUP: PopupMenu = null
var CODE_VIEWER: CodeEdit = null
var TEXT_VIEWER: TextEdit = null
var TEXT_VIEW: Control = null
var TEXT_RESOURCE_VIEWER: TextEdit = null
var AUDIO_PLAYER: Control = null
var TEXTURE_RECT: TextureRect = null
var TEXTURE_VIEW: Control = null
var TEXTURE_INFO: Label = null
var PROGRESS_BAR: Slider = null
var AUDIO_PLAYER_TIME_LABEL: Label = null
var AUDIO_PLAYER_STREAM: AudioStreamPlayer = null
var RESOURCE_INFO: RichTextLabel = null
var VIEWER: Control = null
const RESOURCE_INFO_TEXT_FORMAT = "[b]Path:[/b] %s\n[b]Type:[/b] %s\n[b]Format:[/b] %s"
const IMAGE_FORMAT_NAME = [
	"Lum8",
	"LumAlpha8",
	"Red8",
	"RedGreen",
	"RGB8",
	"RGBA8",
	"RGBA4444",
	"RGBA5551", # Actually RGB565, kept as RGBA5551 for compatibility.
	"RFloat",
	"RGFloat",
	"RGBFloat",
	"RGBAFloat",
	"RHalf",
	"RGHalf",
	"RGBHalf",
	"RGBAHalf",
	"RGBE9995",
	"DXT1 RGB8",
	"DXT3 RGBA8",
	"DXT5 RGBA8",
	"RGTC Red8",
	"RGTC RedGreen8",
	"BPTC_RGBA",
	"BPTC_RGBF",
	"BPTC_RGBFU",
	"ETC",
	"ETC2_R11",
	"ETC2_R11S",
	"ETC2_RG11",
	"ETC2_RG11S",
	"ETC2_RGB8",
	"ETC2_RGBA8",
	"ETC2_RGB8A1",
	"ETC2_RA_AS_RG",
	"FORMAT_DXT5_RA_AS_RG",
	"ASTC_4x4",
	"ASTC_4x4_HDR",
	"ASTC_8x8",
	"ASTC_8x8_HDR",
]
var dragging_slider: bool = false
var last_updated_time: float = 0
var last_seek_pos: float = -1
var was_playing: bool = false

func reset():
	CODE_VIEWER.visible = false
	CODE_VIEWER.text = ""
	AUDIO_PLAYER.visible = false
	AUDIO_PLAYER_STREAM.stop()
	AUDIO_PLAYER_STREAM.stream = null
	TEXT_VIEW.visible = false
	TEXTURE_VIEW.visible = false
	TEXTURE_INFO.text = ""
	TEXTURE_RECT.texture = null
	TEXT_VIEWER.visible = false
	TEXT_VIEWER.text = ""
	TEXT_RESOURCE_VIEWER.visible = false
	TEXT_RESOURCE_VIEWER.text = ""
	PROGRESS_BAR.value = 0
	PROGRESS_BAR.max_value = 0
	RESOURCE_INFO.text = ""
	last_updated_time = 0
	last_updated_time = -1
	dragging_slider = false
	was_playing = false

func load_code(path):
	var code_text = ""
	if path.get_extension().to_lower() == "gd":
		code_text = FileAccess.get_file_as_string(path)
	else:
		var code: FakeGDScript = ResourceCompatLoader.non_global_load(path)
		if (code == null):
			return false
		code_text = code.get_source_code()
	TEXT_VIEW.visible = true
	CODE_VIEWER.text = code_text
	CODE_VIEWER.visible = true
	_reset_code_options_menu(CODE_VIEWER.wrap_mode == TextEdit.LINE_WRAPPING_BOUNDARY)
	return true

func load_text_resource(path):
	TEXT_VIEW.visible = true
	TEXT_RESOURCE_VIEWER.text = FileAccess.get_file_as_string(path)
	TEXT_RESOURCE_VIEWER.visible = true
	_reset_code_options_menu(TEXT_RESOURCE_VIEWER.wrap_mode == TextEdit.LINE_WRAPPING_BOUNDARY)
	return true

func load_text(path):
	TEXT_VIEW.visible = true
	TEXT_VIEWER.text = FileAccess.get_file_as_string(path)
	TEXT_VIEWER.visible = true
	_reset_code_options_menu(TEXT_VIEWER.wrap_mode == TextEdit.LINE_WRAPPING_BOUNDARY)
	return true

func load_text_string(text):
	TEXT_VIEW.visible = true
	TEXT_VIEWER.text = text
	TEXT_VIEWER.visible = true
	_reset_code_options_menu(TEXT_VIEWER.wrap_mode == TextEdit.LINE_WRAPPING_BOUNDARY)
	return true


func load_sample(path):
	AUDIO_PLAYER_STREAM.stream = ResourceCompatLoader.real_load(path, "", ResourceFormatLoader.CACHE_MODE_IGNORE_DEEP)
	if (AUDIO_PLAYER_STREAM.stream == null):
		return false
	AUDIO_PLAYER.visible = true
	update_progress_bar()
	return true

func load_texture(path):
	TEXTURE_RECT.texture = ResourceCompatLoader.real_load(path, "", ResourceFormatLoader.CACHE_MODE_IGNORE_DEEP)
	if (TEXTURE_RECT.texture == null):
		return false
	TEXTURE_RECT.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	TEXTURE_RECT.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED

	# var size = TEXTURE_RECT.texture.get_size()
	# if (size.x > VIEWER.size.x || size.y > VIEWER.size.y):
	# 	TEXTURE_RECT.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	# 	TEXTURE_RECT.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	# else:
	# 	TEXTURE_RECT.expand_mode = TextureRect.EXPAND_FIT_WIDTH_PROPORTIONAL
	# 	TEXTURE_RECT.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	# if (texture->has_mipmaps()) {
	# 	const int mip_count = Image::get_image_required_mipmaps(texture->get_width(), texture->get_height(), texture->get_format());
	# 	const int memory = Image::get_image_data_size(texture->get_width(), texture->get_height(), texture->get_format(), true) * texture->get_depth();

	# 	info->set_text(vformat(String::utf8("%d×%d×%d %s\n") + TTR("%s Mipmaps") + "\n" + TTR("Memory: %s"),
	# 			texture->get_width(),
	# 			texture->get_height(),
	# 			texture->get_depth(),
	# 			format,
	# 			mip_count,
	# 			String::humanize_size(memory)));

	# } else {
	# 	const int memory = Image::get_image_data_size(texture->get_width(), texture->get_height(), texture->get_format(), false) * texture->get_depth();

	# 	info->set_text(vformat(String::utf8("%d×%d×%d %s\n") + TTR("No Mipmaps") + "\n" + TTR("Memory: %s"),
	# 			texture->get_width(),
	# 			texture->get_height(),
	# 			texture->get_depth(),
	# 			format,
	# 			String::humanize_size(memory)));
	# }
	var image = TEXTURE_RECT.texture.get_image()
	var info_text = str(TEXTURE_RECT.texture.get_width()) + "x" + str(TEXTURE_RECT.texture.get_height()) + " " + IMAGE_FORMAT_NAME[image.get_format()] 
	if image.has_mipmaps():
		
		info_text += "\n" + str(image.get_mipmap_count()) + " Mipmaps" + "\n" + "Memory: " + String.humanize_size(image.get_data_size())
	else:
		info_text += "\n" + "No Mipmaps" + "\n" + "Memory: " + String.humanize_size(image.get_data_size())

	TEXTURE_INFO.text = info_text
	TEXTURE_VIEW.visible = true
	return true

func pop_resource_info(path: String):
	if ResourceCompatLoader.handles_resource(path, ""):
		var info = ResourceCompatLoader.get_resource_info(path)
		var type = info["type"]
		var format = info["format_type"]
		RESOURCE_INFO.text = RESOURCE_INFO_TEXT_FORMAT % [path, type, format]
	else:
		RESOURCE_INFO.text = "Path: " + path

func load_resource(path: String) -> void:
	reset()
	var ext = path.get_extension().to_lower()
	var not_a_resource = false
	var error_opening = false
	var not_supported = false
	if (is_text(ext)):
		TEXT_VIEWER.text = FileAccess.get_file_as_string(path)
		TEXT_VIEWER.visible = true
		not_a_resource = true
		return
	elif (is_shader(ext)):
		not_a_resource = true
		TEXT_VIEWER.text = FileAccess.get_file_as_string(path)
		TEXT_VIEWER.visible = true
	elif (is_code(ext)):
		error_opening = not load_code(path)
	elif (is_sample(ext)):
		error_opening = not load_sample(path)
	elif (is_image(ext)):
		error_opening = not load_texture(path)
		not_a_resource = true
	elif (is_texture(ext)):
		error_opening = not load_texture(path)
	elif (is_text_resource(ext) or is_ini_like(ext)):
		load_text_resource(path)
		if is_ini_like(ext):
			not_a_resource = true
	elif is_binary_project_settings(path):
		pass
		var loader = ProjectConfigLoader.new()
		var ver_major = GDRESettings.get_ver_major()
		var text_file = "project.godot"
		if (ver_major > 0):
			loader.load_cfb(path, ver_major, 0)
		else:
			if (path.get_file() == "engine.cfb"):
				ver_major = 2
				loader.load_cfb(path, ver_major, 0)
			else:
				var err = loader.load_cfb(path, 4, 3)
				if (err == OK):
					ver_major = 4
				else:
					loader.load_cfb(path, 3, 3)
					ver_major = 3
		if ver_major == 2:
			text_file = "engine.cfg"
		var temp_path = OS.get_temp_dir().path_join(text_file)
		var config = loader.save_cfb(OS.get_temp_dir(), ver_major, 0)
		load_text_resource(temp_path)
		var da = DirAccess.open(OS.get_temp_dir())
		if da:
			da.remove(temp_path)
	elif ResourceCompatLoader.handles_resource(path, ""):
		var tmp_dir = OS.get_temp_dir()
		var temp_path = tmp_dir.path_join(path.get_file().get_basename() + "." + str(Time.get_ticks_msec()))
		if (path.get_extension().to_lower() == "scn"):
			temp_path += ".tscn"
		else:
			temp_path += ".tres"
		if ResourceCompatLoader.to_text(path, temp_path) != OK:
			error_opening = true
		else:
			load_text_resource(temp_path)
			var da = DirAccess.open(tmp_dir)
			if da:
				da.remove(temp_path)
	else:
		not_supported = true
	if (not_supported):
		load_text_string("Not a supported resource")
		RESOURCE_INFO.text = path
	elif (error_opening):
		load_text_string("Error opening resource")
		RESOURCE_INFO.text = path
	if (RESOURCE_INFO.text == ""):
		pop_resource_info(path)
		
	

	# TODO: handle binary resources
	# var res_info:Dictionary = ResourceCompatLoader.get_resource_info(path)
	# if (res_info.size() == 0):
	# 	return


	
func is_shader(ext, p_type = ""):
	if (ext == "shader" || ext == "gdshader"):
		return true
	return false

func is_code(ext, p_type = ""):
	if (ext == "gd" || ext == "gdc" || ext == "gde"):
		return true
	return false

func is_text(ext, p_type = ""):
	if (ext == "txt" || ext == "json" || ext == "xml" || ext == "csv" || ext == "html" || ext == "md" || ext == "yml" || ext == "yaml"):
		return true
	return false

func is_text_resource(ext, p_type = ""):
	return ext == "tscn" || ext == "tres"

func is_ini_like(ext, p_type = ""):
	return ext == "cfg" || ext == "remap" || ext == "import" || ext == "gdextension" || ext == "gdnative" || ext == "godot"

func is_sample(ext, p_type = ""):
	if (ext == "oggstr" || ext == "mp3str" || ext == "oggvorbisstr" || ext == "sample" || ext == "wav" || ext == "ogg" || ext == "mp3"):
		return true
	return false

func is_texture(ext, p_type = ""):
	if (ext == "ctex" || ext == "stex" || ext == "tex"):
		return true
	# return p_type == "CompressedTexture2D" || p_type == "StreamTexture" || p_type == "Texture2D" || p_type == "ImageTexture"
	return false

func is_binary_project_settings(path):
	return path.get_file() == "project.binary" || path.get_file() == "engine.cfb"

func is_image(ext, p_type = ""):
	if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "svg" || ext == "webp" || ext == "bmp" || ext == "tga" || ext == "tiff" || ext == "hdr" || ext == "ico" || ext == "icns"):
		return true
	return false




func _physics_process(delta: float) -> void:
	if not AUDIO_PLAYER_STREAM or not AUDIO_PLAYER_STREAM.stream or not AUDIO_PLAYER.visible:
		return
	if AUDIO_PLAYER_STREAM.playing:
		if not dragging_slider:
			update_progress_bar()
		else:
			last_updated_time += delta
			if last_updated_time > 0.10:
				if (abs(last_seek_pos - PROGRESS_BAR.value)) > 0.2:
					AUDIO_PLAYER_STREAM.seek(PROGRESS_BAR.value)
					last_updated_time = 0
					last_seek_pos = PROGRESS_BAR.value
			update_text_label()
	# elif AUDIO_PLAYER_STREAM.stream and was_playing:
	# 	update_progress_bar()
	# 	was_playing = false

func time_from_float(time: float) -> String:
	var minutes = int(time / 60)
	var seconds = int(time) % 60
	var microseconds = int((time - int(time)) * 1000)
	var rounded_microseconds = int(microseconds / 100)
	return str(minutes) + ":" + str(seconds).pad_zeros(2) + "." + str(rounded_microseconds)

func update_text_label():
	AUDIO_PLAYER_TIME_LABEL.text = time_from_float(PROGRESS_BAR.value) + " / " + time_from_float(PROGRESS_BAR.max_value)

func update_progress_bar():
	PROGRESS_BAR.max_value = AUDIO_PLAYER_STREAM.stream.get_length()
	PROGRESS_BAR.value = AUDIO_PLAYER_STREAM.get_playback_position()
	update_text_label()

func _on_play_pressed() -> void:
	var pos = PROGRESS_BAR.value
	if (PROGRESS_BAR.value == PROGRESS_BAR.max_value):
		pos = 0
	AUDIO_PLAYER_STREAM.play(pos)
	# was_playing = true
	update_progress_bar()

	pass # Replace with function body.


func _on_pause_pressed() -> void:
	# was_playing = false
	AUDIO_PLAYER_STREAM.stop()
	pass # Replace with function body.


func _on_slider_drag_started() -> void:
	var pos = PROGRESS_BAR.value
	AUDIO_PLAYER_STREAM.seek(pos)
	dragging_slider = true
	pass # Replace with function body.


func _on_slider_drag_ended(value_changed: bool) -> void:
	if value_changed:
		var pos = PROGRESS_BAR.value
		AUDIO_PLAYER_STREAM.seek(pos)
	dragging_slider = false
	pass # Replace with function body.


func _on_audio_stream_player_finished() -> void:
	PROGRESS_BAR.value = 0
	pass # Replace with function body.
func _on_gdre_resource_preview_visibility_changed() -> void:
	if not self.visible:
		AUDIO_PLAYER_STREAM.stop()
	pass # Replace with function body.

func _reset_code_options_menu(word_wrap_enabled: bool):
	var current_code_edit: CodeEdit = CODE_VIEWER
	if CODE_VIEWER.visible:
		current_code_edit = CODE_VIEWER
	elif TEXT_VIEWER.visible :
		current_code_edit = TEXT_VIEWER
	elif TEXT_RESOURCE_VIEWER.visible:
		current_code_edit = TEXT_RESOURCE_VIEWER
	else:
		return
	current_code_edit.wrap_mode = TextEdit.LINE_WRAPPING_NONE if not word_wrap_enabled else TextEdit.LINE_WRAPPING_BOUNDARY
	CODE_VIWER_OPTIONS_POPUP.set_item_checked(0, word_wrap_enabled)

func _on_code_viewer_options_pressed(id) -> void:
	if (id == 0): # Word-wrap
		var current_checked = not CODE_VIWER_OPTIONS_POPUP.is_item_checked(0)
		_reset_code_options_menu(current_checked)
	pass # Replace with function body.


func _ready():
	CODE_VIEWER = $VBoxContainer/ResourceView/TextView/CodeViewer
	TEXT_VIEWER = $VBoxContainer/ResourceView/TextView/TextViewer
	TEXT_RESOURCE_VIEWER = $VBoxContainer/ResourceView/TextView/TextResourceViewer
	CODE_VIWER_OPTIONS = $VBoxContainer/ResourceView/TextView/CodeOptsBox/CodeViewerOptions
	TEXT_VIEW = $VBoxContainer/ResourceView/TextView
	CODE_VIWER_OPTIONS_POPUP = CODE_VIWER_OPTIONS.get_popup()
	AUDIO_PLAYER = $VBoxContainer/ResourceView/AudioPlayer
	TEXTURE_RECT = $VBoxContainer/ResourceView/TextureView/TextureRect
	TEXTURE_VIEW = $VBoxContainer/ResourceView/TextureView
	TEXTURE_INFO = $VBoxContainer/ResourceView/TextureView/TextureInfo
	PROGRESS_BAR = $VBoxContainer/ResourceView/AudioPlayer/BarHBox/ProgressBar
	AUDIO_PLAYER_TIME_LABEL = $VBoxContainer/ResourceView/AudioPlayer/BarHBox/TimeLabel
	AUDIO_PLAYER_STREAM = $VBoxContainer/ResourceView/AudioPlayer/AudioStreamPlayer
	RESOURCE_INFO = $VBoxContainer/Control/ResourceInfo
	VIEWER = $VBoxContainer/ResourceView
	reset()
	self.connect("visibility_changed", self._on_gdre_resource_preview_visibility_changed)
	CODE_VIWER_OPTIONS_POPUP.connect("id_pressed", self._on_code_viewer_options_pressed)

	# TODO: remove me
	#load_resource("res://gdre_file_tree.gd")
	# load_resource("res://.godot/imported/gdre_Script.svg-4c68c9c5e02f5e7a41dddea59a95e245.ctex")
	#load_resource("res://.godot/imported/anomaly 105 jun12.ogg-d3e939934d210d1a4e1f9d2d34966046.oggvorbisstr")

# audio player stuff
