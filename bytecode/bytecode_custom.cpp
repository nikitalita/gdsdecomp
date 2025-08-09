#include "bytecode_custom.h"
#include "bytecode/bytecode_base.h"

static const HashMap<String, Pair<int, int>> builtin_func_arg_elements = {
	{ "sin", { 1, 1 } },
	{ "cos", { 1, 1 } },
	{ "tan", { 1, 1 } },
	{ "sinh", { 1, 1 } },
	{ "cosh", { 1, 1 } },
	{ "tanh", { 1, 1 } },
	{ "asin", { 1, 1 } },
	{ "acos", { 1, 1 } },
	{ "atan", { 1, 1 } },
	{ "atan2", { 2, 2 } },
	{ "sqrt", { 1, 1 } },
	{ "fmod", { 2, 2 } },
	{ "fposmod", { 2, 2 } },
	{ "posmod", { 2, 2 } },
	{ "floor", { 1, 1 } },
	{ "ceil", { 1, 1 } },
	{ "round", { 1, 1 } },
	{ "abs", { 1, 1 } },
	{ "sign", { 1, 1 } },
	{ "pow", { 2, 2 } },
	{ "log", { 1, 1 } },
	{ "exp", { 1, 1 } },
	{ "is_nan", { 1, 1 } },
	{ "is_inf", { 1, 1 } },
	{ "is_equal_approx", { 2, 2 } },
	{ "is_zero_approx", { 1, 1 } },
	{ "ease", { 2, 2 } },
	{ "decimals", { 1, 1 } },
	{ "step_decimals", { 1, 1 } },
	{ "stepify", { 2, 2 } },
	{ "lerp", { 3, 3 } },
	{ "lerp_angle", { 3, 3 } },
	{ "inverse_lerp", { 3, 3 } },
	{ "range_lerp", { 5, 5 } },
	{ "smoothstep", { 3, 3 } },
	{ "move_toward", { 3, 3 } },
	{ "dectime", { 3, 3 } },
	{ "randomize", { 0, 0 } },
	{ "randi", { 0, 0 } },
	{ "randf", { 0, 0 } },
	{ "rand_range", { 2, 2 } },
	{ "seed", { 1, 1 } },
	{ "rand_seed", { 1, 1 } },
	{ "deg2rad", { 1, 1 } },
	{ "rad2deg", { 1, 1 } },
	{ "linear2db", { 1, 1 } },
	{ "db2linear", { 1, 1 } },
	{ "polar2cartesian", { 2, 2 } },
	{ "cartesian2polar", { 2, 2 } },
	{ "wrapi", { 3, 3 } },
	{ "wrapf", { 3, 3 } },
	{ "max", { 2, 2 } },
	{ "min", { 2, 2 } },
	{ "clamp", { 3, 3 } },
	{ "nearest_po2", { 1, 1 } },
	{ "weakref", { 1, 1 } },
	{ "funcref", { 2, 2 } },
	{ "convert", { 2, 2 } },
	{ "typeof", { 1, 1 } },
	{ "type_exists", { 1, 1 } },
	{ "char", { 1, 1 } },
	{ "ord", { 1, 1 } },
	{ "str", { 1, INT_MAX } },
	{ "print", { 0, INT_MAX } },
	{ "printt", { 0, INT_MAX } },
	{ "prints", { 0, INT_MAX } },
	{ "printerr", { 0, INT_MAX } },
	{ "printraw", { 0, INT_MAX } },
	{ "print_debug", { 0, INT_MAX } },
	{ "push_error", { 1, 1 } },
	{ "push_warning", { 1, 1 } },
	{ "var2str", { 1, 1 } },
	{ "str2var", { 1, 1 } },
	{ "var2bytes", { 1, 1 } },
	{ "bytes2var", { 1, 1 } },
	{ "range", { 1, 3 } },
	{ "load", { 1, 1 } },
	{ "inst2dict", { 1, 1 } },
	{ "dict2inst", { 1, 1 } },
	{ "validate_json", { 1, 1 } },
	{ "parse_json", { 1, 1 } },
	{ "to_json", { 1, 1 } },
	{ "hash", { 1, 1 } },
	{ "Color8", { 3, 4 } },
	{ "ColorN", { 1, 2 } },
	{ "print_stack", { 0, 0 } },
	{ "get_stack", { 0, 0 } },
	{ "instance_from_id", { 1, 1 } },
	{ "len", { 1, 1 } },
	{ "is_instance_valid", { 1, 1 } },
	{ "deep_equal", { 2, 2 } },
	{ "get_inst", { 1, 1 } }
};

