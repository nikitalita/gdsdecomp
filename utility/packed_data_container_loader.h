
#include "core/object/ref_counted.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"


#ifndef PACKED_DATA_CONTAINER_LOADER_H
#define PACKED_DATA_CONTAINER_LOADER_H
class PackedDataContainerLoader : public RefCounted {
	GDCLASS(PackedDataContainerLoader, RefCounted);
    
protected:
	static void _bind_methods();

public:
	
    String container_to_json_string(const String &p_path, Error *r_err) const;
};
#endif //PACKED_DATA_CONTAINER_LOADER_H
