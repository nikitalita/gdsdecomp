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

var TEXTURE_RECT: TextureRect = null
var TEXTURE_VIEW: Control = null
var TEXTURE_INFO: Label = null
var RESOURCE_INFO: RichTextLabel = null
var VIEWER: Control = null

var MEDIA_PLAYER: Control = null



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

func reset():
	CODE_VIEWER.visible = false
	CODE_VIEWER.text = ""
	MEDIA_PLAYER.visible = false
	MEDIA_PLAYER.reset()
	TEXT_VIEW.visible = false
	TEXTURE_VIEW.visible = false
	TEXTURE_INFO.text = ""
	TEXTURE_RECT.texture = null
	TEXT_VIEWER.visible = false
	TEXT_VIEWER.text = ""
	TEXT_RESOURCE_VIEWER.visible = false
	TEXT_RESOURCE_VIEWER.text = ""
	RESOURCE_INFO.text = ""

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
		error_opening = not MEDIA_PLAYER.load_sample(path)
		if not error_opening:
			MEDIA_PLAYER.visible = true
	elif (is_video(ext)):
		not_a_resource = true
		error_opening = not MEDIA_PLAYER.load_video(path)
		if not error_opening:
			MEDIA_PLAYER.visible = true
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

func is_video(ext, p_type = ""):
	if (ext == "webm" || ext == "ogv" || ext == "mp4" || ext == "avi" || ext == "mov" || ext == "flv" || ext == "mkv" || ext == "wmv" || ext == "mpg" || ext == "mpeg"):
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


func _reset_code_options_menu(word_wrap_enabled: bool):
	var current_code_edit: CodeEdit = CODE_VIEWER
	if CODE_VIEWER.visible:
		current_code_edit = CODE_VIEWER
	elif TEXT_VIEWER.visible:
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

func _on_gdre_resource_preview_visibility_changed() -> void:
	if not self.visible:
		MEDIA_PLAYER.stop()
	pass # Replace with function body.


func _ready():
	CODE_VIEWER = $VBoxContainer/ResourceView/TextView/CodeViewer
	TEXT_VIEWER = $VBoxContainer/ResourceView/TextView/TextViewer
	TEXT_RESOURCE_VIEWER = $VBoxContainer/ResourceView/TextView/TextResourceViewer
	CODE_VIWER_OPTIONS = $VBoxContainer/ResourceView/TextView/CodeOptsBox/CodeViewerOptions
	TEXT_VIEW = $VBoxContainer/ResourceView/TextView
	CODE_VIWER_OPTIONS_POPUP = CODE_VIWER_OPTIONS.get_popup()
	MEDIA_PLAYER = $VBoxContainer/ResourceView/MediaPlayer
	TEXTURE_RECT = $VBoxContainer/ResourceView/TextureView/TextureRect
	TEXTURE_VIEW = $VBoxContainer/ResourceView/TextureView
	TEXTURE_INFO = $VBoxContainer/ResourceView/TextureView/TextureInfo
	RESOURCE_INFO = $VBoxContainer/Control/ResourceInfo
	VIEWER = $VBoxContainer/ResourceView
	# reset()
	self.connect("visibility_changed", self._on_gdre_resource_preview_visibility_changed)
	CODE_VIWER_OPTIONS_POPUP.connect("id_pressed", self._on_code_viewer_options_pressed)

	# TODO: remove me
	#load_resource("res://gdre_file_tree.gd")
	# load_resource("res://.godot/imported/gdre_Script.svg-4c68c9c5e02f5e7a41dddea59a95e245.ctex")
	#load_resource("res://.godot/imported/anomaly 105 jun12.ogg-d3e939934d210d1a4e1f9d2d34966046.oggvorbisstr")

# audio player stuff
