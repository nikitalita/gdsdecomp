class_name GDREMediaPlayer
extends Control

class GDREAudioPreviewBox extends ColorRect:
	@export var editable: bool = true
	@export var backgroundColor: Color = Color("21262d")
	@export var lineColor: Color = Color("ffffff")
	@export var autoSetIndicatorBGColor: bool = true:
		set(val):
			if val:
				indicatorBGColor = _get_indicator_color(backgroundColor)
			autoSetIndicatorBGColor = val
		get:
			return autoSetIndicatorBGColor
	@export var indicatorLineColor: Color = Color("c8c9cb")
	@export var indicatorBGColor: Color = Color("909396bf"):
		set(val):
			if not autoSetIndicatorBGColor:
				indicatorBGColor = val
			else:
				indicatorBGColor = _get_indicator_color(backgroundColor)
		get:
			return indicatorBGColor

	var preview: GDREAudioStreamPreview = null
	var stream: AudioStream = null
	var pos: float = 0
	var dragging: bool = false

	signal pos_changed(pos: float)

	func _get_indicator_color(p_color):
		return p_color.lerp(Color(1, 1, 1, 0.5), 0.5)

	func _on_input(input: InputEvent):
		if not editable:
			return
		var click = false
		if input is InputEventMouseButton:
			if input.button_index == MOUSE_BUTTON_LEFT:
				if input.pressed:
					click = true
					dragging = true
				else:
					dragging = false
		if input is InputEventMouseMotion:
			if input.button_mask & MOUSE_BUTTON_MASK_LEFT:
				click = true
				dragging = true
			else:
				dragging = false
		if click:
			var rect = get_rect()
			var previewLen = self.preview.get_length()
			var new_pos = input.position.x / rect.size.x * previewLen
			if new_pos != self.pos:
				_set_pos(new_pos)
				emit_signal("pos_changed", pos)

	func _init():
		self.color = backgroundColor
		if autoSetIndicatorBGColor:
			indicatorBGColor = _get_indicator_color(backgroundColor)

	func _ready():
		GDREAudioStreamPreviewGenerator.connect("preview_updated", self._on_preview_updated)
		self.gui_input.connect(self._on_input)

	func set_stream(p_stream):
		self.stream = p_stream
		self.preview = GDREAudioStreamPreviewGenerator.generate_preview(self.stream)
		queue_redraw()

	func _on_preview_updated(stream_id):
		if is_instance_valid(self.stream) and self.stream.get_instance_id() == stream_id:
			queue_redraw()

	func reset():
		self.preview = null
		self.stream = null
		self.pos = 0
		self.dragging = false
		queue_redraw()

	func _set_pos(new_pos):
		var length = self.preview.get_length() if is_instance_valid(self.preview) else 0.0
		new_pos = clamp(new_pos, 0.0, length)
		if self.pos != new_pos:
			self.pos = new_pos
			queue_redraw()
		else:
			self.pos = new_pos

	func update_pos(new_pos):
		if not dragging:
			_set_pos(new_pos)

	func _draw():
		if not is_instance_valid(self.preview):
			return
		var rect = get_rect()
		var rectSize = rect.size
		var previewLen = self.preview.get_length()
		for i in range(0, rectSize.x):
			var ofs = i * previewLen / rectSize.x
			var ofs_n = (i+1) * previewLen / rectSize.x
			var max = self.preview.get_max(ofs, ofs_n) * 0.5 + 0.5
			var min = self.preview.get_min(ofs, ofs_n) * 0.5 + 0.5
			draw_line(Vector2(i,  min * rectSize.y),
			Vector2(i, max * rectSize.y), lineColor, 1, false)
		var indicatorPos = pos / previewLen * rectSize.x
		if indicatorPos >= 0:
			draw_rect(Rect2(0, 0, max(0,indicatorPos - 1), rectSize.y), indicatorBGColor)
			var indicatorColor = _get_indicator_color(indicatorBGColor)
			indicatorColor.a = 1
			draw_line(Vector2(indicatorPos, 0), Vector2(indicatorPos, rectSize.y), indicatorColor, 2, false)

