#include "core/io/marshalls.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#include "bytecode/bytecode_base.h"
#include "bytecode/bytecode_versions.h"
#include "bytecode/gdscript_tokenizer_compat.h"
#include "core/object/property_info.h"
#include "core/version.h"
#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_tokenizer.h"
#include "modules/gdscript/tests/gdscript_test_runner.h"
#include "test_common.h"
#include "utility/gdre_logger.h"
#include "utility/gdre_settings.h"
#include "utility/text_diff.h"
#include <compat/fake_gdscript.h>
#include <compat/resource_compat_text.h>
#include <compat/resource_loader_compat.h>

#include "core/version_generated.gen.h"
#include <modules/gdscript/gdscript_tokenizer_buffer.h>
#include <utility/common.h>
#include <utility/glob.h>

TEST_FORCE_LINK(test_bytecode)
namespace TestBytecode {

void test_fake_script();

Vector<String> get_gdscript2_0_scripts();

struct ScriptToRevision {
	const char *script;
	int revision;
};

static const ScriptToRevision tests[] = {
	{ "has_ord", 0x5565f55 },
	{ "has_lerp_angle", 0x6694c11 },
	{ "has_posmod", 0xa60f242 },
	{ "has_move_toward", 0xc00427a },
	{ "has_step_decimals", 0x620ec47 },
	{ "has_is_equal_approx", 0x7f7d97f },
	{ "has_smoothstep", 0x514a3fb },
	{ "has_do", 0x1a36141 },
	{ "has_push_error", 0x1a36141 },
	{ "has_puppetsync", 0xd6b31da },
	{ "has_typed", 0x8aab9a0 },
	{ "has_classname", 0xa3f1ee5 },
	{ "has_slavesync", 0x8e35d93 },
	// { "has_OS.alert_debug", 0x3ea6d9f },
	{ "has_get_stack", 0xa56d6ff },
	{ "has_is_instance_valid", 0xff1e7cf },
	{ "has_cartesian2polar", 0x054a2ac },
	{ "has_tau", 0x91ca725 },
	{ "has_wrapi", 0x216a8aa },
	{ "has_inverse_lerp", 0xd28da86 },
	{ "has_len", 0xc6120e7 },
	{ "has_is", 0x015d36d },
	{ "has_inf", 0x5e938f0 },
	{ "has_wc", 0xc24c739 },
	{ "has_match", 0xf8a7c46 },
	{ "has_to_json", 0x62273e5 },
	{ "has_path", 0x8b912d1 },
	{ "has_colorn", 0x85585c7 },
	{ "has_type_exists", 0x7124599 },
	{ "has_var2bytes", 0x23441ec },
	{ "has_pi", 0x6174585 },
	{ "has_color8", 0x64872ca },
	{ "has_bp", 0x7d2d144 },
	{ "has_onready", 0x30c1229 },
	{ "has_signal", 0x48f1d02 },
	// { "has_OS.alerts", 0x65d48d6 },
	{ "has_instance_from_id", 0xbe46be7 },
	{ "has_var2str", 0x2185c01 },
	{ "has_setget", 0xe82dc40 },
	{ "has_yield", 0x8cab401 },
	{ "has_hash", 0x703004f },
	{ "has_funcref", 0x31ce3c5 },
	{ nullptr, 0 },
};

static constexpr const char *test_unique_id_modulo = R"(
extends AnimationPlayer

func _ready() -> void :
	%BlinkTimer.timeout.connect(check_for_blink)
	var thingy = 10 % 3
	var thingy2 = 10 % thingy
	var thingy3 = thingy % 20
)";

static constexpr const char *test_indent = R"(
extends Object

func set_worldspawn_layers(new_worldspawn_layers: Array) -> void :
	if worldspawn_layers != new_worldspawn_layers:
		worldspawn_layers = new_worldspawn_layers

		for i in range(0, worldspawn_layers.size()):
			if not worldspawn_layers[i]:
				worldspawn_layers[i] = QodotWorldspawnLayer.new()

)";

static constexpr const char *test_indent_current = R"(
extends Object
func foo():
	var bar = 1;
	if bar == 1:
		print("bar is 1")
	else:
		print("bar is not 1")
	return bar
)";