String GDScriptDecomp_custom::get_function_name(int p_func) const {
	if (p_func >= 0 && p_func < functions.size()) {
		return functions[p_func];
	}
	return "";
}

int GDScriptDecomp_custom::get_function_count() const {
	return functions.size();
}

Pair<int, int> GDScriptDecomp_custom::get_function_arg_count(int p_func) const {
	String func_name = get_function_name(p_func);
	if (builtin_func_arg_elements.has(func_name)) {
		if (func_name == "var2bytes" || func_name == "bytes2var") {
			if (get_engine_ver_major() >= 3) {
				String minor = engine_version.get_slice(".", 1);
				int minor_int = minor.to_int();
				if (minor_int > 1) {
					return Pair<int, int>(1, 2);
				}
			}
		}
		return builtin_func_arg_elements[func_name];
	}
	return Pair<int, int>(0, 0);
}

int GDScriptDecomp_custom::get_token_max() const {
	return tokens.size() - 1;
}

int GDScriptDecomp_custom::get_function_index(const String &p_func) const {
	return functions.find(p_func);
}

GDScriptDecomp::GlobalToken GDScriptDecomp_custom::get_global_token(int p_token) const {
	int idx = p_token & TOKEN_MASK;
	if (idx < 0 || idx >= tokens.size()) {
		return G_TK_MAX;
	}
	return tokens[idx];
}

int GDScriptDecomp_custom::get_local_token_val(GDScriptDecomp::GlobalToken p_token) const {
	for (int i = 0; i < tokens.size(); i++) {
		if (tokens[i] == p_token) {
			return i;
		}
	}
	return -1;
}

GDScriptDecomp_custom::GDScriptDecomp_custom(Dictionary p_custom_def) {
	bytecode_version = p_custom_def.get("bytecode_version", 0);
	bytecode_rev = p_custom_def.get("bytecode_rev", "").operator String().hex_to_int();
	engine_ver_major = p_custom_def.get("engine_ver_major", 0);
	variant_ver_major = p_custom_def.get("variant_ver_major", 0);
	engine_version = p_custom_def.get("engine_version", "");
	max_engine_version = p_custom_def.get("max_engine_version", "");
	date = p_custom_def.get("date", "");
	parent = p_custom_def.get("parent", "").operator String().hex_to_int();
	Vector<String> token_names = p_custom_def.get("tk_names", Vector<String>());
	for (int i = 0; i < token_names.size(); i++) {
		const String &token_name = token_names[i];
		tokens.append(get_token_for_name(token_name));
	}
	functions = p_custom_def.get("func_names", Vector<String>());
}

GDScriptDecomp_custom *GDScriptDecomp_custom::_create_from_json(Dictionary p_custom_def) {
	if ((int)p_custom_def.get("bytecode_version", 0) == 0) {
		ERR_FAIL_V_MSG(nullptr, "Bytecode version is required");
	}
	if ((int)p_custom_def.get("engine_ver_major", 0) == 0) {
		ERR_FAIL_V_MSG(nullptr, "Engine version major is required");
	}
	if ((int)p_custom_def.get("variant_ver_major", 0) == 0) {
		ERR_FAIL_V_MSG(nullptr, "Variant version major is required");
	}
	if ((int)p_custom_def.get("bytecode_rev", "").operator String().hex_to_int() == 0) {
		ERR_FAIL_V_MSG(nullptr, "Bytecode revision is required");
	}
	if (p_custom_def.get("engine_version", "") == "") {
		ERR_FAIL_V_MSG(nullptr, "Engine version is required");
	}
	// if ((int)p_custom_def.get("parent", 0) == 0) {
	// 	return nullptr;
	// }
	if (p_custom_def.get("tk_names", Vector<String>()) == Vector<String>()) {
		ERR_FAIL_V_MSG(nullptr, "Tokens are required");
	}
	// if (!p_custom_def.has("func_names")) {
	// 	ERR_FAIL_V_MSG(nullptr, "Functions are required");
	// }
	// if (p_custom_def.get("date", "") == "") {
	// 	return nullptr;
	// }
	return memnew(GDScriptDecomp_custom(p_custom_def));
}

Ref<GDScriptDecomp_custom> GDScriptDecomp_custom::create_from_json(Dictionary p_custom_def) {
	return Ref<GDScriptDecomp_custom>(_create_from_json(p_custom_def));
}
