extends Control

func _ready():
	
	get_node("menu_background/version_lbl").text = get_node("re_editor_standalone").get_version()
	get_node("log_window_bg/log_window").set_scroll_active(true)
	get_node("log_window_bg/log_window").set_scroll_follow(true)

func _on_re_editor_standalone_write_log_message(message):
	get_node("log_window_bg/log_window").add_text(message)

func _on_version_lbl_pressed():
	OS.shell_open("https://github.com/bruvzg/gdsdecomp")