static constexpr const char *test_multiline_string = R"(
static func find_or_null(arr: Array[Node], index: int = 0) -> Node:
	if (arr.is_empty()):
		push_error("""Node that is needed for Script-IDE not found.
Plugin will not work correctly.
This might be due to some other plugins or changes in the Engine.
Please report this to Script-IDE, so we can figure out a fix.""")
		return null
	return arr[index]
)";

// should pass on all versions of GDScript
static constexpr const char *test_reserved_word_as_accessor_name = R"(
extends Object

func _ready():
	var thingy = {}
	thingy["func"] = "bar"
	thingy["enum"] = "foo"
	thingy["preload"] = "foo"
	thingy["yield"] = "foo"
	thingy["sin"] = "foo"
	thingy["static"] = "foo"
	thingy["pass"] = "foo"
	foo.sin()
	print(thingy.func)
	print(thingy.enum)
	print(thingy.preload)
	print(thingy.yield)
	print(thingy.sin)
	print(thingy.static)
	print(thingy.pass)
)";

// clang-format off
	static constexpr const char *test_eof_newline = R"(
extends RefCounted
func _ready():
	pass
	)";
// clang-format on

inline void output_file(const String &path, const String &text) {
	gdre::ensure_dir(path.get_base_dir());
	auto fa = FileAccess::open(path, FileAccess::WRITE);
	if (fa.is_valid()) {
		fa->store_string(text);
		fa->flush();
		fa->close();
	}
}

inline void debug_output(const String &script_name, const String &helper_script_text_stripped, const String &decompiled_string_stripped) {
	auto diff = TextDiff::get_diff_with_header(script_name + "_original", script_name + "_decompiled", helper_script_text_stripped, decompiled_string_stripped);
	TextDiff::print_diff(diff);
	auto base_path = get_tmp_path().path_join(script_name.get_basename());
	output_file(base_path + ".diff", diff);
	output_file(base_path + ".decompiled.gd", decompiled_string_stripped);
}

inline String strip_script_text(const String &script_text) {
	return remove_comments(script_text).replace("\"\"\"", "\"").replace("'", "\"");
}

inline Vector<PropertyInfo> list_to_vector(const List<PropertyInfo> &list) {
	Vector<PropertyInfo> vector;
	for (auto &E : list) {
		vector.push_back(E);
	}
	return vector;
}

