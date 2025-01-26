extends Node

class item_struct:
	var text: String = ""
	var id: int = 0
	var accel: int = 0
	var language: String = ""
	var icon: Texture2D = null
	var icon_max_width: int = 0
	var icon_modulate: Color = Color(0,0,0,0)
	var checkable: bool = false
	var checked: bool = false
	var radio: bool = false
	var separator: bool = false
	var metadata: Variant = null
	var tooltip: String = ""
	var shortcut: Shortcut = null
	var shortcut_disabled: bool = false
	var submenu: String = ""
	var indent: int = 0
	var disabled: bool = false
	func _init():
		pass
		# text = ""
		# id = 0
		# accel = 0
		# language = ""
		# icon = null
		# icon_max_width = 0
		# icon_modulate = Color(0,0,0,0)
		# checkable = false
		# checked = false
		# radio = false
		# separator = false
		# metadata = null
		# tooltip = ""
		# shortcut = null
		# shortcut_disabled = false
		# submenu = ""
		# indent = 0
		# disabled = false
	func add_to_menu(menu_btn: MenuButton):
		var popup = menu_btn.get_popup()
		var count = popup.get_item_count()
		var idx = count
		if separator:
			popup.add_separator(text)
		elif radio:
			popup.add_icon_radio_check_item(icon, text, id, accel)
		elif checkable:
			popup.add_icon_check_item(icon, text, id, accel)
		else:
			popup.add_icon_item(icon, text, id, accel)
		popup.set_item_checked(idx, checked)
		popup.set_item_metadata(idx, metadata)
		popup.set_item_tooltip(idx, tooltip)
		popup.set_item_shortcut(idx, shortcut)
		popup.set_item_shortcut_disabled(idx, shortcut_disabled)
		popup.set_item_submenu(idx, submenu)
		popup.set_item_indent(idx, indent)
		popup.set_item_disabled(idx, disabled)
		popup.set_item_icon_max_width(idx, icon_max_width)
		popup.set_item_icon_modulate(idx, icon_modulate)
		popup.set_item_language(idx, language)



func readd_items(menu_btn: MenuButton):
	var popup = menu_btn.get_popup()
	var item_count = menu_btn.get_popup().get_item_count()
	var arr: Array = []
	for i in range(item_count):
		var item = item_struct.new()
		item.text = popup.get_item_text(i)
		item.id = popup.get_item_id(i)
		item.accel = popup.get_item_accelerator(i)
		item.language = popup.get_item_language(i)
		item.icon = popup.get_item_icon(i)
		item.icon_max_width = popup.get_item_icon_max_width(i)
		item.icon_modulate = popup.get_item_icon_modulate(i)
		item.checkable = popup.is_item_checkable(i)
		item.checked = popup.is_item_checked(i)
		item.radio = popup.is_item_radio_checkable(i)
		item.separator = popup.is_item_separator(i)
		item.metadata = popup.get_item_metadata(i)
		item.tooltip = popup.get_item_tooltip(i)
		item.shortcut = popup.get_item_shortcut(i)
		item.shortcut_disabled = popup.is_item_shortcut_disabled(i)
		item.submenu = popup.get_item_submenu(i)
		item.indent = popup.get_item_indent(i)
		item.disabled = popup.is_item_disabled(i)
		print(item)
		arr.push_back(item)
	popup.clear(true)
	for item in arr:
		item.add_to_menu(menu_btn)

# Called when the node enters the scene tree for the first time.
func _ready():
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta):
	pass
