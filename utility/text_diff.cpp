#include "text_diff.h"
#include "core/string/print_string.h"
#include "external/dtl/dtl.hpp"
#include <sstream>
#include <string>
const char *RED = "\u001b[1;91m";
const char *GREEN = "\u001b[1;92m";
const char *RESET = "\u001b[0m";
const char *BLUE = "\u001b[1;94m";
String TextDiff::get_diff(const String &old_text, const String &new_text) {
	std::string old_text_str = old_text.utf8().get_data();
	std::string new_text_str = new_text.utf8().get_data();
	Vector<String> old_text_lines = old_text.split("\n");
	Vector<String> new_text_lines = new_text.split("\n");
	std::vector<std::string> old_text_lines_vec;
	std::vector<std::string> new_text_lines_vec;
	for (String line : old_text_lines) {
		old_text_lines_vec.push_back(line.utf8().get_data());
	}
	for (String line : new_text_lines) {
		new_text_lines_vec.push_back(line.utf8().get_data());
	}
	dtl::Diff<std::string, std::vector<std::string>> d(old_text_lines_vec, new_text_lines_vec);
	d.compose();
	d.composeUnifiedHunks();
	std::stringstream ss;
	d.printUnifiedFormat(ss);
	return String::utf8(ss.str().c_str());
}

String TextDiff::get_diff_with_header(const String &old_path, const String &new_path, const String &old_text, const String &new_text) {
	String header = "--- a/" + old_path + "\n";
	header += "+++ b/" + new_path + "\n";
	return header + get_diff(old_text, new_text);
}

void TextDiff::print_diff(const String &diff) {
	Vector<String> diff_lines = diff.split("\n");
	for (String line : diff_lines) {
		if (line.begins_with("---") || line.begins_with("+++")) {
			print_line(line); // regular line
		} else if (line.begins_with("@@")) {
			print_line(BLUE + line + RESET); // header line
		} else if (line.begins_with("-")) {
			print_line(RED + line + RESET); // deleted line
		} else if (line.begins_with("+")) {
			print_line(GREEN + line + RESET); // added line
		} else {
			print_line(line); // regular line
		}
	}
}

void TextDiff::_bind_methods() {
	ClassDB::bind_static_method(get_class_static(), D_METHOD("get_diff", "old_text", "new_text"), &TextDiff::get_diff);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("get_diff_with_header", "old_path", "new_path", "old_text", "new_text"), &TextDiff::get_diff_with_header);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("print_diff", "diff"), &TextDiff::print_diff);
}
