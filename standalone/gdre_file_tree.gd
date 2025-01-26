class_name GDREFileTree
extends Tree

var FILTER_DELAY = 0.25
var LARGE_PCK = 5000
var prev_filter_string: String = ""
var timer: SceneTreeTimer = null
@export var file_icon: Texture2D = preload("res://gdre_icons/gdre_File.svg")
@export var file_ok: Texture2D = preload("res://gdre_icons/gdre_FileOk.svg")
@export var file_broken: Texture2D = preload("res://gdre_icons/gdre_FileBroken.svg")
@export var folder_icon: Texture2D = get_theme_icon("folder", "FileDialog")
# @export var editable_only_when_checkbox_clicked: bool = true
# make setters for the export
@export var editable_only_when_checkbox_clicked: bool = true:
	set(val):
		editable_only_when_checkbox_clicked = val
		GodotREEditorStandalone.tree_set_edit_checkbox_cell_only_when_checkbox_is_pressed(self, editable_only_when_checkbox_clicked)
	get:
		return editable_only_when_checkbox_clicked
@export var right_click_outline_color: Color = Color(0.8, 0.8, 0.8, 0.9)

# var isHiDPI = DisplayServer.screen_get_dpi() >= 240
var isHiDPI = false
var root: TreeItem = null
var userroot: TreeItem = null
var num_files:int = 0
var num_broken:int = 0
var num_malformed:int = 0
var items: Dictionary[String, TreeItem] = {}
var right_click_menu: PopupMenu = null
var right_clicked_item: TreeItem = null
enum SortType {
	SORT_NAME_ASCENDING,
	SORT_NAME_DESCENDING,
	SORT_SIZE_DESCENDING,
	SORT_SIZE_ASCENDING,
	SORT_INFO,
	SORT_REVERSE_INFO,
}

enum {
	COLUMN_NAME,
	COLUMN_SIZE,
	COLUMN_INFO,
}

enum {
	POPUP_COPY_PATHS,
	POPUP_CHECK_ALL,
	POPUP_UNCHECK_ALL,
	POPUP_SEPERATOR,
	POPUP_FOLD_ALL,
	POPUP_UNFOLD_ALL,
}
var current_sort = SortType.SORT_NAME_ASCENDING

func get_highlighted_items() -> Array:
	var highlighted_items = []
	if (select_mode == SELECT_MULTI):
		var item = self.get_next_selected(get_root())
		while (item):
			highlighted_items.append(item)
			item = self.get_next_selected(item)
	if highlighted_items.is_empty():
		var item = self.get_selected()
		if (item):
			highlighted_items.append(item)
	return highlighted_items

func _propagate_check(item: TreeItem, checked: bool, check_visible_ignore_folders: bool = false):
	if (check_visible_ignore_folders and (not item.is_visible_in_tree())):
		return
	if (not check_visible_ignore_folders or item.get_icon(0) != folder_icon):
		item.set_checked(0, checked)
	var it: TreeItem = item.get_first_child()
	while (it):
		_propagate_check(it, checked, check_visible_ignore_folders)
		it = it.get_next()

func _on_item_edited():
	var item = self.get_edited()
	var checked = item.is_checked(0)
	_propagate_check(item, checked)

func check_if_multiple_items_are_highlighted():
	if (select_mode == SELECT_MULTI):
		var first_item = self.get_next_selected(get_root())
		var item = self.get_next_selected(first_item)
		if (item):
			return true
	return false

func _on_gui_input(input:InputEvent):
	if input is InputEventMouseButton:
		var item = self.get_item_at_position(get_local_mouse_position())
		if (item):
			if input.double_click and input.button_index == MOUSE_BUTTON_LEFT:
				if item_is_folder(item):
					item.collapsed = not item.collapsed
			else:
				_on_custom_item_clicked(input.button_index)

func item_is_folder(item: TreeItem) -> bool:
	return item.get_icon(0) == folder_icon

func items_has_folder(items: Array) -> bool:
	for item in items:
		if item_is_folder(item):
			return true
	return false

func right_clicked_item_is_selected(selected_items: Array = []) -> bool:
	if not selected_items:
		selected_items = get_highlighted_items()
	var selected_items_has_right_clicked_item = true
	if right_clicked_item:
		selected_items_has_right_clicked_item = selected_items.has(right_clicked_item)
	return selected_items_has_right_clicked_item

func set_right_clicked_outline_color(clear_bg: bool = false):
	if not right_clicked_item:
		return
	for i in range(0, columns):
		if not clear_bg:
			right_clicked_item.set_custom_bg_color(i, self.right_click_outline_color, true)
		else:
			right_clicked_item.set_custom_bg_color(i, Color(0,0,0,0), true)