var TIME_LABEL: Label
var PROGRESS_BAR: Slider
var PLAY_BUTTON: Button
var PAUSE_BUTTON: Button
var STOP_BUTTON: Button
var AUDIO_PLAYER_STREAM: AudioStreamPlayer
var AUDIO_PREVIEW_BOX: GDREAudioPreviewBox
var AUDIO_VIEW_BOX: Control
var AUDIO_STREAM_INFO: Label
var VIDEO_PLAYER_STREAM: VideoStreamPlayer
var VIDEO_VIEW_BOX: Control
var VIDEO_ASPECT_RATIO_CONTAINER: AspectRatioContainer

var controller: PlayerController = null
var dragging_slider: bool = false
var last_updated_time: float = 0
var last_seek_pos: float = -1
@export var play_icon: Texture = preload("res://gdre_icons/gdre_Play.svg")
@export var pause_icon: Texture = preload("res://gdre_icons/gdre_Pause.svg")
@export var stop_icon: Texture = preload("res://gdre_icons/gdre_Stop.svg")

func reset():
	if controller:
		controller.stop()
	controller = PlayerController.new()
	AUDIO_VIEW_BOX.visible = false
	AUDIO_PREVIEW_BOX.reset()
	AUDIO_PLAYER_STREAM.stream = null
	VIDEO_VIEW_BOX.visible = false
	VIDEO_PLAYER_STREAM.stream = null
	setup_progress_bar()
	last_updated_time = 0
	last_updated_time = -1
	dragging_slider = false


class PlayerController:
	enum PlayerType {
		NONE,
		AUDIO,
		VIDEO
	}
	signal finished()
	func _on_finished():
		finished.emit()

	func _init(_player = null):
		pass
	func stop():
		pass
	func pause():
		pass
	func play(_pos: float):
		pass
	func is_playing() -> bool:
		return false
	func is_paused() -> bool:
		return false
	func seek(_pos: float):
		pass
	func is_stream_loaded() -> bool:
		return false
	func get_stream_length() -> float:
		return 0
	func get_playback_position() -> float:
		return 0
	func supports_seek() -> bool:
		return false
	func get_type() -> PlayerType:
		return PlayerType.NONE
	pass

class AudioPlayerController extends PlayerController:
	var audio_player: AudioStreamPlayer = null
	func _init(p_audio_player: AudioStreamPlayer):
		audio_player = p_audio_player
		audio_player.finished.connect(_on_finished)
	func stop():
		audio_player.stop()
		seek(0)
	func pause():
		audio_player.stop()
	func play(pos: float):
		audio_player.play(pos)
	func is_playing() -> bool:
		return audio_player.playing
	func is_paused() -> bool:
		# Can't pause audio streams
		return false
	func seek(pos: float):
		audio_player.seek(pos)
	func is_stream_loaded() -> bool:
		return !(not audio_player or not audio_player.stream)
	func get_stream_length() -> float:
		if not is_stream_loaded():
			return 0
		return audio_player.stream.get_length()
	func get_playback_position() -> float:
		if not is_stream_loaded():
			return 0
		return audio_player.get_playback_position()
	func supports_seek() -> bool:
		return true
	func get_type() -> PlayerType:
		return PlayerType.AUDIO

class VideoPlayerController extends PlayerController:
	var video_player: VideoStreamPlayer = null

	func _init(p_video_player: VideoStreamPlayer):
		video_player = p_video_player
		video_player.finished.connect(self._on_finished)
		# play and then stop to queue up the first frame
		play(0)
		stop()
	func stop():
		# seek 0 first before stopping to queue up the first frame
		seek(0)
		video_player.stop()
		video_player.paused = false
	func pause():
		video_player.paused = true
	func play(pos: float):
		video_player.paused = false
		video_player.play()
		if supports_seek():
			video_player.stream_position = pos
	func is_playing() -> bool:
		return video_player.is_playing()
	func is_paused() -> bool:
		return video_player.paused
	func seek(pos: float):
		if supports_seek():
			if not is_playing():
				# play and then pause to queue up the frame at the given position
				play(pos)
				pause()
			# seeking is expensive in video streams
			elif video_player.stream_position != pos:
				video_player.stream_position = pos
	func is_stream_loaded() -> bool:
		return !(not video_player or not video_player.stream)
	func get_stream_length() -> float:
		if not is_stream_loaded():
			return 0
		return video_player.get_stream_length()
	func get_playback_position() -> float:
		if not is_stream_loaded():
			return 0
		return video_player.stream_position
	func supports_seek() -> bool:
		if not is_stream_loaded():
			return false
		if video_player.get_stream_length() == 0:
			return false
		return true
	func get_type() -> PlayerType:
		return PlayerType.VIDEO

