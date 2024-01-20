/*************************************************************************/
/*  bytecode_base.cpp                                                    */
/*************************************************************************/

#include "bytecode_base.h"
#include "compat/file_access_encrypted_v3.h"
#include "compat/variant_writer_compat.h"

#include "core/config/engine.h"
#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/image.h"
#include "core/io/marshalls.h"

#include <limits.h>

void GDScriptDecomp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("decompile_byte_code", "path"), &GDScriptDecomp::decompile_byte_code);
	ClassDB::bind_method(D_METHOD("decompile_byte_code_encrypted", "path", "key"), &GDScriptDecomp::decompile_byte_code_encrypted);

	ClassDB::bind_method(D_METHOD("get_script_text"), &GDScriptDecomp::get_script_text);
	ClassDB::bind_method(D_METHOD("get_error_message"), &GDScriptDecomp::get_error_message);
}

void GDScriptDecomp::_ensure_space(String &p_code) {
	if (!p_code.ends_with(" ")) {
		p_code += String(" ");
	}
}

Error GDScriptDecomp::decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key) {
	Vector<uint8_t> bytecode;
	Error err = get_buffer_encrypted(p_path, get_engine_ver_major(), p_key, bytecode);
	if (err != OK) {
		if (err == ERR_BUG) {
			error_message = RTR("FAE doesn't exist...???");
		} else if (err == ERR_UNAUTHORIZED) {
			error_message = RTR("Encryption Error");
		} else {
			error_message = RTR("File Error");
		}
		ERR_FAIL_V_MSG(err, error_message);
	}
	error_message = RTR("No error");
	return decompile_buffer(bytecode);
}

Error GDScriptDecomp::decompile_byte_code(const String &p_path) {
	Vector<uint8_t> bytecode;

	bytecode = FileAccess::get_file_as_bytes(p_path);

	error_message = RTR("No error");

	return decompile_buffer(bytecode);
}

Error GDScriptDecomp::get_buffer_encrypted(const String &p_path, int engine_ver_major, Vector<uint8_t> p_key, Vector<uint8_t> &bytecode) {
	Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(fa.is_null(), ERR_FILE_CANT_OPEN);

	// Godot v3 only encrypted the scripts and used a different format with different header fields,
	// So we need to use an older version of FAE to access them
	if (engine_ver_major == 3) {
		Ref<FileAccessEncryptedv3> fae;
		fae.instantiate();
		ERR_FAIL_COND_V(fae.is_null(), ERR_BUG);

		Error err = fae->open_and_parse(fa, p_key, FileAccessEncryptedv3::MODE_READ);
		ERR_FAIL_COND_V(err != OK, ERR_UNAUTHORIZED);

		bytecode.resize(fae->get_length());
		fae->get_buffer(bytecode.ptrw(), bytecode.size());
	} else {
		Ref<FileAccessEncrypted> fae;
		fae.instantiate();
		ERR_FAIL_COND_V(fae.is_null(), ERR_BUG);

		Error err = fae->open_and_parse(fa, p_key, FileAccessEncrypted::MODE_READ);
		ERR_FAIL_COND_V(err != OK, ERR_UNAUTHORIZED);

		bytecode.resize(fae->get_length());
		fae->get_buffer(bytecode.ptrw(), bytecode.size());
	}
	return OK;
}

String GDScriptDecomp::get_script_text() {
	return script_text;
}

String GDScriptDecomp::get_error_message() {
	return error_message;
}

String GDScriptDecomp::get_constant_string(Vector<Variant> &constants, uint32_t constId) {
	String constString;
	Error err = VariantWriterCompat::write_to_string(constants[constId], constString, get_variant_ver_major());
	ERR_FAIL_COND_V(err, "");
	if (constants[constId].get_type() == Variant::Type::STRING) {
		constString = constString.replace("\n", "\\n");
	}
	return constString;
}