func _on_custom_item_clicked(mouse_button: MouseButton):
	if (mouse_button == MOUSE_BUTTON_RIGHT):
		clear_right_click_state()
		var selected_items = get_highlighted_items()
		var has_folder = false
		right_clicked_item = self.get_item_at_position(get_local_mouse_position())
		if (not right_clicked_item_is_selected(selected_items)):
			right_click_menu.set_item_text(POPUP_COPY_PATHS, "Copy path")
			right_click_menu.set_item_text(POPUP_UNCHECK_ALL, "Uncheck item")
			right_click_menu.set_item_text(POPUP_CHECK_ALL, "Check item")
			set_right_clicked_outline_color(false)
			if right_clicked_item and item_is_folder(right_clicked_item):
				has_folder = true
		else:
			if items_has_folder(selected_items):
				has_folder = true
			if (selected_items.size() > 1):
				right_click_menu.set_item_text(POPUP_COPY_PATHS, "Copy paths")
				right_click_menu.set_item_text(POPUP_UNCHECK_ALL, "Uncheck selected")
				right_click_menu.set_item_text(POPUP_CHECK_ALL, "Check selected")
			else:
				right_click_menu.set_item_text(POPUP_COPY_PATHS, "Copy path")
				right_click_menu.set_item_text(POPUP_UNCHECK_ALL, "Uncheck selected")
				right_click_menu.set_item_text(POPUP_CHECK_ALL, "Check selected")

		if (has_folder):
			right_click_menu.set_item_disabled(POPUP_FOLD_ALL, false)
			right_click_menu.set_item_disabled(POPUP_UNFOLD_ALL, false)
		else:
			right_click_menu.set_item_disabled(POPUP_FOLD_ALL, true)
			right_click_menu.set_item_disabled(POPUP_UNFOLD_ALL, true)

		right_click_menu.position = DisplayServer.mouse_get_position()
		right_click_menu.visible = true

func clear_right_click_state():
	set_right_clicked_outline_color(true)
	right_clicked_item = null

func _on_right_click_id(id):
	var selected_items = get_highlighted_items()
	if not right_clicked_item_is_selected(selected_items):
		selected_items = [right_clicked_item]
	clear_right_click_state()
	right_click_menu.hide()
	match id:
		POPUP_COPY_PATHS:
			var selected_files: PackedStringArray = []
			for item in selected_items:
				selected_files.append(_get_path(item))
			DisplayServer.clipboard_set("\n".join(selected_files))
		POPUP_UNCHECK_ALL:
			for item in selected_items:
				_propagate_check(item, false)
		POPUP_CHECK_ALL:
			for item in selected_items:
				_propagate_check(item, true)
		POPUP_FOLD_ALL:
			for item in selected_items:
				set_fold_all(item, true, true)
		POPUP_UNFOLD_ALL:
			for item in selected_items:
				set_fold_all(item, false, true)

func _on_right_click_visibility_changed():
	if not right_click_menu.visible:
		set_right_clicked_outline_color(true)


func cmp_item_folders(a: TreeItem, b: TreeItem, descending_name_sort: bool) -> int:
	var a_is_folder = a.get_icon(0) == folder_icon
	var b_is_folder = b.get_icon(0) == folder_icon
	if (a_is_folder and !b_is_folder):
		return 1 if not descending_name_sort else -1
	if (!a_is_folder and b_is_folder):
		return -1 if not descending_name_sort else 1
	if (descending_name_sort):
		return a.get_text(0).filenocasecmp_to(b.get_text(0))
	return b.get_text(0).filenocasecmp_to(a.get_text(0))

