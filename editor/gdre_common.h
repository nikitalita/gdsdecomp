#ifndef GODOT_RE_COMMON_H
#define GODOT_RE_COMMON_H

#include "core/map.h"
#include "core/resource.h"

Vector<String> get_directory_listing(const String dir, const Vector<String> &filters, const String rel = "");
Vector<String> get_directory_listing(const String dir);

#endif // GODOT_RE_COMMON_H