inline void test_script_binary(const String &script_name, const Vector<uint8_t> &bytecode, const String &helper_script_text, int revision, bool helper_script, bool no_text_equality_check, bool compare_whitespace = false) {
	auto decomp = GDScriptDecomp::create_decomp_for_commit(revision);
	CHECK(decomp.is_valid());
	auto result = decomp->test_bytecode(bytecode, false);
	CHECK(result == GDScriptDecomp::BYTECODE_TEST_PASS);
	if (helper_script && decomp->get_parent() != 0) {
		// Test our previously compiled bytecode against the parent
		auto parent = GDScriptDecomp::create_decomp_for_commit(decomp->get_parent());
		CHECK(parent.is_valid());
		// Can't test be46be7, the only thing that changed in this commit is the name of the function
		if (revision != 0xbe46be7 && revision != 0x2b64f73) {
			auto parent_result = parent->test_bytecode(bytecode, false);
			CHECK(parent_result == GDScriptDecomp::BYTECODE_TEST_FAIL);
		}
	}

	// test compiling the decompiled code
	Error err = decomp->decompile_buffer(bytecode);
	CHECK(err == OK);
	CHECK(decomp->get_error_message() == "");
	// no whitespace
	auto decompiled_string = decomp->get_script_text();
	auto helper_script_text_stripped = strip_script_text(helper_script_text);
	if (!helper_script_text_stripped.strip_edges().is_empty()) {
		CHECK(decompiled_string != "");
	}

	auto decompiled_string_stripped = strip_script_text(decompiled_string);

#if DEBUG_ENABLED
	if (!no_text_equality_check && gdre::remove_whitespace(decompiled_string_stripped) != gdre::remove_whitespace(helper_script_text_stripped)) {
		debug_output(script_name, helper_script_text_stripped, decompiled_string_stripped);
	}
#endif
	if (!no_text_equality_check) {
		CHECK_MESSAGE(gdre::remove_whitespace(decompiled_string_stripped) == gdre::remove_whitespace(helper_script_text_stripped), (String("No whitespace text diff failed: \n") + TextDiff::get_diff_with_header(script_name, script_name, decompiled_string_stripped, helper_script_text_stripped)));
	}
	if (compare_whitespace) {
		if (decompiled_string_stripped != helper_script_text_stripped) {
			debug_output(script_name, helper_script_text_stripped, decompiled_string_stripped);
		}
		CHECK(decompiled_string_stripped == helper_script_text_stripped);
	}
	auto recompiled_bytecode = decomp->compile_code_string(decompiled_string);
	CHECK(decomp->get_error_message() == "");
	CHECK(recompiled_bytecode.size() > 0);
	auto recompiled_result = decomp->test_bytecode(recompiled_bytecode, false);
	CHECK(recompiled_result == GDScriptDecomp::BYTECODE_TEST_PASS);
	err = decomp->test_bytecode_match(bytecode, recompiled_bytecode, !compare_whitespace, false);
#if DEBUG_ENABLED
	if (err) {
		debug_output(script_name, helper_script_text_stripped, decompiled_string_stripped);
	}
#endif

	CHECK(decomp->get_error_message() == "");
	if (script_name != "super" && revision == GDScriptDecompVersion::LATEST_GDSCRIPT_COMMIT) {
		// test with the latest GDScriptTokenizer
		auto reference_result = GDScriptTokenizerBuffer::parse_code_string(helper_script_text, GDScriptTokenizerBuffer::CompressMode::COMPRESS_ZSTD);
		CHECK(reference_result.size() > 0);
		err = decomp->test_bytecode_match(bytecode, reference_result, true, false);
		CHECK(decomp->get_error_message() == "");
		CHECK(err == OK);
		err = decomp->decompile_buffer(reference_result);
		CHECK(err == OK);
		String decompiled_ref = decomp->get_script_text();
		auto decompiled_ref_stripped = strip_script_text(decompiled_ref);
		if (!no_text_equality_check) {
			CHECK_MESSAGE(gdre::remove_whitespace(decompiled_ref_stripped) == gdre::remove_whitespace(helper_script_text_stripped), (String("No whitespace text diff failed: \n") + TextDiff::get_diff_with_header(script_name, script_name, decompiled_ref_stripped, helper_script_text_stripped)));
		}
		auto recompiled_ref = GDScriptTokenizerBuffer::parse_code_string(decompiled_ref, GDScriptTokenizerBuffer::CompressMode::COMPRESS_ZSTD);
		CHECK(decomp->get_error_message() == "");
		CHECK(recompiled_ref.size() > 0);
		err = decomp->test_bytecode_match(reference_result, recompiled_ref, true, false);
		CHECK(decomp->get_error_message() == "");
		CHECK(err == OK);
		if (script_name != "super") {
			auto buffer = GDScriptTokenizerBuffer();
			err = buffer.set_code_buffer(bytecode);
			REQUIRE(err == OK);
			auto recompiled_buffer = GDScriptTokenizerBuffer();
			err = recompiled_buffer.set_code_buffer(reference_result);
			REQUIRE(err == OK);
			auto token = buffer.scan();
			auto recompiled_token = recompiled_buffer.scan();
			CHECK(token.type == recompiled_token.type);
			while (token.type != GDScriptTokenizer::Token::TK_EOF) {
				token = buffer.scan();
				recompiled_token = recompiled_buffer.scan();
				if (token.type != recompiled_token.type) {
					debug_output(script_name, helper_script_text_stripped, decompiled_ref_stripped);
				}
				CHECK(token.type == recompiled_token.type);
			}
		}
	}

	{
		auto buffer = GDScriptTokenizerCompat::create_buffer_tokenizer(decomp.ptr(), bytecode);
		REQUIRE(buffer.is_valid());
		auto recompiled_buffer = GDScriptTokenizerCompat::create_buffer_tokenizer(decomp.ptr(), recompiled_bytecode);
		REQUIRE(recompiled_buffer.is_valid());
		auto token = buffer->scan();
		auto recompiled_token = recompiled_buffer->scan();
		CHECK(token.type == recompiled_token.type);
		while (token.type != GDScriptDecomp::G_TK_EOF) {
			token = buffer->scan();
			recompiled_token = recompiled_buffer->scan();
			CHECK(token.type == recompiled_token.type);
		}
	}

	CHECK(err == OK);

	Ref<FakeGDScript> fake_script = memnew(FakeGDScript);
	fake_script->set_override_bytecode_revision(revision);
	CHECK(fake_script->get_override_bytecode_revision() == revision);
	fake_script->set_source_code(helper_script_text);
	CHECK(fake_script->is_loaded());
	CHECK(fake_script->get_error_message() == "");
	fake_script->load_binary_tokens(bytecode);
	CHECK(fake_script->is_loaded());
	CHECK(fake_script->get_error_message() == "");
	CHECK(fake_script->get_override_bytecode_revision() == revision);

	// if (decomp->get_bytecode_version() <= GDScriptDecomp::GDSCRIPT_2_0_VERSION) {
	// 	auto tokenizer = GDScriptTokenizerBufferCompat(decomp.ptr());
	// 	tokenizer.set_code_buffer(bytecode);
	// 	auto token = tokenizer.scan();
	// 	while (token.type != GDScriptDecomp::G_TK_EOF) {
	// 		print_line(vformat("Token: '%s', Line: %d, Column: %d, Indent: %d, Function: %s, Error: %s", GDScriptTokenizerBufferCompat::get_token_name(token.type), token.line, token.col, token.current_indent, token.func_name, token.error));
	// 		token = tokenizer.scan();
	// 	}
	// 	bool thignas = false;
	// }
}

