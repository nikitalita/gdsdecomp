#if !GODOT_MONO_DECOMP_DISABLED
#include "core/core_constants.h"
#include "core/object/property_info.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "tests/test_macros.h"

#include "../godot-mono-decomp/GodotMonoDecompNativeAOT/include/godot_mono_decomp.h"

TEST_FORCE_LINK(test_csharp)

// The C# side (GodotMonoDecomp/GodotStuff.cs) has its own copies of Godot's PropertyHint, PropertyUsageFlags and
// Variant::Type enums. These tests compare them against the engine's own definitions (via CoreConstants),
// so that we notice when they get out of sync.

namespace TestCSharp {

struct EngineConstant {
	String name;
	int64_t value;
};

// Gets all of the constants that are in the same enum as `p_constant_name`, e.g. "PROPERTY_HINT_MAX" gives all of the PropertyHint constants.
static Vector<EngineConstant> get_engine_enum_constants(const char *p_constant_name) {
	Vector<EngineConstant> ret;
	int idx = CoreConstants::get_global_constant_index(p_constant_name);
	REQUIRE_MESSAGE(idx >= 0, p_constant_name);
	StringName enum_name = CoreConstants::get_global_constant_enum(idx);
	for (int i = 0; i < CoreConstants::get_global_constant_count(); i++) {
		if (CoreConstants::get_global_constant_enum(i) == enum_name) {
			ret.push_back({ CoreConstants::get_global_constant_name(i), CoreConstants::get_global_constant_value(i) });
		}
	}
	return ret;
}

// Converts (and frees) a string returned by the C# library.
static String take_string(const char *p_str) {
	REQUIRE(p_str != nullptr);
	String ret = String::utf8(p_str);
	GodotMonoDecomp_FreeString((void *)p_str);
	return ret;
}

static String property_hint_to_string(int p_hint) {
	return take_string(GodotMonoDecomp_GetPropertyHintString(p_hint));
}

static String variant_type_to_string(int p_type) {
	return take_string(GodotMonoDecomp_GetVariantTypeString(p_type));
}

static String property_usage_to_string(int p_usage) {
	return take_string(GodotMonoDecomp_GetPropertyUsageString(p_usage));
}

static int property_hint_from_string(const String &p_name) {
	return GodotMonoDecomp_GetPropertyHintValue(p_name.utf8().get_data());
}

static int variant_type_from_string(const String &p_name) {
	return GodotMonoDecomp_GetVariantTypeValue(p_name.utf8().get_data());
}

static int property_usage_from_string(const String &p_name) {
	return GodotMonoDecomp_GetPropertyUsageValue(p_name.utf8().get_data());
}

TEST_CASE("[GDSDecomp][CSharp] Max enum values haven't changed") {
	CHECK(GodotMonoDecomp_GetPropertyHintMax() == (int)PropertyHint::PROPERTY_HINT_MAX);
	CHECK(GodotMonoDecomp_GetVariantTypeMax() == (int)Variant::VARIANT_MAX);
}

TEST_CASE("[GDSDecomp][CSharp] PropertyHint values haven't changed") {
	Vector<EngineConstant> hints = get_engine_enum_constants("PROPERTY_HINT_MAX");
	// PROPERTY_HINT_NO_NODEPATH isn't bound, so we add it manually
	EngineConstant no_nodepath = { "PROPERTY_HINT_NO_NODEPATH", (int)PropertyHint::PROPERTY_HINT_NO_NODEPATH };
	if (std::find_if(hints.begin(), hints.end(), [no_nodepath](const EngineConstant &h) { return h.name == no_nodepath.name; }) == hints.end()) {
		hints.push_back(no_nodepath);
	}
	// All of the hints, plus PROPERTY_HINT_MAX
	CHECK_EQ(hints.size(), (int)PropertyHint::PROPERTY_HINT_MAX + 1);

	SUBCASE("Value to name") {
		for (const EngineConstant &hint : hints) {
			INFO(hint.name);
			CHECK_EQ(property_hint_to_string((int)hint.value), hint.name);
		}
	}
	SUBCASE("Name to value") {
		for (const EngineConstant &hint : hints) {
			INFO(hint.name);
			CHECK_EQ(property_hint_from_string(hint.name), (int)hint.value);
		}
	}
	SUBCASE("Names without the PROPERTY_HINT_ prefix and in other casings") {
		CHECK_EQ(property_hint_from_string("NONE"), (int)PropertyHint::PROPERTY_HINT_NONE);
		CHECK_EQ(property_hint_from_string("Range"), (int)PropertyHint::PROPERTY_HINT_RANGE);
		CHECK_EQ(property_hint_from_string("range"), (int)PropertyHint::PROPERTY_HINT_RANGE);
		CHECK_EQ(property_hint_from_string("Enum"), (int)PropertyHint::PROPERTY_HINT_ENUM);
		CHECK_EQ(property_hint_from_string("EnumSuggestion"), (int)PropertyHint::PROPERTY_HINT_ENUM_SUGGESTION);
		CHECK_EQ(property_hint_from_string("MultilineText"), (int)PropertyHint::PROPERTY_HINT_MULTILINE_TEXT);
		CHECK_EQ(property_hint_from_string("File"), (int)PropertyHint::PROPERTY_HINT_FILE);
		CHECK_EQ(property_hint_from_string("GlobalDir"), (int)PropertyHint::PROPERTY_HINT_GLOBAL_DIR);
		CHECK_EQ(property_hint_from_string("ResourceType"), (int)PropertyHint::PROPERTY_HINT_RESOURCE_TYPE);
		CHECK_EQ(property_hint_from_string("PROPERTY_HINT_Flags"), (int)PropertyHint::PROPERTY_HINT_FLAGS);
	}
	SUBCASE("Unknown names are PROPERTY_HINT_NONE") {
		CHECK_EQ(property_hint_from_string("ThisIsNotAHint"), (int)PropertyHint::PROPERTY_HINT_NONE);
		CHECK_EQ(property_hint_from_string(""), (int)PropertyHint::PROPERTY_HINT_NONE);
	}
	SUBCASE("Unknown values have no name") {
		CHECK_EQ(property_hint_to_string((int)PropertyHint::PROPERTY_HINT_MAX + 1), String());
		CHECK_EQ(property_hint_to_string(-1), String());
		CHECK_EQ(property_hint_to_string(0x7FFFFFFF), String());
	}
}

TEST_CASE("[GDSDecomp][CSharp] VariantType values haven't changed") {
	Vector<EngineConstant> types = get_engine_enum_constants("TYPE_MAX");
	// All of the types, plus TYPE_MAX
	CHECK_EQ(types.size(), (int)Variant::VARIANT_MAX + 1);

	// The engine calls them TYPE_BOOL, TYPE_STRING_NAME, etc.; the C# enum doesn't have the prefix
	// (and calls the last one VARIANT_MAX).
	auto get_csharp_name = [](const String &p_engine_name) {
		if (p_engine_name == "TYPE_MAX") {
			return String("VARIANT_MAX");
		}
		return p_engine_name.trim_prefix("TYPE_");
	};

	SUBCASE("Value to name") {
		for (const EngineConstant &type : types) {
			INFO(type.name);
			CHECK_EQ(variant_type_to_string((int)type.value), get_csharp_name(type.name));
		}
	}
	SUBCASE("Godot type name to value") {
		// Variant::get_type_name() gives us the GDScript names ("bool", "Vector2", "PackedInt32Array", etc.)
		// Skip NIL; it has no corresponding C# type.
		for (int i = Variant::BOOL; i < Variant::VARIANT_MAX; i++) {
			String type_name = Variant::get_type_name((Variant::Type)i);
			INFO(type_name);
			CHECK_EQ(variant_type_from_string(type_name), i);
		}
	}
	SUBCASE("C# type names to value") {
		struct {
			const char *csharp_type;
			Variant::Type expected;
		} const cases[] = {
			{ "Boolean", Variant::BOOL },
			{ "bool", Variant::BOOL },
			{ "System.Boolean", Variant::BOOL },
			{ "Int32", Variant::INT },
			{ "Int64", Variant::INT },
			{ "UInt32", Variant::INT },
			{ "int", Variant::INT },
			{ "System.Int32", Variant::INT },
			{ "Single", Variant::FLOAT },
			{ "Double", Variant::FLOAT },
			{ "float", Variant::FLOAT },
			{ "String", Variant::STRING },
			{ "string", Variant::STRING },
			{ "Godot.Vector2", Variant::VECTOR2 },
			{ "Vector2I", Variant::VECTOR2I },
			{ "Rect2I", Variant::RECT2I },
			{ "Vector3I", Variant::VECTOR3I },
			{ "Vector4I", Variant::VECTOR4I },
			{ "Transform2D", Variant::TRANSFORM2D },
			{ "Transform3D", Variant::TRANSFORM3D },
			{ "StringName", Variant::STRING_NAME },
			{ "NodePath", Variant::NODE_PATH },
			{ "Godot.Collections.Dictionary", Variant::DICTIONARY },
			{ "Godot.Collections.Array", Variant::ARRAY },
			{ "byte[]", Variant::PACKED_BYTE_ARRAY },
			{ "Int32[]", Variant::PACKED_INT32_ARRAY },
			{ "Int64[]", Variant::PACKED_INT64_ARRAY },
			{ "Single[]", Variant::PACKED_FLOAT32_ARRAY },
			{ "Double[]", Variant::PACKED_FLOAT64_ARRAY },
			{ "String[]", Variant::PACKED_STRING_ARRAY },
			{ "string[]", Variant::PACKED_STRING_ARRAY },
			{ "Godot.Color[]", Variant::PACKED_COLOR_ARRAY },
			{ "Godot.Vector2[]", Variant::PACKED_VECTOR2_ARRAY },
			{ "Godot.Vector3[]", Variant::PACKED_VECTOR3_ARRAY },
			// Typed arrays are still just Arrays
			{ "Array<Node>", Variant::ARRAY },
			{ "Dictionary<string, int>", Variant::DICTIONARY },
			// Godot 3.x names
			{ "Quat", Variant::QUATERNION },
			{ "Transform", Variant::TRANSFORM3D },
			{ "PoolByteArray", Variant::PACKED_BYTE_ARRAY },
			{ "PoolIntArray", Variant::PACKED_INT32_ARRAY },
			{ "PoolRealArray", Variant::PACKED_FLOAT32_ARRAY },
			{ "PoolStringArray", Variant::PACKED_STRING_ARRAY },
			{ "PoolVector2Array", Variant::PACKED_VECTOR2_ARRAY },
			{ "PoolVector3Array", Variant::PACKED_VECTOR3_ARRAY },
			{ "PoolColorArray", Variant::PACKED_COLOR_ARRAY },
		};
		for (const auto &c : cases) {
			INFO(c.csharp_type);
			CHECK_EQ(variant_type_from_string(c.csharp_type), (int)c.expected);
		}
	}
	SUBCASE("Types that aren't Variant types are OBJECT") {
		CHECK_EQ(variant_type_from_string("Godot.Node"), (int)Variant::OBJECT);
		CHECK_EQ(variant_type_from_string("Node2D"), (int)Variant::OBJECT);
		CHECK_EQ(variant_type_from_string("MyNamespace.MyCustomClass"), (int)Variant::OBJECT);
	}
	SUBCASE("Unknown values have no name") {
		CHECK_EQ(variant_type_to_string((int)Variant::VARIANT_MAX + 1), String());
		CHECK_EQ(variant_type_to_string(-1), String());
		CHECK_EQ(variant_type_to_string(0x7FFFFFFF), String());
	}
}

TEST_CASE("[GDSDecomp][CSharp] PropertyUsageFlags values haven't changed") {
	Vector<EngineConstant> usages = get_engine_enum_constants("PROPERTY_USAGE_DEFAULT");
	REQUIRE_GT(usages.size(), 0);

	// Some flags have the same value (e.g. PROPERTY_USAGE_STORAGE and PROPERTY_USAGE_NO_EDITOR);
	// C# can only give us one of the names for those.
	HashMap<int64_t, Vector<String>> names_by_value;
	for (const EngineConstant &usage : usages) {
		names_by_value[usage.value].push_back(usage.name);
	}

	CHECK_EQ(GodotMonoDecomp_GetPropertyUsageDefault(), (int)PROPERTY_USAGE_DEFAULT);

	SUBCASE("Value to name") {
		for (const EngineConstant &usage : usages) {
			INFO(usage.name);
			const Vector<String> &names = names_by_value[usage.value];
			String csharp_name = property_usage_to_string((int)usage.value);
			if (names.size() == 1) {
				CHECK_EQ(csharp_name, usage.name);
			} else {
				CHECK(names.has(csharp_name));
			}
		}
	}
	SUBCASE("Name to value") {
		for (const EngineConstant &usage : usages) {
			INFO(usage.name);
			CHECK_EQ(property_usage_from_string(usage.name), (int)usage.value);
		}
	}
	SUBCASE("Names without the PROPERTY_USAGE_ prefix and in other casings") {
		CHECK_EQ(property_usage_from_string("NONE"), (int)PROPERTY_USAGE_NONE);
		CHECK_EQ(property_usage_from_string("Storage"), (int)PROPERTY_USAGE_STORAGE);
		CHECK_EQ(property_usage_from_string("Default"), (int)PROPERTY_USAGE_DEFAULT);
		CHECK_EQ(property_usage_from_string("NoEditor"), (int)PROPERTY_USAGE_NO_EDITOR);
		CHECK_EQ(property_usage_from_string("ScriptVariable"), (int)PROPERTY_USAGE_SCRIPT_VARIABLE);
		CHECK_EQ(property_usage_from_string("ReadOnly"), (int)PROPERTY_USAGE_READ_ONLY);
	}
	SUBCASE("Unknown names don't crash") {
		// There's no "none" value to fall back on here, so this returns -1.
		CHECK_EQ(property_usage_from_string("ThisIsNotAUsage"), 0);
		CHECK_EQ(property_usage_from_string(""), 0);
	}
	SUBCASE("Unknown values have no name") {
		// combinations of flags aren't named
		CHECK_EQ(property_usage_to_string((int)(PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL)), "");
	}
}

} // namespace TestCSharp
#endif