func sort_tree(item:TreeItem, recursive: bool = true):
	var it: TreeItem = item.get_first_child()
	var arr: Array = []
	while (it):
		if recursive:
			sort_tree(it)
		arr.append(it)
		it = it.get_next()
	if (arr.size() <= 1):
		return
	match current_sort:
		SortType.SORT_NAME_ASCENDING:
			arr.sort_custom(func(a: TreeItem, b: TreeItem) -> bool:
				return cmp_item_folders(a, b, false) > 0
			)
		SortType.SORT_NAME_DESCENDING:
			arr.sort_custom(func(a: TreeItem, b: TreeItem) -> bool:
				return cmp_item_folders(a, b, true) > 0
			)
		SortType.SORT_SIZE_DESCENDING:
			arr.sort_custom(func(a: TreeItem, b: TreeItem) -> bool:
				var a_size = a.get_metadata(1)
				var b_size = b.get_metadata(1)
				if (a_size == b_size):
					return cmp_item_folders(a, b, true) > 0
				return a_size > b_size
			)
		SortType.SORT_SIZE_ASCENDING:
			arr.sort_custom(func(a: TreeItem, b: TreeItem) -> bool:
				var a_size = (a.get_metadata(1))
				var b_size = (b.get_metadata(1))
				if (a_size == b_size):
					return cmp_item_folders(a, b, false) > 0
				return a_size < b_size
			)
		SortType.SORT_INFO:
			arr.sort_custom(func(a: TreeItem, b: TreeItem) -> bool:
				return a.get_text(2).filenocasecmp_to(b.get_text(2)) > 0
			)
		SortType.SORT_REVERSE_INFO:
			arr.sort_custom(func(a: TreeItem, b: TreeItem) -> bool:
				return a.get_text(2).filenocasecmp_to(b.get_text(2)) < 0
			)
	var names: PackedStringArray = []
	for i in range(arr.size()):
		names.push_back(arr[i].get_text(0))
	
	arr[0].move_before(arr[1])
	for i in range(1, arr.size()):
		arr[i].move_after(arr[i - 1])

func sort_entire_tree():
	self.set_column_title(COLUMN_NAME, "File name")
	match(current_sort):
		SortType.SORT_NAME_ASCENDING:
			self.set_column_title(COLUMN_NAME, "File name ▲")
		SortType.SORT_NAME_DESCENDING:
			self.set_column_title(COLUMN_NAME, "File name ▼")
		_:
			self.set_column_title(COLUMN_NAME, "File name")
	if (columns >= 2):
		match(current_sort):
			SortType.SORT_SIZE_DESCENDING:
				self.set_column_title(COLUMN_SIZE, "Size ▼")
			SortType.SORT_SIZE_ASCENDING:
				self.set_column_title(COLUMN_SIZE, "Size ▲")
			_:
				self.set_column_title(COLUMN_SIZE, "Size")
	if (columns >= 3):
		match(current_sort):
			SortType.SORT_INFO:
				self.set_column_title(COLUMN_INFO, "Info ▲")
			SortType.SORT_REVERSE_INFO:
				self.set_column_title(COLUMN_INFO, "Info ▼")
			_:
				self.set_column_title(COLUMN_INFO, "Info")
	sort_tree(root)
	if (userroot):
		sort_tree(userroot)


func _on_column_title_clicked(column: int, mouse_button_index: int):
	if (mouse_button_index == MOUSE_BUTTON_LEFT):
		match(column):
			COLUMN_NAME:
				current_sort = SortType.SORT_NAME_ASCENDING if current_sort != SortType.SORT_NAME_ASCENDING else SortType.SORT_NAME_DESCENDING
			COLUMN_SIZE:
				current_sort = SortType.SORT_SIZE_DESCENDING if current_sort != SortType.SORT_SIZE_DESCENDING else SortType.SORT_SIZE_ASCENDING
			COLUMN_INFO:
				current_sort = SortType.SORT_INFO if current_sort != SortType.SORT_INFO else SortType.SORT_REVERSE_INFO
			_:
				return
		sort_entire_tree()

func _on_empty_clicked(_mouse_pos, mouse_button: MouseButton):
	if (mouse_button == MOUSE_BUTTON_LEFT):
		self.deselect_all()

func _get_path(item: TreeItem) -> String:
	var path = item.get_metadata(0)
	if (path.is_empty()):
		path = item.get_text(0)
		item = item.get_parent()
		while (item):
			var item_name: String = item.get_text(0)
			if (item_name.begins_with("user://") or item_name.begins_with("res://")):
				path = item_name + path
				break
			path = item_name + "/" + path
			item = item.get_parent()
	return path