inline void test_script_text(const String &script_name, const String &helper_script_text, int revision, bool helper_script, bool no_text_equality_check, bool compare_whitespace = false) {
	auto decomp = GDScriptDecomp::create_decomp_for_commit(revision);
	CHECK(decomp.is_valid());
	auto bytecode = decomp->compile_code_string(helper_script_text);
	auto compile_error_message = decomp->get_error_message();
	CHECK(compile_error_message == "");
	CHECK(bytecode.size() > 0);
	test_script_binary(script_name, bytecode, helper_script_text, revision, helper_script, no_text_equality_check, compare_whitespace);
}

inline void test_script(const String &helper_script_path, int revision, bool helper_script, bool no_text_equality_check) {
	// tests are located in modules/gdsdecomp/helpers
	auto da = DirAccess::create_for_path(helper_script_path);
	CHECK(da.is_valid());
	CHECK(da->file_exists(helper_script_path));
	Error err;
	auto helper_script_text = FileAccess::get_file_as_string(helper_script_path, &err);
	CHECK(err == OK);
	CHECK(helper_script_text != "");
	auto script_name = helper_script_path.get_file().get_basename();
	test_script_text(script_name, helper_script_text, revision, helper_script, no_text_equality_check);
}

TEST_CASE("[GDSDecomp][Bytecode] GDScriptTokenizer outputs bytecode_version == LATEST_GDSCRIPT_VERSION") {
	auto buf = GDScriptTokenizerBuffer::parse_code_string("", GDScriptTokenizerBuffer::CompressMode::COMPRESS_NONE);
	CHECK(buf.size() >= 8);
	int this_ver = decode_uint32(&buf[4]);
	CHECK(this_ver == GDScriptDecomp::LATEST_GDSCRIPT_VERSION);
}

TEST_CASE("[GDSDecomp][Bytecode] Bytecode for current engine version has same number of tokens") {
	auto decomp = GDScriptDecomp::create_decomp_for_version(vformat("%d.%d.%d", GODOT_VERSION_MAJOR, GODOT_VERSION_MINOR, GODOT_VERSION_PATCH));
	CHECK(decomp.is_valid());
	CHECK(decomp->get_token_max() == GDScriptTokenizerBuffer::Token::TK_MAX);
}

