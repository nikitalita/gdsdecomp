
#pragma once

#include "bytecode_base.h"

class GDScriptDecomp_custom : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_custom, GDScriptDecomp);

protected:
	static void _bind_methods() {}
	int bytecode_version;
	int bytecode_rev;
	int engine_ver_major;
	int variant_ver_major;
	String engine_version;
	String max_engine_version;
	String date;
	int parent;
	Vector<GlobalToken> tokens;
	Vector<String> functions;
	GDScriptDecomp_custom(Dictionary p_custom_def);

public:
	virtual String get_function_name(int p_func) const override;
	virtual int get_function_count() const override;
	virtual Pair<int, int> get_function_arg_count(int p_func) const override;
	virtual int get_token_max() const override;
	virtual int get_function_index(const String &p_func) const override;
	virtual GDScriptDecomp::GlobalToken get_global_token(int p_token) const override;
	virtual int get_local_token_val(GDScriptDecomp::GlobalToken p_token) const override;
	virtual int get_bytecode_version() const override { return bytecode_version; }
	virtual int get_bytecode_rev() const override { return bytecode_rev; }
	virtual int get_engine_ver_major() const override { return engine_ver_major; }
	virtual int get_variant_ver_major() const override { return variant_ver_major; }
	virtual int get_parent() const override { return parent; }
	virtual String get_engine_version() const override { return engine_version; }
	virtual String get_max_engine_version() const override { return max_engine_version; }
	virtual String get_date() const override { return date; }
	virtual bool is_custom() const override { return true; }
	GDScriptDecomp_custom() {}

	static GDScriptDecomp_custom *_create_from_json(Dictionary p_custom_def);
	static Ref<GDScriptDecomp_custom> create_from_json(Dictionary p_custom_def);
};