func _ready():
	var file_list: Tree = self
	right_click_menu = PopupMenu.new()
	right_click_menu.add_item("Copy path", POPUP_COPY_PATHS)
	right_click_menu.add_item("Check", POPUP_CHECK_ALL)
	right_click_menu.add_item("Uncheck", POPUP_UNCHECK_ALL)
	right_click_menu.add_separator("", POPUP_SEPERATOR)
	right_click_menu.add_item("Fold all", POPUP_FOLD_ALL)
	right_click_menu.add_item("Unfold all", POPUP_UNFOLD_ALL)

	right_click_menu.visible = false
	right_click_menu.connect("id_pressed", self._on_right_click_id)
	right_click_menu.connect("visibility_changed", self._on_right_click_visibility_changed)
	add_child(right_click_menu)
	# get the number of columns set
	var num_columns = file_list.columns
	if num_columns <= 0:
		print("No columns set in the Tree")
		return
	self.connect("gui_input", self._on_gui_input)
	self.connect("empty_clicked", self._on_empty_clicked)
	#column_title_clicked( column: int, mouse_button_index: int )
	self.connect("column_title_clicked", self._on_column_title_clicked)
	file_list.set_column_title(0, "File name")
	file_list.set_column_expand(0, true)
	if (num_columns >= 2):
		file_list.set_column_title(1, "Size")
		file_list.set_column_expand(1, false)
		file_list.set_column_custom_minimum_width(1, 120)
		file_list.add_theme_constant_override("draw_relationship_lines", 1)
	elif (num_columns >= 3):
		file_list.set_column_title(2, "Info")
		file_list.set_column_custom_minimum_width(2, 120)
		file_list.set_column_expand(2, false)
	file_list.connect("item_edited", self._on_item_edited)

func set_fold_all_children(item: TreeItem, collapsed: bool = true, recursive: bool = false):
	var it: TreeItem = item.get_first_child()
	while (it):
		if (it.get_icon(0) == folder_icon):
			it.collapsed = collapsed
		if (recursive):
			set_fold_all_children(it, collapsed, recursive)
		it = it.get_next()

func set_fold_all(item: TreeItem, collapsed: bool = true, recursive: bool = false):
	set_fold_all_children(item, collapsed, recursive)
	item.collapsed = collapsed

func add_files(infos: Array, skipped_md5_check: bool = false):
	# reverse alphabetical order, we want to put directories at the front in alpha order

	# infos.sort_custom(func(a, b) -> bool:
	# 	return a.get_path().filenocasecmp_to(b.get_path()) <= 0
	# )
	for file in infos:
		_add_file(file, skipped_md5_check)
	# collapse all the first level directories
	sort_entire_tree()
	set_fold_all_children(root, true, true)

func _add_file(info: PackedFileInfo, skipped_md5_check: bool = false):
	num_files += 1
	var file_size = info.get_size()
	var path = info.get_path()
	var is_malformed = info.is_malformed()
	var is_verified = info.is_checksum_validated()
	var has_md5 = info.has_md5()
	var icon = file_icon
	var errstr = ""
	if is_malformed:
		icon = file_broken
		errstr = "Malformed path"
		num_malformed += 1
	elif is_verified:
		icon = file_ok
	elif skipped_md5_check:
		icon = file_icon
	elif !is_verified && has_md5:
		icon = file_broken
		errstr = "Checksum mismatch"
		num_broken += 1
	var item
	if ("user://" in path):
		if userroot == null:
			userroot = self.create_item(root)
			userroot.set_cell_mode(0, TreeItem.CELL_MODE_CHECK)
			userroot.set_checked(0, true)
			userroot.set_editable(0, true)
			userroot.set_icon(0, folder_icon)
			userroot.set_text(0, "user://")
			userroot.set_metadata(0, String())
		item = add_file_to_item(userroot, path, path.replace("user://", ""), file_size, icon, errstr, false)
	else:
		item = add_file_to_item(root, path, path.replace("res://", ""), file_size, icon, errstr, false)
	items[path] = item
	if (items.size() > LARGE_PCK):
		FILTER_DELAY = 0.5
	

func _clear():
	if (userroot != null):
		userroot.clear()
		userroot = null
	items.clear()
	self.clear()
	num_files = 0
	num_broken = 0
	num_malformed = 0
	root = self.create_item()
	root.set_cell_mode(0, TreeItem.CELL_MODE_CHECK)
	root.set_checked(0, true)
	root.set_editable(0, true)
	root.set_icon(0, folder_icon)
	root.set_text(0, "res://")
	root.set_metadata(0, String())
	
func get_first_index_of_non_folder_child(p_item: TreeItem):
	var it: TreeItem = p_item.get_first_child()
	while (it):
		if (it.get_icon(0) != folder_icon):
			return it.get_index()
		it = it.get_next()
	return -1
	