TEST_CASE("[GDSDecomp][Bytecode][GDScript1.0] Compiling Helper Scripts") {
	for (int i = 0; tests[i].script != nullptr; i++) {
		auto &script_to_revision = tests[i];
		String sub_case_name = vformat("Testing compiling script %s, revision %07x", String(script_to_revision.script), script_to_revision.revision);
		SUBCASE(sub_case_name.utf8().get_data()) {
			// tests are located in modules/gdsdecomp/helpers
			auto helpers_path = get_gdsdecomp_helpers_path();
			auto da = DirAccess::open(helpers_path);
			CHECK(da.is_valid());
			CHECK(da->dir_exists(helpers_path));
			auto helper_script_path = helpers_path.path_join(script_to_revision.script) + ".gd";
			test_script(helper_script_path, script_to_revision.revision, true, false);
		}
	}
}

TEST_CASE("[GDSDecomp][Bytecode][GDScript2.0] Compiling GDScript Tests") {
	REQUIRE(GDRESettings::get_singleton());
	auto gdscript_test_scripts = get_gdscript2_0_scripts();

	for (int64_t i = 0; i < gdscript_test_scripts.size(); i++) {
		auto &script_path = gdscript_test_scripts[i];
		auto sub_case_name = vformat("Testing compiling script %s", script_path);
		SUBCASE(sub_case_name.utf8().get_data()) {
			test_script(script_path, 0x77af6ca, false, true);
			test_script(script_path, GDScriptDecompVersion::LATEST_GDSCRIPT_COMMIT, false, true);
		}
	}
}

TEST_CASE("[GDSDecomp][Bytecode][GDScript2.0] Test indenting") {
	test_script_text("test_indent_current", test_indent_current, GDScriptDecompVersion::LATEST_GDSCRIPT_COMMIT, false, false, true);
}

TEST_CASE("[GDSDecomp][Bytecode][GDScript2.0] Test multi-line strings") {
	test_script_text("test_multiline_string", test_multiline_string, GDScriptDecompVersion::LATEST_GDSCRIPT_COMMIT, false, false, true);
}

TEST_CASE("[GDSDecomp][Bytecode] Test indenting") {
	test_script_text("test_indent", test_indent, 0xa7aad78, false, false, true);
}

TEST_CASE("[GDSDecomp][Bytecode][GDScript2.0] Test unique_id modulo operator") {
	test_script_text("test_unique_id_modulo", test_unique_id_modulo, 0x77af6ca, false, false, true);
	test_script_text("test_unique_id_modulo", test_unique_id_modulo, GDScriptDecompVersion::LATEST_GDSCRIPT_COMMIT, false, false, true);
}

TEST_CASE("[GDSDecomp][Bytecode] Test sample GDScript bytecode") {
	Vector<String> versions = DirAccess::get_directories_at(get_test_scripts_path());
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		auto decomp = GDScriptDecomp::create_decomp_for_version(version);
		CHECK(decomp.is_valid());
		int revision = decomp->get_bytecode_rev();
		String test_dir = get_test_scripts_path().path_join(version).path_join("code");
		REQUIRE(DirAccess::exists(test_dir));
		Vector<String> files = gdre::get_recursive_dir_list(test_dir, { "*.gdc" });
		String output_dir = get_tmp_path().path_join(version).path_join("code");
		for (const String &file : files) {
			auto sub_case_name = vformat("Testing compiling script %s, version %s", file, version);
			SUBCASE(sub_case_name.utf8().get_data()) {
				String original_file = file.get_basename() + ".gd";
				REQUIRE(FileAccess::exists(file));
				REQUIRE(FileAccess::exists(original_file));

				Vector<uint8_t> bytecode = FileAccess::get_file_as_bytes(file);
				String original_script_text = FileAccess::get_file_as_string(original_file);
				auto compiled_bytecode = decomp->compile_code_string(original_script_text);
				CHECK(decomp->get_error_message() == "");
				CHECK(compiled_bytecode.size() > 0);
				CHECK(decomp->test_bytecode_match(bytecode, compiled_bytecode, false, false) == OK);
				test_script_binary(original_file, bytecode, original_script_text, revision, false, true);
			}
		}
	}
}

