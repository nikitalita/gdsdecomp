class_name GDREAudioPreviewBox 
extends ColorRect

@export var editable: bool = true
var preview: GDREAudioStreamPreview = null
var stream: AudioStream = null
var pos: float = 0
var dragging: bool = false

func _get_indicator_color(color):
	return color.lerp(Color(1, 1, 1, 0.5), 0.5)
func _get_indicator_color_full_white(color):
	var col = _get_indicator_color(color)
	col.a = 1
	return col
func _get_default_indicator_color():
	return _get_indicator_color(self.color)

signal pos_changed(pos: float)

const EMPTY_COLOR = Color(0,0,0,1)
const DEFAULT_BACKGROUND = Color("21262d") # 0.129 0.149 0.176
@export var lineColor: Color = Color(1, 1, 1, 1)
@export var indicatorBGColor: Color = EMPTY_COLOR
@export var indicatorLineColor: Color = EMPTY_COLOR

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


func _ready():
	GDREAudioStreamPreviewGenerator.connect("preview_updated", self._on_preview_updated)
	if indicatorBGColor == EMPTY_COLOR:
		indicatorBGColor = _get_default_indicator_color()
	if indicatorLineColor == EMPTY_COLOR:
		indicatorLineColor = _get_indicator_color_full_white(indicatorBGColor)
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
