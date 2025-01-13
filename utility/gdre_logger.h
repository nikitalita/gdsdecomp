#pragma once

#include "core/io/logger.h"
#include "gd_parallel_queue.h"

class GDRELogger : public Logger {
	Ref<FileAccess> file;
	String base_path;
	bool disabled = false;
	static std::atomic<uint64_t> warning_count;
	static std::atomic<uint64_t> error_count;
	static StaticParallelQueue<String, 2048> error_queue;
	bool is_prebuffering = false;
	Mutex buffer_mutex;
	Vector<String> buffer;

public:
	String get_path() { return base_path; }
	GDRELogger();
	bool is_prebuffering_enabled() { return is_prebuffering; }
	void start_prebuffering();
	void stop_prebuffering();
	Error open_file(const String &p_base_path);
	void close_file();
	void _disable(); // only used for during cleanup, because we can't remove the logger
	virtual void logv(const char *p_format, va_list p_list, bool p_err) _PRINTF_FORMAT_ATTRIBUTE_2_0;
	static uint64_t get_error_count();
	static uint64_t get_thread_error_count();
	static Vector<String> get_errors();
	static Vector<String> get_thread_errors();
	static void clear_error_queues();
	virtual ~GDRELogger();
};