inline void simple_pass_fail_test(const String &script_name, const String &helper_script_text, int revision, bool expect_fail) {
	SUBCASE(vformat("Testing %s, revision %07x", script_name, revision).utf8().get_data()) {
		auto decomp = GDScriptDecomp::create_decomp_for_commit(revision);
		CHECK(decomp.is_valid());
		auto bytecode = decomp->compile_code_string(helper_script_text);
		CHECK(decomp->get_error_message() == "");
		CHECK(bytecode.size() > 0);
		auto result = decomp->test_bytecode(bytecode, false);
		if (!expect_fail) {
			CHECK(decomp->get_error_message() == "");
			CHECK(result == GDScriptDecomp::BYTECODE_TEST_PASS);
		} else {
			CHECK(result == GDScriptDecomp::BYTECODE_TEST_FAIL);
		}
	}
}

TEST_CASE("[GDSDecomp][Bytecode] Test reserved words as global function names") {
	// get all the decomp versions for 2.x and 3.x (GDScript 1.0)

	Vector<GDScriptDecompVersion> versions = GDScriptDecompVersion::get_decomp_versions(true, 0);

	static constexpr const char *test_global_function_name = R"(
	extends Object

	func %s():
		pass
	)";
	static constexpr const char *func_call_fragment = R"(
	func _ready():
		%s()
	)";

	// TODO: We should test all reserved words to see if they're being used in a function declaration in _test_bytecode
	Vector<Pair<bool, String>> keywords_to_test = {
		// { true, "func" },
		{ true, "enum" },
		// { false, "preload" },
		// { false, "yield" },
		// { false, "sin" },
		{ true, "static" },
		// { false, "pass" },
	};

	for (int i = 0; i < keywords_to_test.size(); i++) {
		auto &keyword = keywords_to_test[i];
		String test_name = keyword.second;
		String test_script = vformat(test_global_function_name, test_name);
		if (keyword.first) {
			test_script += vformat(func_call_fragment, test_name);
		}
		for (const GDScriptDecompVersion &version : versions) {
			int revision = version.commit;
			bool expect_fail = version.bytecode_version >= GDScriptDecomp::GDSCRIPT_2_0_VERSION;

			simple_pass_fail_test(test_name, test_script, revision, expect_fail);
		}
	}
}

TEST_CASE("[GDSDecomp][Bytecode] Test reserved words as member function names") {
	static constexpr const char *test_member_function_name = R"(
	extends Object

	class test_class:
		func %s():
			pass
	)";
	static constexpr const char *func_call_fragment = R"(
	func _ready():
		var test = test_class.new()
		test.%s()
	)";

	auto versions = GDScriptDecompVersion::get_decomp_versions(true, 0);
	Vector<Pair<bool, String>> keywords_to_test = {
		// { true, "func" },
		{ true, "enum" },
		// { false, "preload" },
		// { false, "yield" },
		// { false, "sin" },
		{ true, "static" },
		// { false, "pass" },
	};

	for (int i = 0; i < keywords_to_test.size(); i++) {
		auto &keyword = keywords_to_test[i];
		String test_name = keyword.second;
		String test_script = vformat(test_member_function_name, test_name);
		if (keyword.first) {
			test_script += vformat(func_call_fragment, test_name);
		}
		for (const GDScriptDecompVersion &version : versions) {
			int revision = version.commit;
			bool expect_fail = version.bytecode_version >= GDScriptDecomp::GDSCRIPT_2_0_VERSION;

			simple_pass_fail_test(test_name, test_script, revision, expect_fail);
		}
	}
}

TEST_CASE("[GDSDecomp][Bytecode] Test reserved words as accessor names") {
	auto versions = GDScriptDecompVersion::get_decomp_versions(true, 0);
	for (const GDScriptDecompVersion &version : versions) {
		int revision = version.commit;
		simple_pass_fail_test("all", test_reserved_word_as_accessor_name, revision, false);
	}
}