func add_file_to_item(p_item: TreeItem, p_fullname: String, p_name: String, p_size: int, p_icon: Texture2D,  p_error: String, p_enc: bool):
	var pp: int = p_name.find("/")
	if (pp == -1):
		# Add file
		var item: TreeItem = self.create_item(p_item);
		item.set_cell_mode(0, TreeItem.CELL_MODE_CHECK);
		item.set_checked(0, true);
		item.set_editable(0, true);
		item.set_icon(0, p_icon)
		item.set_text(0, p_name)
		item.set_metadata(0, p_fullname)
		item.set_tooltip_text(0, p_error);
		if columns >= 2:
			if (p_size < (1024)):
				item.set_text(1, String.num_int64(p_size) + " B");
			elif (p_size < (1024 * 1024)):
				item.set_text(1, String.num(float(p_size) / 1024, 2) + " KiB");
			elif (p_size < (1024 * 1024 * 1024)):
				item.set_text(1, String.num(float(p_size) / (1024 * 1024), 2) + " MiB");
			else:
				item.set_text(1, String.num(float(p_size) / (1024 * 1024 * 1024), 2) + " GiB");
			item.set_metadata(1, p_size)
			item.set_tooltip_text(1, p_error);
		if columns >= 3:
			item.set_text(2, "Encrypted" if p_enc else "");
		return item
	else:
		var fld_name: String = p_name.substr(0, pp);
		var path: String = p_name.substr(pp + 1, p_name.length());
		# Add folder if any
		var it: TreeItem = p_item.get_first_child();
		while (it) :
			if (it.get_text(0) == fld_name) :
				return add_file_to_item(it, p_fullname, path, p_size, p_icon, p_error, false);
			it = it.get_next()
		
		var item:TreeItem = self.create_item(p_item, get_first_index_of_non_folder_child(p_item)) # directories at the front
		item.set_cell_mode(0, TreeItem.CELL_MODE_CHECK)
		item.set_checked(0, true)
		item.set_editable(0, true)
		item.set_icon(0, folder_icon)
		item.set_text(0, fld_name)
		item.set_metadata(0, String())
		if columns >= 2:
			item.set_metadata(1, 0)
		return add_file_to_item(item, p_fullname, path, p_size, p_icon, p_error, false);

func _filter_item(filter_str: String, item: TreeItem, is_glob: bool, clear_filter: bool):
	if (item.get_icon(0) == folder_icon): # directory
		var one_item_visible = false
		var it: TreeItem = item.get_first_child()
		while (it):
			var item_visible = _filter_item(filter_str, it, is_glob, clear_filter)
			if (not one_item_visible and item_visible):
				one_item_visible = true
			it = it.get_next()
		item.visible = one_item_visible
		if (not clear_filter and one_item_visible and item.collapsed):
			item.collapsed = false
		return one_item_visible
	var path: String = item.get_metadata(0)
	if clear_filter:
		item.visible = true
		return true
	if (is_glob and path.matchn(filter_str)) or (!is_glob and path.contains(filter_str)):
		item.visible = true
		return true
	else:
		item.visible = false
		return false
	
func _filter(filter_str):
	var is_glob = filter_str.contains("*")
	var clear_filter = filter_str.is_empty() or filter_str == "*"
	_filter_item(filter_str, root, is_glob, clear_filter)
	if (userroot):
		_filter_item(filter_str, userroot, is_glob, clear_filter)

func _filter_timeout():
	timer = null
	_filter(prev_filter_string)
	prev_filter_string = ""

func filter(filter_str: String):
	if FILTER_DELAY > 0:
		if (timer != null):
			timer.disconnect("timeout", self._filter_timeout)
			timer = null
		timer = get_tree().create_timer(FILTER_DELAY)
		timer.connect("timeout", self._filter_timeout)
		prev_filter_string = filter_str
		return
	_filter(filter_str)
			
func check_all_shown(checked: bool):
	var it: TreeItem = root.get_first_child()
	while (it):
		_propagate_check(it, checked, true)
		it = it.get_next()
	if (userroot):
		it = userroot.get_first_child()
		while (it):
			_propagate_check(it, checked, true)
			it = it.get_next()

func _get_selected_files(p_item: TreeItem):
	var arr: PackedStringArray = []
	var it: TreeItem = p_item
	var p_name: String = it.get_metadata(0)
	if (it.is_checked(0) and !p_name.is_empty()):
		arr.append(p_name)
	it = p_item.get_first_child();
	while (it):
		arr.append_array(_get_selected_files(it))
		it = it.get_next()
	return arr
	
func get_checked_files() -> PackedStringArray:
	var arr: PackedStringArray = _get_selected_files(root)
	if (userroot != null):
		arr.append_array(_get_selected_files(userroot))
	return arr

func _init():
	var arrow = get_theme_icon("arrow", "Tree")
	GodotREEditorStandalone.tree_set_edit_checkbox_cell_only_when_checkbox_is_pressed(self, editable_only_when_checkbox_clicked)
	pass


func _process(_delta:float):
	pass