func pause():
	controller.pause()

func stop():
	controller.stop()
	last_seek_pos = 0
	update_progress_bar()

func play():
	var pos = PROGRESS_BAR.value
	if (not controller.supports_seek() or PROGRESS_BAR.value == PROGRESS_BAR.max_value):
		pos = 0
	controller.play(pos)
	update_progress_bar()

func is_playing() -> bool:
	return controller.is_playing()

func seek(pos: float):
	if not controller.supports_seek():
		return
	var length = get_stream_length()
	if (length - pos < PROGRESS_BAR.step):
		pos = length - PROGRESS_BAR.step
	controller.seek(pos)
	last_seek_pos = pos

func is_stream_loaded() -> bool:
	return controller.is_stream_loaded()

func get_stream_length() -> float:
	return controller.get_stream_length()

func get_playback_position() -> float:
	return controller.get_playback_position()


const sample_info_box_text_format = """WAV
Sample Rate: %d Hz
Channels: %d
Format: %s
Loop Mode: %s"""

func loopmode_to_string(mode: int) -> String:
	match mode:
		AudioStreamWAV.LOOP_DISABLED:
			return "Disabled"
		AudioStreamWAV.LOOP_FORWARD:
			return "Forward"
		AudioStreamWAV.LOOP_PINGPONG:
			return "PingPong"
		AudioStreamWAV.LOOP_BACKWARD:
			return "Backward"
	return "Unknown"

func sample_format_to_string(format: int) -> String:
	match format:
		AudioStreamWAV.FORMAT_8_BITS:
			return "PCM 8-bit"
		AudioStreamWAV.FORMAT_16_BITS:
			return "PCM 16-bit"
		AudioStreamWAV.FORMAT_IMA_ADPCM:
			return "ADPCM"
		AudioStreamWAV.FORMAT_QOA:
			return "Quite OK"
	return "Unknown"

func is_supported_video_format(path) -> bool:
	var ext = path.get_extension().to_lower()
	return ext == "ogv" || ext == "ogm"

func is_video(path) -> bool:
	var ext = path.get_extension().to_lower()
	return ext == "ogv" or ext == "mp4" or ext == "webm" or ext == "ogm"

func is_audio(path) -> bool:
	return !is_video(path)

func is_non_resource_smp(ext, p_type = ""):
	return (ext == "wav" || ext == "ogg" || ext == "mp3")

func load_media(path):
	if is_video(path):
		return load_video(path)
	return load_sample(path)

func load_video(path):
	if not is_supported_video_format(path):
		return false
	var video_stream: VideoStream = ResourceCompatLoader.real_load(path, "", ResourceCompatLoader.CACHE_MODE_IGNORE_DEEP)
	return load_video_stream(video_stream)


func load_video_stream(video_stream: VideoStream):
	reset()
	if (video_stream == null):
		return false
	VIDEO_VIEW_BOX.visible = true
	VIDEO_PLAYER_STREAM.stream = video_stream
	VIDEO_PLAYER_STREAM.expand = false
	var texture: Texture2D = VIDEO_PLAYER_STREAM.get_video_texture()
	var sz = texture.get_size()
	VIDEO_PLAYER_STREAM.expand = true
	VIDEO_ASPECT_RATIO_CONTAINER.ratio = sz.x / sz.y
	controller = VideoPlayerController.new(VIDEO_PLAYER_STREAM)
	setup_progress_bar()
	controller.finished.connect(self.update_progress_bar)
	return true


