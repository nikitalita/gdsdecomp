#ifndef RESOURCE_DOWNGRADER_H
#define RESOURCE_DOWNGRADER_H

#include "core/object/object.h"

class ResourceDowngrader {
public:
	static Error resource_downgrade(const String src_path, const uint32_t target_format_version);

	ResourceDowngrader();
	~ResourceDowngrader();
};
#endif
