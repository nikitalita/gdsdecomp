#pragma once

#include "core/io/logger.h"
#include "core/templates/safe_refcount.h"

class GDRELogger : public Logger {
	Ref<FileAccess> file;
	String base_path;
	bool disabled = false;
	static std::atomic<uint64_t> error_count;
	thread_local static uint64_t thread_error_count;

public:
	String get_path() { return base_path; };
	GDRELogger();
	Error open_file(const String &p_base_path);
	void close_file();
	void _disable(); // only used for during cleanup, because we can't remove the logger
	virtual void logv(const char *p_format, va_list p_list, bool p_err) _PRINTF_FORMAT_ATTRIBUTE_2_0;
	static uint64_t get_error_count();
	static uint64_t get_thread_error_count();
	virtual ~GDRELogger();
};