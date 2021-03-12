#include "gdre_common.h"
#include "core/os/dir_access.h"


Vector<String> get_directory_listing(const String dir, const Vector<String> &filters, const String rel){
	Vector<String> ret;
	DirAccess *da = DirAccess::open(dir.plus_file(rel));
	if (!da) {
		return ret;
	}
	da->list_dir_begin();
	String f;
	while ((f = da->get_next()) != "") {
		if (f == "." || f == ".."){
			continue;
		} else if (da->current_is_dir()){
			Vector<String> dirlist = get_directory_listing(dir, filters, rel.plus_file(f));
			for (int i = 0; i < dirlist.size(); i++){
				ret.push_back(dirlist[i]);
			}
		} else {
			if (filters.size() > 0){
				for (int i = 0; i < filters.size(); i++){
					if (filters[i] == f.extension()) {
						ret.push_back(dir.plus_file(rel).plus_file(f));
						break;
					}
				}
			} else {
				ret.push_back(dir.plus_file(rel).plus_file(f));
			}
			
		}
	}
	memdelete(da);
	return ret;
}

Vector<String> get_directory_listing(const String dir){
	Vector<String> temp;
	return get_directory_listing(dir, temp);
}
