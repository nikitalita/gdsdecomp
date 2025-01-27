class_name GDRECompDecomp
extends GDREChildDialog



func confirm():
	assert(false, "Not implemented")

func clear():
	$Box/VBoxContainer/FileList.clear()

func close():
	clear()
	hide_win()

func cancelled():
	clear()
	hide_win()

func get_file_list():
	var ret: PackedStringArray = []
	for i in range($Box/VBoxContainer/FileList.get_item_count()):
		ret.append($Box/VBoxContainer/FileList.get_item_text(i))
	return ret

func _validate():
	if $Box/VBoxContainer/FileList.get_item_count() == 0:
		self.get_ok_button().disabled = true
	elif $Box/VBoxContainer/BytecodeSelector.get_selected_id() == 0:
		self.get_ok_button().disabled = true
	elif $Box/VBoxContainer/DestinationFolderHBox/DestinationFolder.text == "":
		self.get_ok_button().disabled = true
	else:
		self.get_ok_button().disabled = false

func _on_add_files_button_pressed() -> void:
	$AddFilesDialog.popup_centered()

func _on_remove_files_button_pressed() -> void:
	# get all the selected items in the list, then remove them
	var items: PackedInt32Array = $Box/VBoxContainer/FileList.get_selected_items()
	items.reverse()
	for item in items:
		$Box/VBoxContainer/FileList.remove_item(item)
	_validate()


func _on_clear_files_button_pressed() -> void:
	$Box/VBoxContainer/FileList.clear()
	_validate()


func _on_destination_folder_button_pressed() -> void:
	$DestinationFolderDialog.popup_centered()

func _on_destination_folder_dialog_dir_selected(dir: String) -> void:
	$Box/VBoxContainer/DestinationFolderHBox/DestinationFolder.text = dir
	_validate()

func _on_add_files_dialog_files_selected(paths: PackedStringArray) -> void:
	for path in paths:
		$Box/VBoxContainer/FileList.add_item(path)
	_validate()

func _on_bytecode_selector_item_selected(_index: int) -> void:
	_validate()

func _on_destination_folder_text_changed(_new_text: String) -> void:
	_validate()

func _ready() -> void:
	self.add_cancel_button("Cancel")
	var versions = GDScriptDecomp.get_bytecode_versions()
	if $Box/VBoxContainer/BytecodeSelector.get_item_count() != versions.size() + 1:
		$Box/VBoxContainer/BytecodeSelector.clear()
		$Box/VBoxContainer/BytecodeSelector.add_item("--- Please select bytecode version ---")
		for version in versions:
			$Box/VBoxContainer/BytecodeSelector.add_item(version)
		$Box/VBoxContainer/BytecodeSelector.select(0)
	self.connect("files_dropped", self._on_add_files_dialog_files_selected)

	$Box/VBoxContainer/ButtonHBox/AddFilesButton.connect("pressed", self._on_add_files_button_pressed)
	$Box/VBoxContainer/ButtonHBox/RemoveFilesButton.connect("pressed", self._on_remove_files_button_pressed)
	$Box/VBoxContainer/ButtonHBox/ClearFilesButton.connect("pressed", self._on_clear_files_button_pressed)
	$Box/VBoxContainer/DestinationFolderHBox/DestinationFolderButton.connect("pressed", self._on_destination_folder_button_pressed)
	$Box/VBoxContainer/DestinationFolderHBox/DestinationFolder.connect("text_changed", self._on_destination_folder_text_changed)
	$Box/VBoxContainer/BytecodeSelector.connect("item_selected", self._on_bytecode_selector_item_selected)
	$AddFilesDialog.connect("files_selected", self._on_add_files_dialog_files_selected)
	$DestinationFolderDialog.connect("dir_selected", self._on_destination_folder_dialog_dir_selected)
	_validate()
