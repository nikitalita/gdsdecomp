#pragma once

#include "test_common.h"
#include "tests/test_macros.h"

namespace TestResourceExport {

Error test_export_sample(const String &version);
Error test_export_oggvorbisstr(const String &version);
Error test_export_texture(const String &version);

TEST_CASE("[GDSDecomp][ResourceExport] Export sample") {
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		SUBCASE(vformat("%s: Test export sample", version).utf8().get_data()) {
			test_export_sample(version);
		}
	}
}

TEST_CASE("[GDSDecomp][ResourceExport] Export oggvorbisstr") {
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		SUBCASE(vformat("%s: Test export oggvorbisstr", version).utf8().get_data()) {
			test_export_oggvorbisstr(version);
		}
	}
}

TEST_CASE("[GDSDecomp][ResourceExport] Export texture") {
	Vector<String> versions = get_test_versions();
	CHECK(versions.size() > 0);

	for (const String &version : versions) {
		SUBCASE(vformat("%s: Test export texture", version).utf8().get_data()) {
			test_export_texture(version);
		}
	}
}

} // namespace TestResourceExport
