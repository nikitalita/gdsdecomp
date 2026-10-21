#pragma once
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"

#if defined(WAYLAND_ENABLED)
#include "core/os/os.h"
#endif

#include "modules/gdsdecomp/utility/gdre_version.gen.h"

namespace gdre {
static const HashSet<String> main_commands = {
	// TODO: Auto-generate this list somehow
	"--recover",
	"--extract",
	"--compile",
	"--list-bytecode-versions",
	"--pck-create",
	"--pck-patch",
	"--txt-to-bin",
	"--bin-to-txt",
	"--patch-translations",
	"--gdre-help",
	"--gdre-version",
};

static void print_version() {
	print_line("Godot RE Tools " + String(GDRE_VERSION));
}

[[maybe_unused]]
static void remove_flag(List<String> *args, const String &flag, bool has_value = false) {
	for (List<String>::Element *E = args->front(); E; E = E->next()) {
		auto arg_and_value = E->get().split("=", true, 1);
		if (arg_and_value[0] == flag) {
			// if the flag has a value, get the next element and remove it
			if (has_value && arg_and_value.size() == 1 && E->next()) {
				args->erase(E->next());
			}
			args->erase(E);
			break;
		}
	}
}

[[maybe_unused]]
static void insert_flag_at_front(List<String> *args, const String &flag, const String &value = "") {
	for (List<String>::Element *E = args->front(); E; E = E->next()) {
		if (E->get().begins_with("-")) {
			auto inserted = args->insert_before(E, flag);
			if (!value.is_empty()) {
				args->insert_after(inserted, value);
			}
			return;
		}
	}
	auto inserted = args->push_back(flag);
	if (!value.is_empty()) {
		args->insert_after(inserted, value);
	}
}

static void add_wayland_args(List<String> *args) {
#if defined(WAYLAND_ENABLED)
	if (!OS::get_singleton()->get_environment("WAYLAND_DISPLAY").is_empty()) {
		remove_flag(args, "--display-driver", true);
		insert_flag_at_front(args, "--display-driver", "wayland");
	}
#endif
}

// returns true if we should quit the program immediately
static bool modify_cli_args(List<String> *args, List<String> *user_args) {
	bool found_help = false;
	String path = "";
	bool display_driver_chosen = false;
	for (List<String>::Element *E = args->front(); E; E = E->next()) {
		String arg = E->get();
		if (arg == "--path" && E->next()) {
			path = E->next()->get();
		} else if (arg == "--help" || arg == "--gdre-help") {
			found_help = true;
		} else if (arg == "--gdre-version" || arg == "--version") {
			print_version();
			return true;
		} else if (arg == "--godot-version") {
			args->clear();
			user_args->clear();
			args->push_back("--version");
			return false;
		} else if (arg == "--godot-help") {
			args->clear();
			user_args->clear();
			args->push_back("--help");
			return false;
		} else if (arg == "--headless" || arg == "--display-driver") {
			display_driver_chosen = true;
		}
	}
	if (found_help) {
		args->clear();
		args->push_back("--headless");
		args->push_back("--no-header");
		if (!path.is_empty()) {
			args->push_back("--path");
			args->push_back(path);
		}
		user_args->clear();
		user_args->push_back("--gdre-help");
		return false;
	}
	for (List<String>::Element *E = args->front(); E; E = E->next()) {
		String arg = E->get();
		String main_command = arg.get_slice("=", 0);

		if (arg == "--" || arg == "++") {
			break;
		}
		if (main_commands.has(main_command)) {
			args->insert_before(E, "--");
			break;
		}
	}
	if (!display_driver_chosen) {
		add_wayland_args(args);
	}
	return false;
}
} //namespace gdre
