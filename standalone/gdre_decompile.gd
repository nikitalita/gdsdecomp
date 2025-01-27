class_name GDREDecompile
extends GDRECompDecomp

func confirm():
	var selector:OptionButton = $Box/VBoxContainer/BytecodeSelector
	var bytecode_str = selector.get_item_text( selector.get_selected_id())
	var bytecode_version = bytecode_str.split(" ", false, 1)[0].strip_edges()
	var decomp = GDScriptDecomp.create_decomp_for_version(bytecode_version)
	if decomp == null:
		popup_error_box("Failed to create decompiler for version: " + bytecode_version, "Decompiler Error")
		return
	var files = get_file_list()
	if files.size() == 0:
		popup_error_box("No files selected", "Error")
		return
	var dest_folder = $Box/VBoxContainer/DestinationFolderHBox/DestinationFolder.text
	if dest_folder == "":
		popup_error_box("No destination folder selected", "Error")
		return
	for file in files:
		var err = decomp.decompile_byte_code(file)
		if err != OK:
			popup_error_box("Failed to decompile file: " + file, "Decompile Error")
			return
		var decompiled = decomp.get_script_text()
		var dest_file = dest_folder.path_join(file.get_file().get_basename() + ".gd")
		var f = FileAccess.open(dest_file, FileAccess.WRITE)
		if f == null:
			popup_error_box("Failed to open file for writing: " + dest_file, "File Error")
			return
		f.store_string(decompiled)
		f.close()
	popup_confirm_box("The following files were successfully decompiled: " + "\n".join(files), "Decompile Complete", self.close, self.close)
