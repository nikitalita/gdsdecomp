#ifndef PLUGIN_SOURCE_H
#define PLUGIN_SOURCE_H

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/shared_object.h"
#include "utility/plugin_info.h"

class PluginSource : public RefCounted {
	GDCLASS(PluginSource, RefCounted)

protected:
	static void _bind_methods();
	Mutex cache_mutex;

public:
	Error populate_plugin_version_hashes(PluginVersion &plugin_version);
	static PluginBin get_plugin_bin(const String &path, const SharedObject &obj);

	virtual PluginVersion get_plugin_version(const String &plugin_name, const String &version);
	virtual String get_plugin_download_url(const String &plugin_name, const Vector<String> &hashes);
	virtual Vector<String> get_plugin_version_numbers(const String &plugin_name);
	virtual void load_cache();
	virtual void save_cache();
	virtual void prepop_cache(const Vector<String> &plugin_names, bool multithread = false);
	virtual bool handles_plugin(const String &plugin_name);
	virtual bool is_default();
	// Helper method for cache expiration
	static constexpr time_t EXPIRY_TIME = 3600; // 1 hour in seconds
	bool is_cache_expired(double retrieved_time) {
		return retrieved_time + EXPIRY_TIME <= OS::get_singleton()->get_unix_time();
	}
};

#endif // PLUGIN_SOURCE_H