func load_sample(path):
	var audio_stream: AudioStream = null
	var ext = path.get_extension().to_lower()
	if not is_non_resource_smp(ext):
		audio_stream = ResourceCompatLoader.real_load(path, "", ResourceCompatLoader.CACHE_MODE_IGNORE_DEEP)
	else:
		if ext == "wav":
			audio_stream = AudioStreamWAV.load_from_file(path)
		elif ext == "ogg":
			audio_stream = AudioStreamOggVorbis.load_from_file(path)
		elif ext == "mp3":
			audio_stream = AudioStreamMP3.load_from_file(path)

	return load_audio_stream(audio_stream)

func load_audio_stream(audio_stream: AudioStream):
	reset()
	if (audio_stream == null):
		return false
	AUDIO_VIEW_BOX.visible = true
	AUDIO_PLAYER_STREAM.stream = audio_stream
	controller = AudioPlayerController.new(AUDIO_PLAYER_STREAM)
	AUDIO_PREVIEW_BOX.set_stream(AUDIO_PLAYER_STREAM.stream)
	# check if it's an AudioStreamSample
	if (audio_stream.get_class() == "AudioStreamWAV"):
		var sample: AudioStreamWAV = audio_stream

		AUDIO_STREAM_INFO.text = sample_info_box_text_format % [sample.mix_rate, 2 if sample.stereo else 1, sample_format_to_string(sample.format),  loopmode_to_string(sample.loop_mode)]
		if (sample.loop_mode != AudioStreamWAV.LOOP_DISABLED):
			AUDIO_STREAM_INFO.text += "\nLoop Begin: " + str(sample.loop_begin) + "\nLoop End: " + str(sample.loop_end)
	elif (audio_stream.get_class() == "AudioStreamOggVorbis" or audio_stream.get_class() == "AudioStreamMP3"):
		var info_string = ""
		var sampling_rate: String = "unknown"
		if audio_stream.get_class() == "AudioStreamOggVorbis":
			sampling_rate = str(int(audio_stream.packet_sequence.sampling_rate))
			info_string += "Ogg Vorbis"
			info_string += "\nSample Rate: " + sampling_rate + " Hz"
		elif audio_stream.get_class() == "AudioStreamMP3":
			info_string += "MP3"
			# no sample rate information for MP3 in Godot yet
		if (audio_stream.bpm != 0):
			info_string += "\nBPM: " + str(audio_stream.bpm)
		if (audio_stream.bar_beats != 4):
			info_string += "\nBar Beats: " + str(audio_stream.bar_beats)
		if (audio_stream.beat_count != 0):
			info_string += "\nBeat Count: " + str(audio_stream.beat_count)
		if (audio_stream.loop):
			info_string += "\nLoop: Yes"
			info_string += "\nLoop Offset: " + str(audio_stream.loop_offset)
		else:
			info_string += "\nLoop: No"
		AUDIO_STREAM_INFO.text = info_string
	else:
		AUDIO_STREAM_INFO.text = ""
	setup_progress_bar()
	controller.finished.connect(self.update_progress_bar)
	return true

func time_from_float(time: float, step: float) -> String:
	var minutes = int(time / 60)
	var seconds = int(time) % 60
	var microseconds = int((time - int(time)) * 1000)
	var rounded_microseconds_zero_padding = max(0, 4 - str(int(1000 * step)).length())
	var ret = str(minutes) + ":" + str(seconds).pad_zeros(2)
	if rounded_microseconds_zero_padding > 0:
		var step_val = 1000 * step
		var rounded_microseconds = int(round(float(microseconds) / int(step_val)))
		ret += "." + str(rounded_microseconds).pad_zeros(rounded_microseconds_zero_padding)
	return ret

func setup_progress_bar():
	if (get_stream_length() < 30):
		PROGRESS_BAR.step = 0.01
	else:
		PROGRESS_BAR.step = 0.1
	PROGRESS_BAR.value = get_playback_position()
	PROGRESS_BAR.max_value = get_stream_length()
	PROGRESS_BAR.editable = controller.supports_seek()
	if not controller.supports_seek():
		PAUSE_BUTTON.disabled = true
	else:
		PAUSE_BUTTON.disabled = false
	update_text_label()

func update_progress_bar():
	if not PROGRESS_BAR.editable or not is_stream_loaded():
		return
	PROGRESS_BAR.max_value = get_stream_length()
	PROGRESS_BAR.value = get_playback_position()
	update_text_label()