TEST_CASE("[GDSDecomp][Bytecode][Create] Test creating custom decomp") {
	REQUIRE(GDRESettings::get_singleton());
	auto cwd = GDRESettings::get_singleton()->get_cwd();
	String gdscript_tests_path = get_gdscript_tests_path();
	auto gdscript_test_scripts = Glob::rglob(gdscript_tests_path.path_join("**/*.gd"), true);
	auto gdscript_test_error_scripts = Vector<String>();
	for (int i = 0; i < gdscript_test_scripts.size(); i++) {
		// remove any that contain ".notest." or "/error/"
		auto script_path = gdscript_test_scripts[i].trim_prefix(cwd + "/");
		if (script_path.contains(".notest.") || script_path.contains("error") || script_path.contains("completion")) {
			gdscript_test_error_scripts.push_back(script_path);
			gdscript_test_scripts.erase(gdscript_test_scripts[i]);
			i--;
		} else {
			gdscript_test_scripts.write[i] = script_path;
		}
	}

	GDScriptDecompVersion ver = GDScriptDecompVersion::create_derived_version_from_custom_def(GDScriptDecompVersion::LATEST_GDSCRIPT_COMMIT, Dictionary());
	CHECK(!ver.name.is_empty());
	int revision = GDScriptDecompVersion::register_decomp_version_custom(ver.custom);
	CHECK(revision != 0);
	Ref<GDScriptDecomp> decomp = GDScriptDecompVersion::create_decomp_for_commit(revision);
	CHECK(decomp.is_valid());

	for (int64_t i = 0; i < gdscript_test_scripts.size(); i++) {
		auto &script_path = gdscript_test_scripts[i];
		auto sub_case_name = vformat("Testing compiling script %s", script_path);
		SUBCASE(sub_case_name.utf8().get_data()) {
			test_script(script_path, revision, false, true);
		}
	}
}

TEST_CASE("[GDSDecomp][Bytecode][EOFNewline] Test indented newline at EOF") {
	auto versions = GDScriptDecompVersion::get_decomp_versions(true, 0);
	for (const GDScriptDecompVersion &version : versions) {
		int revision = version.commit;
		test_script_text("indented_newline_at_eof", test_eof_newline, revision, false, true, false);
	}
}
TEST_CASE("[GDSDecomp][FakeScript] test FakeScript") {
	test_fake_script();
}
void test_individual_script(const String &script_path, const String &helper_script_text) {
	// disable this for now
	CoreGlobals::print_error_enabled = false;
	Ref<FakeGDScript> fake_script = memnew(FakeGDScript);
	fake_script->set_override_bytecode_revision(GDScriptDecompVersion::LATEST_GDSCRIPT_COMMIT);
	fake_script->load_source_code(script_path);
	Error err = fake_script->reload(false);
	CHECK(err == OK);

	auto cwd = GDRESettings::get_singleton()->get_cwd();

	String local_path = cwd.path_join(script_path); //ProjectSettings::get_singleton()->localize_path(script_path);

	Ref<GDScript> real_script = memnew(GDScript);
	real_script->set_path(local_path);
	err = real_script->load_source_code(local_path);
	CHECK(err == OK);
	err = real_script->reload(false);
	// TODO: Figure out why the real loader fails to load some scripts
	if (err != OK) {
		CoreGlobals::print_error_enabled = true;
		return;
	}
	CHECK(err == OK);
	List<PropertyInfo> decompiled_list;
	fake_script->get_script_property_list(&decompiled_list);
	// Vector<PropertyInfo> decompiled_vector = list_to_vector(decompiled_list);
	HashMap<String, PropertyInfo> decompiled_map;
	for (auto &E : decompiled_list) {
		decompiled_map[E.name] = E;
	}
	List<PropertyInfo> real_list;
	real_script->get_script_property_list(&real_list);
	Vector<PropertyInfo> real_vector = list_to_vector(real_list);

	for (int i = 0; i < real_vector.size(); i++) {
		if (((real_vector[i].usage & PropertyUsageFlags::PROPERTY_USAGE_STORAGE) == 0) || real_vector[i].usage & PROPERTY_USAGE_GROUP || real_vector[i].usage & PROPERTY_USAGE_CATEGORY || real_vector[i].usage & PROPERTY_USAGE_SUBGROUP) {
			continue;
		}
		CHECK(decompiled_map.has(real_vector[i].name));
	}
	CoreGlobals::print_error_enabled = true;
}