Error GDScriptDecomp::get_ids_consts_tokens(const Vector<uint8_t> &p_buffer, int bytecode_version, Vector<StringName> &identifiers, Vector<Variant> &constants, Vector<uint32_t> &tokens, VMap<uint32_t, uint32_t> lines) {
	const uint8_t *buf = p_buffer.ptr();
	int total_len = p_buffer.size();
	ERR_FAIL_COND_V(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', ERR_INVALID_DATA);

	int version = decode_uint32(&buf[4]);
	if (version != bytecode_version) {
		ERR_FAIL_COND_V(version > bytecode_version, ERR_INVALID_DATA);
	}
	int identifier_count = decode_uint32(&buf[8]);
	int constant_count = decode_uint32(&buf[12]);
	int line_count = decode_uint32(&buf[16]);
	int token_count = decode_uint32(&buf[20]);

	const uint8_t *b = &buf[24];
	total_len -= 24;

	identifiers.resize(identifier_count);
	for (int i = 0; i < identifier_count; i++) {
		int len = decode_uint32(b);
		ERR_FAIL_COND_V(len > total_len, ERR_INVALID_DATA);
		b += 4;
		Vector<uint8_t> cs;
		cs.resize(len);
		for (int j = 0; j < len; j++) {
			cs.write[j] = b[j] ^ 0xb6;
		}

		cs.write[cs.size() - 1] = 0;
		String s;
		s.parse_utf8((const char *)cs.ptr());
		b += len;
		total_len -= len + 4;
		identifiers.write[i] = s;
	}

	constants.resize(constant_count);
	for (int i = 0; i < constant_count; i++) {
		Variant v;
		int len;
		Error err = VariantDecoderCompat::decode_variant_compat(get_variant_ver_major(), v, b, total_len, &len);
		if (err) {
			error_message = RTR("Invalid constant");
			return err;
		}
		b += len;
		total_len -= len;
		constants.write[i] = v;
	}

	ERR_FAIL_COND_V(line_count * 8 > total_len, ERR_INVALID_DATA);

	for (int i = 0; i < line_count; i++) {
		uint32_t token = decode_uint32(b);
		b += 4;
		uint32_t linecol = decode_uint32(b);
		b += 4;

		lines.insert(token, linecol);
		total_len -= 8;
	}
	tokens.resize(token_count);
	for (int i = 0; i < token_count; i++) {
		ERR_FAIL_COND_V(total_len < 1, ERR_INVALID_DATA);

		if ((*b) & 0x80) { //BYTECODE_MASK, little endian always
			ERR_FAIL_COND_V(total_len < 4, ERR_INVALID_DATA);

			tokens.write[i] = decode_uint32(b) & ~0x80;
			b += 4;
		} else {
			tokens.write[i] = *b;
			b += 1;
			total_len--;
		}
	}
	return OK;
}

int GDScriptDecomp::read_bytecode_version(const String &p_path) {
	Vector<uint8_t> p_buffer;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), -1, "Cannot open file!");
	p_buffer = f->get_buffer(24);
	ERR_FAIL_COND_V_MSG(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', -1, "Invalid gdscript!");
	const uint8_t *buf = p_buffer.ptr();
	int version = decode_uint32(&buf[4]);
	return version;
}