func update_text_label():
	if not controller.supports_seek():
		TIME_LABEL.text = "--:-- / --:--"
	else:
		TIME_LABEL.text = time_from_float(PROGRESS_BAR.value, PROGRESS_BAR.step) + " / " + time_from_float(PROGRESS_BAR.max_value, PROGRESS_BAR.step)


func _on_slider_drag_started() -> void:
	var pos = PROGRESS_BAR.value
	seek(pos)
	last_seek_pos = pos
	dragging_slider = true
	pass # Replace with function body.


func _on_slider_drag_ended(value_changed: bool) -> void:
	if value_changed:
		var pos = PROGRESS_BAR.value
		if last_seek_pos != pos:
			seek(pos)
	dragging_slider = false


	pass # Replace with function body.


func _on_audio_stream_player_finished() -> void:
	PROGRESS_BAR.value = 0
	pass # Replace with function body.

func get_drag_delta_threshold():
	if controller.get_type() == PlayerController.PlayerType.VIDEO:
		return 0.1
	return 0.1

func get_drag_pos_threshold():
	var min_t = 0.05
	if controller.get_type() == PlayerController.PlayerType.VIDEO:
		min_t = 0.1
	return max(PROGRESS_BAR.step, min_t)

func _process(delta: float) -> void:
	if not is_stream_loaded() or not self.visible:
		return
	if controller.is_playing() or controller.is_paused():
		if not dragging_slider:
			if controller.is_playing() and not controller.is_paused():
				update_progress_bar()
		else: # dragging slider
			last_updated_time += delta
			if last_updated_time > get_drag_delta_threshold():
				if (abs(last_seek_pos - PROGRESS_BAR.value)) > get_drag_pos_threshold():
					seek(PROGRESS_BAR.value)
					last_updated_time = 0
					last_seek_pos = PROGRESS_BAR.value

func _on_progress_bar_value_changed(_value: float) -> void:
	update_text_label()
	AUDIO_PREVIEW_BOX.update_pos(PROGRESS_BAR.value)

func _on_audio_preview_box_pos_changed(value: float) -> void:
	seek(value)
	PROGRESS_BAR.max_value = AUDIO_PLAYER_STREAM.stream.get_length()
	PROGRESS_BAR.value = value
	update_text_label()

func _ready():
	# connect signals
	PROGRESS_BAR.connect("drag_started", self._on_slider_drag_started)
	PROGRESS_BAR.connect("drag_ended", self._on_slider_drag_ended)
	AUDIO_PLAYER_STREAM.connect("finished", self._on_audio_stream_player_finished)
	PLAY_BUTTON.connect("pressed", self.play)
	PAUSE_BUTTON.connect("pressed", self.pause)
	STOP_BUTTON.connect("pressed", self.stop)
	PROGRESS_BAR.connect("value_changed", self._on_progress_bar_value_changed)
	AUDIO_PREVIEW_BOX.connect("pos_changed", self._on_audio_preview_box_pos_changed)

	PLAY_BUTTON.icon = play_icon
	PAUSE_BUTTON.icon = pause_icon
	STOP_BUTTON.icon = stop_icon

	reset()
	# load_media("/Users/nikita/Workspace/godot-ws/test-decomps/_test_files/Door_OGV.ogv")
	#load_media("/Users/nikita/Desktop/_test_individual_export/gearhead.ogv")
	#load_media("/Users/nikita/Downloads/4K_resolution_sample.ogv")
	# load_media('/Users/nikita/Desktop/_test_individual_export/A moment of silent.ogg')
	# load_sample("res://anomaly 105 jun12.ogg")
	# load_sample("res://2.wav")
	# load_media("res://Door_OGV.ogv")

