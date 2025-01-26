extends Control
var AUDIO_PLAYER: Control = null
var AUDIO_PLAYER_TIME_LABEL: Label = null
var AUDIO_PLAYER_STREAM: AudioStreamPlayer = null
var AUDIO_PREVIEW_BOX: GDREAudioPreviewBox = null
var AUDIO_VIEW_BOX: Control = null
var AUDIO_STREAM_INFO: Label = null
var PROGRESS_BAR: Slider = null
var PLAY_BUTTON: Button = null
var PAUSE_BUTTON: Button = null
var dragging_slider: bool = false
var last_updated_time: float = 0
var last_seek_pos: float = -1

func reset():
	AUDIO_VIEW_BOX.visible = false
	AUDIO_PREVIEW_BOX.reset()
	AUDIO_PLAYER_STREAM.stream = null
	PROGRESS_BAR.value = 0
	PROGRESS_BAR.max_value = 0
	last_updated_time = 0
	last_updated_time = -1
	dragging_slider = false

func stop():
	AUDIO_PLAYER_STREAM.stop()

func play():
	var pos = PROGRESS_BAR.value
	if (PROGRESS_BAR.value == PROGRESS_BAR.max_value):
		pos = 0
	AUDIO_PLAYER_STREAM.play(pos)
	update_progress_bar()

func is_playing() -> bool:
	return AUDIO_PLAYER_STREAM.playing

func _seek(pos: float):
	AUDIO_PLAYER_STREAM.seek(pos)

func seek(pos: float):
	var length = get_stream_length()
	if (length - pos < PROGRESS_BAR.step):
		pos = length - PROGRESS_BAR.step
	_seek(pos)
	last_seek_pos = pos

func is_stream_loaded() -> bool:
	return !(not AUDIO_PLAYER_STREAM or not AUDIO_PLAYER_STREAM.stream)

func get_stream_length() -> float:
	if not is_stream_loaded():
		return 0
	return AUDIO_PLAYER_STREAM.stream.get_length()

func get_playback_position() -> float:
	if not is_stream_loaded():
		return 0
	return AUDIO_PLAYER_STREAM.get_playback_position()

func update_audio_preview_pos():
	if not is_stream_loaded():
		return
	AUDIO_PREVIEW_BOX.update_pos(get_playback_position())


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

func load_sample(path):
	AUDIO_VIEW_BOX.visible = true
	var audio_stream: AudioStream = ResourceCompatLoader.real_load(path, "", ResourceFormatLoader.CACHE_MODE_IGNORE_DEEP)
	AUDIO_PLAYER_STREAM.stream = audio_stream
	if (AUDIO_PLAYER_STREAM.stream == null):
		return false
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
	if (AUDIO_PLAYER_STREAM.stream.get_length() < 30):
		PROGRESS_BAR.step = 0.01
	else:
		PROGRESS_BAR.step = 0.1
	update_progress_bar()
	return true

func update_progress_bar():
	if not is_stream_loaded():
		return
	PROGRESS_BAR.max_value = get_stream_length()
	PROGRESS_BAR.value = get_playback_position()
	update_text_label()


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


func update_text_label():
	AUDIO_PLAYER_TIME_LABEL.text = time_from_float(PROGRESS_BAR.value, PROGRESS_BAR.step) + " / " + time_from_float(PROGRESS_BAR.max_value, PROGRESS_BAR.step)


func _on_play_pressed() -> void:
	play()
	pass # Replace with function body.


func _on_pause_pressed() -> void:
	stop()
	pass # Replace with function body.


func _on_slider_drag_started() -> void:
	var pos = PROGRESS_BAR.value
	seek(pos)
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

func _process(delta: float) -> void:
	if not is_stream_loaded() or not AUDIO_PLAYER.visible:
		return
	if is_playing():
		if not dragging_slider:
			update_progress_bar()
		else:
			last_updated_time += delta
			if last_updated_time > 0.10:
				if (abs(last_seek_pos - PROGRESS_BAR.value)) > 0.2:
					seek(PROGRESS_BAR.value)
					last_updated_time = 0
					last_seek_pos = PROGRESS_BAR.value
		update_audio_preview_pos()


func _on_progress_bar_value_changed(_value: float) -> void:
	update_text_label()
	AUDIO_PREVIEW_BOX.update_pos(PROGRESS_BAR.value)

func _on_audio_preview_box_pos_changed(value: float) -> void:
	seek(value)
	PROGRESS_BAR.max_value = AUDIO_PLAYER_STREAM.stream.get_length()
	PROGRESS_BAR.value = value
	update_text_label()

func _ready():
	AUDIO_PLAYER = get_node(".")
	AUDIO_PLAYER_TIME_LABEL = get_node("BarHBox/TimeLabel")
	AUDIO_PLAYER_STREAM = get_node("AudioStreamPlayer")
	PROGRESS_BAR = get_node("BarHBox/ProgressBar")
	PLAY_BUTTON = get_node("Play")
	PAUSE_BUTTON = get_node("Pause")
	AUDIO_PREVIEW_BOX = get_node("AudioViewBox/AudioPreviewBox")
	AUDIO_VIEW_BOX = get_node("AudioViewBox")
	AUDIO_STREAM_INFO = get_node("AudioViewBox/AudioStreamInfo")
	# connect signals
	PROGRESS_BAR.connect("drag_started", self._on_slider_drag_started)
	PROGRESS_BAR.connect("drag_ended", self._on_slider_drag_ended)
	AUDIO_PLAYER_STREAM.connect("finished", self._on_audio_stream_player_finished)
	PLAY_BUTTON.connect("pressed", self._on_play_pressed)
	PAUSE_BUTTON.connect("pressed", self._on_pause_pressed)
	PROGRESS_BAR.connect("value_changed", self._on_progress_bar_value_changed)
	AUDIO_PREVIEW_BOX.connect("pos_changed", self._on_audio_preview_box_pos_changed)

	#load_sample("res://anomaly 105 jun12.ogg")
	# load_sample("res://2.wav")