int GDScriptDecomp::read_bytecode_version_encrypted(const String &p_path, int engine_ver_major, Vector<uint8_t> p_key) {
	Vector<uint8_t> p_buffer;
	Error err = get_buffer_encrypted(p_path, engine_ver_major, p_key, p_buffer);
	ERR_FAIL_COND_V_MSG(err != OK, -1, "Cannot open file!");
	ERR_FAIL_COND_V_MSG(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', -1, "Invalid gdscript!");
	const uint8_t *buf = p_buffer.ptr();
	int version = decode_uint32(&buf[4]);
	return version;
}

Error GDScriptDecomp::decompile_buffer(Vector<uint8_t> p_buffer) {
	//Cleanup
	script_text = String();

	//Load bytecode
	Vector<StringName> identifiers;
	Vector<Variant> constants;
	VMap<uint32_t, uint32_t> lines;
	Vector<uint32_t> tokens;
	int bytecode_version = get_bytecode_version();
	int variant_ver_major = get_variant_ver_major();
	int engine_ver_major = get_engine_ver_major();
	int FUNC_MAX = get_function_count();

	const uint8_t *buf = p_buffer.ptr();
	int total_len = p_buffer.size();
	ERR_FAIL_COND_V(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', ERR_INVALID_DATA);

	int version = decode_uint32(&buf[4]);
	ERR_FAIL_COND_V(version != get_bytecode_version(), ERR_INVALID_DATA);
	Error err = get_ids_consts_tokens(p_buffer, bytecode_version, identifiers, constants, tokens, lines);
	ERR_FAIL_COND_V(err != OK, err);

	//Decompile script
	String line;
	int indent = 0;

	GlobalToken prev_token = G_TK_NEWLINE;
	for (int i = 0; i < tokens.size(); i++) {
		GlobalToken curr_token = get_global_token(tokens[i] & TOKEN_MASK);
		switch (curr_token) {
			case G_TK_EMPTY: {
				//skip
			} break;
			case G_TK_IDENTIFIER: {
				uint32_t identifier = tokens[i] >> TOKEN_BITS;
				ERR_FAIL_COND_V(identifier >= (uint32_t)identifiers.size(), ERR_INVALID_DATA);
				line += String(identifiers[identifier]);
			} break;
			case G_TK_CONSTANT: {
				uint32_t constant = tokens[i] >> TOKEN_BITS;
				ERR_FAIL_COND_V(constant >= (uint32_t)constants.size(), ERR_INVALID_DATA);
				line += get_constant_string(constants, constant);
			} break;
			case G_TK_SELF: {
				line += "self";
			} break;
			case G_TK_BUILT_IN_TYPE: {
				line += VariantDecoderCompat::get_variant_type_name(tokens[i] >> TOKEN_BITS, variant_ver_major);
			} break;
			case G_TK_BUILT_IN_FUNC: {
				ERR_FAIL_COND_V(tokens[i] >> TOKEN_BITS >= FUNC_MAX, ERR_INVALID_DATA);
				line += get_function_name(tokens[i] >> TOKEN_BITS);
			} break;
			case G_TK_OP_IN: {
				_ensure_space(line);
				line += "in ";
			} break;
			case G_TK_OP_EQUAL: {
				_ensure_space(line);
				line += "== ";
			} break;
			case G_TK_OP_NOT_EQUAL: {
				_ensure_space(line);
				line += "!= ";
			} break;
			case G_TK_OP_LESS: {
				_ensure_space(line);
				line += "< ";
			} break;
			case G_TK_OP_LESS_EQUAL: {
				_ensure_space(line);
				line += "<= ";
			} break;
			case G_TK_OP_GREATER: {
				_ensure_space(line);
				line += "> ";
			} break;
			case G_TK_OP_GREATER_EQUAL: {
				_ensure_space(line);
				line += ">= ";
			} break;
			case G_TK_OP_AND: {
				_ensure_space(line);
				line += "and ";
			} break;
			case G_TK_OP_OR: {
				_ensure_space(line);
				line += "or ";
			} break;
			case G_TK_OP_NOT: {
				_ensure_space(line);
				line += "not ";
			} break;
			case G_TK_OP_ADD: {
				_ensure_space(line);
				line += "+ ";
			} break;
			case G_TK_OP_SUB: {
				if (prev_token != G_TK_NEWLINE)
					_ensure_space(line);
				line += "- ";
				//TODO: do not add space after unary "-"
			} break;
			case G_TK_OP_MUL: {
				_ensure_space(line);
				line += "* ";
			} break;
			case G_TK_OP_DIV: {
				_ensure_space(line);
				line += "/ ";
			} break;
			case G_TK_OP_MOD: {
				_ensure_space(line);
				line += "% ";
			} break;
			case G_TK_OP_SHIFT_LEFT: {
				_ensure_space(line);
				line += "<< ";
			} break;
			case G_TK_OP_SHIFT_RIGHT: {
				_ensure_space(line);
				line += ">> ";
			} break;
			case G_TK_OP_ASSIGN: {
				_ensure_space(line);
				line += "= ";
			} break;
			case G_TK_OP_ASSIGN_ADD: {
				_ensure_space(line);
				line += "+= ";
			} break;
			case G_TK_OP_ASSIGN_SUB: {
				_ensure_space(line);
				line += "-= ";
			} break;
			case G_TK_OP_ASSIGN_MUL: {
				_ensure_space(line);
				line += "*= ";
			} break;
			case G_TK_OP_ASSIGN_DIV: {
				_ensure_space(line);
				line += "/= ";
			} break;
			case G_TK_OP_ASSIGN_MOD: {
				_ensure_space(line);
				line += "%= ";
			} break;
			case G_TK_OP_ASSIGN_SHIFT_LEFT: {
				_ensure_space(line);
				line += "<<= ";
			} break;
			case G_TK_OP_ASSIGN_SHIFT_RIGHT: {
				_ensure_space(line);
				line += ">>= ";
			} break;
			case G_TK_OP_ASSIGN_BIT_AND: {
				_ensure_space(line);
				line += "&= ";
			} break;
			case G_TK_OP_ASSIGN_BIT_OR: {
				_ensure_space(line);
				line += "|= ";
			} break;
			case G_TK_OP_ASSIGN_BIT_XOR: {
				_ensure_space(line);
				line += "^= ";
			} break;
			case G_TK_OP_BIT_AND: {
				_ensure_space(line);
				line += "& ";
			} break;
			case G_TK_OP_BIT_OR: {
				_ensure_space(line);
				line += "| ";
			} break;
			case G_TK_OP_BIT_XOR: {
				_ensure_space(line);
				line += "^ ";
			} break;
			case G_TK_OP_BIT_INVERT: {
				_ensure_space(line);
				line += "~ ";
			} break;
			//case G_TK_OP_PLUS_PLUS: {
			//	line += "++";
			//} break;
			//case G_TK_OP_MINUS_MINUS: {
			//	line += "--";
			//} break;
			case G_TK_CF_IF: {
				if (prev_token != G_TK_NEWLINE)
					_ensure_space(line);
				line += "if ";
			} break;
			case G_TK_CF_ELIF: {
				line += "elif ";
			} break;
			case G_TK_CF_ELSE: {
				if (prev_token != G_TK_NEWLINE)
					_ensure_space(line);
				line += "else ";
			} break;
			case G_TK_CF_FOR: {
				line += "for ";
			} break;
			case G_TK_CF_WHILE: {
				line += "while ";
			} break;
			case G_TK_CF_BREAK: {
				line += "break";
			} break;
			case G_TK_CF_CONTINUE: {
				line += "continue";
			} break;
			case G_TK_CF_PASS: {
				line += "pass";
			} break;
			case G_TK_CF_RETURN: {
				line += "return ";
			} break;
			case G_TK_CF_MATCH: {
				line += "match ";
			} break;
			case G_TK_PR_FUNCTION: {
				line += "func ";
			} break;
			case G_TK_PR_CLASS: {
				line += "class ";
			} break;
			case G_TK_PR_CLASS_NAME: {
				line += "class_name ";
			} break;
			case G_TK_PR_EXTENDS: {
				if (prev_token != G_TK_NEWLINE)
					_ensure_space(line);
				line += "extends ";
			} break;
			case G_TK_PR_IS: {
				_ensure_space(line);
				line += "is ";
			} break;
			case G_TK_PR_ONREADY: {
				line += "onready ";
			} break;
			case G_TK_PR_TOOL: {
				line += "tool ";
			} break;
			case G_TK_PR_STATIC: {
				line += "static ";
			} break;
			case G_TK_PR_EXPORT: {
				line += "export ";
			} break;
			case G_TK_PR_SETGET: {
				line += " setget ";
			} break;
			case G_TK_PR_CONST: {
				line += "const ";
			} break;
			case G_TK_PR_VAR: {
				if (line != String() && prev_token != G_TK_PR_ONREADY)
					line += " ";
				line += "var ";
			} break;
			case G_TK_PR_AS: {
				_ensure_space(line);
				line += "as ";
			} break;
			case G_TK_PR_VOID: {
				line += "void ";
			} break;
			case G_TK_PR_ENUM: {
				line += "enum ";
			} break;
			case G_TK_PR_PRELOAD: {
				line += "preload";
			} break;
			case G_TK_PR_ASSERT: {
				line += "assert ";
			} break;
			case G_TK_PR_YIELD: {
				line += "yield ";
			} break;
			case G_TK_PR_SIGNAL: {
				line += "signal ";
			} break;
			case G_TK_PR_BREAKPOINT: {
				line += "breakpoint ";
			} break;
			case G_TK_PR_REMOTE: {
				line += "remote ";
			} break;
			case G_TK_PR_SYNC: {
				line += "sync ";
			} break;
			case G_TK_PR_MASTER: {
				line += "master ";
			} break;
			case G_TK_PR_SLAVE: {
				line += "slave ";
			} break;
			case G_TK_PR_PUPPET: {
				line += "puppet ";
			} break;
			case G_TK_PR_REMOTESYNC: {
				line += "remotesync ";
			} break;
			case G_TK_PR_MASTERSYNC: {
				line += "mastersync ";
			} break;
			case G_TK_PR_PUPPETSYNC: {
				line += "puppetsync ";
			} break;
			case G_TK_BRACKET_OPEN: {
				line += "[";
			} break;
			case G_TK_BRACKET_CLOSE: {
				line += "]";
			} break;
			case G_TK_CURLY_BRACKET_OPEN: {
				line += "{";
			} break;
			case G_TK_CURLY_BRACKET_CLOSE: {
				line += "}";
			} break;
			case G_TK_PARENTHESIS_OPEN: {
				line += "(";
			} break;
			case G_TK_PARENTHESIS_CLOSE: {
				line += ")";
			} break;
			case G_TK_COMMA: {
				line += ", ";
			} break;
			case G_TK_SEMICOLON: {
				line += ";";
			} break;
			case G_TK_PERIOD: {
				line += ".";
			} break;
			case G_TK_QUESTION_MARK: {
				line += "?";
			} break;
			case G_TK_COLON: {
				line += ":";
			} break;
			case G_TK_DOLLAR: {
				line += "$";
			} break;
			case G_TK_FORWARD_ARROW: {
				line += "->";
			} break;
			case G_TK_NEWLINE: {
				for (int j = 0; j < indent; j++) {
					script_text += "\t";
				}
				script_text += line + "\n";
				line = String();
				indent = tokens[i] >> TOKEN_BITS;
			} break;
			case G_TK_CONST_PI: {
				line += "PI";
			} break;
			case G_TK_CONST_TAU: {
				line += "TAU";
			} break;
			case G_TK_WILDCARD: {
				line += "_";
			} break;
			case G_TK_CONST_INF: {
				line += "INF";
			} break;
			case G_TK_CONST_NAN: {
				line += "NAN";
			} break;
			case G_TK_PR_SLAVESYNC: {
				line += "slavesync ";
			} break;
			case G_TK_CF_DO: {
				line += "do ";
			} break;
			case G_TK_CF_CASE: {
				line += "case ";
			} break;
			case G_TK_CF_SWITCH: {
				line += "switch ";
			} break;
			case G_TK_ERROR: {
				//skip - invalid
			} break;
			case G_TK_EOF: {
				//skip - invalid
			} break;
			case G_TK_CURSOR: {
				//skip - invalid
			} break;
			case G_TK_MAX: {
				ERR_FAIL_COND_V_MSG(curr_token == G_TK_MAX, ERR_INVALID_DATA, "Invalid token");
			} break;
			default: {
				ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid token");
			}
		}
		prev_token = curr_token;
	}

	if (!line.is_empty()) {
		for (int j = 0; j < indent; j++) {
			script_text += "\t";
		}
		script_text += line + "\n";
	}

	if (script_text == String()) {
		error_message = RTR("Invalid token");
		return ERR_INVALID_DATA;
	}
	return OK;
}

GDScriptDecomp::BYTECODE_TEST_RESULT GDScriptDecomp::test_bytecode(Vector<uint8_t> p_buffer) {
	//Cleanup
	script_text = String();

	//Load bytecode
	Vector<StringName> identifiers;
	Vector<Variant> constants;
	VMap<uint32_t, uint32_t> lines;
	Vector<uint32_t> tokens;
	int bytecode_version = get_bytecode_version();
	int variant_ver_major = get_variant_ver_major();
	int engine_ver_major = get_engine_ver_major();
	int FUNC_MAX = get_function_count();

	const uint8_t *buf = p_buffer.ptr();
	int total_len = p_buffer.size();
	ERR_FAIL_COND_V(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', BYTECODE_TEST_CORRUPT);

	int version = decode_uint32(&buf[4]);
	ERR_FAIL_COND_V(version != bytecode_version, BYTECODE_TEST_CORRUPT);
	Error err = get_ids_consts_tokens(p_buffer, bytecode_version, identifiers, constants, tokens, lines);
	ERR_FAIL_COND_V(err != OK, BYTECODE_TEST_CORRUPT);
	// Generic test: test that function counts are valid, and that no token >= G_TK_MAX is present
	for (int i = 0; i < tokens.size(); i++) {
		GlobalToken curr_token = get_global_token(tokens[i] & TOKEN_MASK);
		Pair<int, int> arg_count;
		bool test_func = false;
		switch (curr_token) {
			case G_TK_PR_ENUM: {
				if (i + 1 >= tokens.size()) { // if we're at the end of the token list, then we don't have a valid enum token OR a valud TK_PR_PRELOAD token
					return BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL;
				}
				// ENUM requires TK_IDENTIFIER or TK_CURLY_BRACKET_OPEN as the next token
				GlobalToken next_token = get_global_token(tokens[i + 1] & TOKEN_MASK);
				if (next_token != G_TK_IDENTIFIER && next_token != G_TK_CURLY_BRACKET_OPEN) {
					return BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL;
				}
			} break;
			case G_TK_BUILT_IN_FUNC: {
				int func_id = tokens[i] >> TOKEN_BITS;
				if (func_id >= FUNC_MAX) {
					return BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL;
				}
				arg_count = get_function_arg_count(func_id);
				test_func = true;
			} break;
			case G_TK_PR_PRELOAD: // Preload is like a function with 1 argument
				arg_count = { 1, 1 };
				test_func = true;
				break;
			case G_TK_ERROR: // none of these should have made it into the bytecode
			case G_TK_CURSOR:
			case G_TK_MAX:
				return BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL;
			case G_TK_EOF: {
				if (tokens.size() != i + 1) {
					return BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL;
				}
			} break;
			default:
				break;
		}
		if (test_func) { // we're at the function identifier, so check the argument count
			if (i + 2 >= tokens.size()) { // should at least have two more tokens for `()` after the function identifier
				return BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL;
			}
			i++;
			if (tokens[i] != G_TK_PARENTHESIS_OPEN) {
				return BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL;
			}
			i++;
			if (!test_built_in_func_arg_count(tokens, arg_count, i)) { // skips past the function arguments and close parenthesis
				return BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL;
			};
		}
	}

	return BYTECODE_TEST_UNKNOWN;
}

// we should be just past the open parenthesis
bool GDScriptDecomp::test_built_in_func_arg_count(const Vector<uint32_t> &tokens, Pair<int, int> arg_count, int &curr_pos) {
	int pos = curr_pos;
	int comma_count = 0;
	int min_args = arg_count.first;
	int max_args = arg_count.second;
	GlobalToken t = get_global_token(tokens[pos] & TOKEN_MASK);
	int bracket_open = 0;

	if (min_args == 0 && max_args == 0) {
		for (; pos < tokens.size(); pos++, t = get_global_token(tokens[pos] & TOKEN_MASK)) {
			if (t == G_TK_PARENTHESIS_CLOSE) {
				break;
			} else if (t != G_TK_NEWLINE) {
				return false;
			}
		}
		// if we didn't find a close parenthesis, then we have an error
		if (pos == tokens.size()) {
			return false;
		}
		curr_pos = pos;
		return true;
	}
	// count the commas
	// at least in 3.x and below, the only time commas are allowed in function args are other expressions
	// this is not the case for GDScript 2.0 (4.x), due to lambdas, but that doesn't have a compiled version yet
	for (; pos < tokens.size(); pos++, t = get_global_token(tokens[pos] & TOKEN_MASK)) {
		switch (t) {
			case G_TK_BRACKET_OPEN:
			case G_TK_CURLY_BRACKET_OPEN:
			case G_TK_PARENTHESIS_OPEN:
				bracket_open++;
				break;
			case G_TK_BRACKET_CLOSE:
			case G_TK_CURLY_BRACKET_CLOSE:
			case G_TK_PARENTHESIS_CLOSE:
				bracket_open--;
				break;
			case G_TK_COMMA:
				if (bracket_open == 0) {
					comma_count++;
				}
				break;
			default:
				break;
		}
		if (bracket_open == -1) {
			break;
		}
	}
	// trailing commas are not allowed after the last argument
	if (pos == tokens.size() || t != G_TK_PARENTHESIS_CLOSE || comma_count < min_args - 1 || comma_count > max_args - 1) {
		return false;
	}
	curr_pos = pos;
	return true;
}