static bool generate_class_index_recursive(const String &p_dir) {
	Error err = OK;
	Ref<DirAccess> dir(DirAccess::open(p_dir, &err));

	if (err != OK) {
		return false;
	}

	String current_dir = dir->get_current_dir();

	dir->list_dir_begin();
	String next = dir->get_next();

	StringName gdscript_name = GDScriptLanguage::get_singleton()->get_name();
	while (!next.is_empty()) {
		if (dir->current_is_dir()) {
			if (next == "." || next == ".." || next == "completion" || next == "lsp") {
				next = dir->get_next();
				continue;
			}
			if (!generate_class_index_recursive(current_dir.path_join(next))) {
				return false;
			}
		} else {
			if (!next.ends_with(".gd")) {
				next = dir->get_next();
				continue;
			}
			String base_type;
			String source_file = current_dir.path_join(next);
			bool is_abstract = false;
			bool is_tool = false;
			String class_name = GDScriptLanguage::get_singleton()->get_global_class_name(source_file, &base_type, nullptr, &is_abstract, &is_tool);
			if (class_name.is_empty()) {
				next = dir->get_next();
				continue;
			}
			ERR_FAIL_COND_V_MSG(ScriptServer::is_global_class(class_name), false,
					"Class name '" + class_name + "' from " + source_file + " is already used in " + ScriptServer::get_global_class_path(class_name));

			ScriptServer::add_global_class(class_name, base_type, gdscript_name, source_file, is_abstract, is_tool);
		}

		next = dir->get_next();
	}

	dir->list_dir_end();

	return true;
}

bool generate_class_index(const String &source_dir) {
	Error err = OK;
	Ref<DirAccess> dir(DirAccess::open(source_dir, &err));

	ERR_FAIL_COND_V_MSG(err != OK, false, "Could not open specified test directory.");

	return generate_class_index_recursive(dir->get_current_dir());
}
void test_fake_script() {
	GDScriptTests::GDScriptTestRunner runner(get_gdscript_tests_path(), false, false);
	generate_class_index(get_gdscript_tests_path());
	GDRESettings::get_singleton()->_set_version_override(GODOT_VERSION_FULL_CONFIG);
	String project_path = GDRESettings::get_singleton()->get_project_path();
	GDRESettings::get_singleton()->set_project_path(get_gdscript_tests_path());
	Error err = GDRESettings::get_singleton()->load_project({ get_gdscript_tests_path() });
	CHECK(err == OK);
	auto gdscript_test_scripts = get_gdscript2_0_scripts();
	for (int i = 0; i < gdscript_test_scripts.size(); i++) {
		auto script_path = gdscript_test_scripts[i];
		auto sub_case_name = vformat("Testing compiling script %s", script_path);
		String script_text = FileAccess::get_file_as_string(script_path);
		test_individual_script(script_path, script_text);
	}
	GDRESettings::get_singleton()->unload_project();
	GDRESettings::get_singleton()->_set_version_override("");
	GDRESettings::get_singleton()->set_project_path(project_path);
}

Vector<String> get_gdscript2_0_scripts() {
	REQUIRE(GDRESettings::get_singleton());
	auto cwd = GDRESettings::get_singleton()->get_cwd();
	String gdscript_tests_path = get_gdscript_tests_path();
	auto gdscript_test_scripts = Glob::rglob(gdscript_tests_path.path_join("**/*.gd"), true);
	auto gdscript_test_error_scripts = Vector<String>();
	for (int i = 0; i < gdscript_test_scripts.size(); i++) {
		// remove any that contain ".notest." or "/error/"
		auto script_path = gdscript_test_scripts[i].trim_prefix(cwd + "/");
		if (script_path.contains(".notest.") || script_path.contains("error") || script_path.contains("completion")) {
			gdscript_test_error_scripts.push_back(script_path);
			gdscript_test_scripts.erase(gdscript_test_scripts[i]);
			i--;
		} else {
			gdscript_test_scripts.write[i] = script_path;
		}
	}
	return gdscript_test_scripts;
}
} // namespace TestBytecode
