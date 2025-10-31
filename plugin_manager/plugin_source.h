#ifndef PLUGIN_SOURCE_H
#define PLUGIN_SOURCE_H

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/shared_object.h"
#include "plugin_info.h"

class PluginSource : public RefCounted {
	GDCLASS(PluginSource, RefCounted)

protected:
	Dictionary _get_release_info(const String &plugin_name, int64_t primary_id, int64_t secondary_id);
	Dictionary _get_plugin_version_numbers(const String &plugin_name);
	static void _bind_methods();
	Mutex cache_mutex;

public:
	static PluginBin get_plugin_bin(const String &path, const SharedObject &obj);

	virtual String get_plugin_name();
	virtual Vector<Pair<int64_t, int64_t>> get_plugin_version_numbers(const String &plugin_name);
	virtual ReleaseInfo get_release_info(const String &plugin_name, int64_t primary_id, int64_t secondary_id);
	void load_cache();
	virtual void load_cache_internal();
	virtual void save_cache();
	virtual bool handles_plugin(const String &plugin_name);
	virtual bool is_default();
	// Helper method for cache expiration
	static constexpr time_t EXPIRY_TIME = 3600; // 1 hour in seconds
	bool is_cache_expired(double retrieved_time) {
		return retrieved_time + EXPIRY_TIME <= OS::get_singleton()->get_unix_time();
	}
};

#endif // PLUGIN_SOURCE_H