func _init():
	var main_margin_container: MarginContainer = MarginContainer.new()
	main_margin_container.layout_mode = 1
	main_margin_container.anchors_preset = 15
	main_margin_container.anchor_right = 1.0
	main_margin_container.anchor_bottom = 1.0
	main_margin_container.grow_horizontal = 2
	main_margin_container.grow_vertical = 2
	main_margin_container.add_theme_constant_override("margin_bottom", 20)
	self.add_child(main_margin_container)

	var vbox_container = VBoxContainer.new()
	vbox_container.layout_mode = 2
	main_margin_container.add_child(vbox_container)

	var tab_container = TabContainer.new()
	tab_container.layout_mode = 2
	tab_container.size_flags_vertical = 3
	tab_container.current_tab = 0
	tab_container.add_theme_stylebox_override("panel", StyleBoxEmpty.new())
	tab_container.tabs_visible = false
	vbox_container.add_child(tab_container)

	var default_box = Control.new()
	default_box.layout_mode = 2
	tab_container.add_child(default_box)

	AUDIO_VIEW_BOX = Control.new()
	AUDIO_VIEW_BOX.layout_mode = 2
	tab_container.add_child(AUDIO_VIEW_BOX)

	AUDIO_PLAYER_STREAM = AudioStreamPlayer.new()
	AUDIO_VIEW_BOX.add_child(AUDIO_PLAYER_STREAM)

	AUDIO_PREVIEW_BOX = GDREAudioPreviewBox.new()
	AUDIO_PREVIEW_BOX.layout_mode = 1
	AUDIO_PREVIEW_BOX.anchors_preset = 15
	AUDIO_PREVIEW_BOX.anchor_right = 1.0
	AUDIO_PREVIEW_BOX.anchor_bottom = 1.0
	AUDIO_PREVIEW_BOX.grow_horizontal = 2
	AUDIO_PREVIEW_BOX.grow_vertical = 2
	AUDIO_PREVIEW_BOX.color = Color(0.129412, 0.14902, 0.176471, 1)
	AUDIO_VIEW_BOX.add_child(AUDIO_PREVIEW_BOX)

	AUDIO_STREAM_INFO = Label.new()
	AUDIO_STREAM_INFO.layout_mode = 1
	AUDIO_STREAM_INFO.anchors_preset = -1
	AUDIO_STREAM_INFO.anchor_left = 1.0
	AUDIO_STREAM_INFO.anchor_right = 1.0
	AUDIO_STREAM_INFO.anchor_top = 1.0
	AUDIO_STREAM_INFO.anchor_bottom = 1.0
	AUDIO_STREAM_INFO.offset_left = -155.0
	AUDIO_STREAM_INFO.offset_top = -43.0
	AUDIO_STREAM_INFO.grow_horizontal = 0
	AUDIO_STREAM_INFO.grow_vertical = 0
	AUDIO_STREAM_INFO.layout_direction = 2
	AUDIO_STREAM_INFO.add_theme_color_override("font_shadow_color", Color(0, 0, 0, 1))
	AUDIO_STREAM_INFO.add_theme_color_override("font_outline_color", Color(0, 0, 0, 1))
	AUDIO_STREAM_INFO.add_theme_constant_override("outline_size", 8)
	AUDIO_STREAM_INFO.add_theme_font_size_override("font_size", 14)
	AUDIO_VIEW_BOX.add_child(AUDIO_STREAM_INFO)

	VIDEO_VIEW_BOX = Control.new()
	VIDEO_VIEW_BOX.layout_mode = 2
	VIDEO_VIEW_BOX.size_flags_vertical = 3
	tab_container.add_child(VIDEO_VIEW_BOX)

	VIDEO_ASPECT_RATIO_CONTAINER = AspectRatioContainer.new()
	VIDEO_ASPECT_RATIO_CONTAINER.layout_mode = 1
	VIDEO_ASPECT_RATIO_CONTAINER.anchors_preset = 15
	VIDEO_ASPECT_RATIO_CONTAINER.anchor_right = 1.0
	VIDEO_ASPECT_RATIO_CONTAINER.anchor_bottom = 1.0
	VIDEO_ASPECT_RATIO_CONTAINER.grow_horizontal = 2
	VIDEO_ASPECT_RATIO_CONTAINER.grow_vertical = 2
	VIDEO_ASPECT_RATIO_CONTAINER.clip_contents = true
	VIDEO_ASPECT_RATIO_CONTAINER.ratio = 1.7778
	VIDEO_VIEW_BOX.add_child(VIDEO_ASPECT_RATIO_CONTAINER)

	var bg = Panel.new()
	bg.layout_mode = 2
	var stylebox = StyleBoxFlat.new()
	stylebox.bg_color = Color(0, 0, 0, 1)
	bg.add_theme_stylebox_override("panel", stylebox)
	VIDEO_ASPECT_RATIO_CONTAINER.add_child(bg)

	VIDEO_PLAYER_STREAM = VideoStreamPlayer.new()
	VIDEO_PLAYER_STREAM.layout_mode = 2
	VIDEO_PLAYER_STREAM.custom_minimum_size = Vector2(16, 9)
	VIDEO_PLAYER_STREAM.expand = true
	VIDEO_ASPECT_RATIO_CONTAINER.add_child(VIDEO_PLAYER_STREAM)

	var BAR_MARGIN_CONTAINER = MarginContainer.new()
	BAR_MARGIN_CONTAINER.layout_mode = 2
	BAR_MARGIN_CONTAINER.add_theme_constant_override("margin_left", 40)
	BAR_MARGIN_CONTAINER.add_theme_constant_override("margin_top", 8)
	BAR_MARGIN_CONTAINER.add_theme_constant_override("margin_right", 40)
	BAR_MARGIN_CONTAINER.add_theme_constant_override("margin_bottom", 0)
	vbox_container.add_child(BAR_MARGIN_CONTAINER)

	var BAR_HBOX: HBoxContainer = HBoxContainer.new()
	BAR_HBOX.layout_mode = 2
	BAR_MARGIN_CONTAINER.add_child(BAR_HBOX)

	TIME_LABEL = Label.new()
	TIME_LABEL.layout_mode = 2
	TIME_LABEL.layout_direction = 2
	TIME_LABEL.text = "0:00.0 / 0:00.0"
	TIME_LABEL.horizontal_alignment = 2
	TIME_LABEL.vertical_alignment = 1
	BAR_HBOX.add_child(TIME_LABEL)

	var SPACER = Label.new()
	SPACER.layout_mode = 2
	SPACER.text = " "
	BAR_HBOX.add_child(SPACER)

	PROGRESS_BAR = HSlider.new()
	PROGRESS_BAR.layout_mode = 2
	PROGRESS_BAR.size_flags_horizontal = 3
	PROGRESS_BAR.size_flags_vertical = 4
	PROGRESS_BAR.step = 0.1
	BAR_HBOX.add_child(PROGRESS_BAR)

	var MEDIA_CONTROLS_HBOX = HBoxContainer.new()
	MEDIA_CONTROLS_HBOX.layout_mode = 2
	MEDIA_CONTROLS_HBOX.size_flags_horizontal = 4
	MEDIA_CONTROLS_HBOX.alignment = 1
	vbox_container.add_child(MEDIA_CONTROLS_HBOX)

	PLAY_BUTTON = Button.new()
	PLAY_BUTTON.layout_mode = 2
	PLAY_BUTTON.theme_type_variation = "FlatButton"
	PLAY_BUTTON.icon = play_icon
	PLAY_BUTTON.flat = true
	MEDIA_CONTROLS_HBOX.add_child(PLAY_BUTTON)

	var spacer1 = Control.new()
	spacer1.layout_mode = 2
	spacer1.custom_minimum_size = Vector2(10, 0)
	MEDIA_CONTROLS_HBOX.add_child(spacer1)

	PAUSE_BUTTON = Button.new()
	PAUSE_BUTTON.layout_mode = 2
	PAUSE_BUTTON.theme_type_variation = "FlatButton"
	PAUSE_BUTTON.icon = pause_icon
	PAUSE_BUTTON.disabled = true
	PAUSE_BUTTON.flat = true
	MEDIA_CONTROLS_HBOX.add_child(PAUSE_BUTTON)

	var spacer2 = Control.new()
	spacer2.layout_mode = 2
	spacer2.custom_minimum_size = Vector2(10, 0)
	MEDIA_CONTROLS_HBOX.add_child(spacer2)

	STOP_BUTTON = Button.new()
	STOP_BUTTON.layout_mode = 2
	STOP_BUTTON.theme_type_variation = "FlatButton"
	STOP_BUTTON.icon = stop_icon
	STOP_BUTTON.flat = true
	MEDIA_CONTROLS_HBOX.add_child(STOP_BUTTON)
