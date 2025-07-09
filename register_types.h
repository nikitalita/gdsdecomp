/*************************************************************************/
/*  register_types.h                                                     */
/*************************************************************************/

#ifndef GDSDECOMP_REGISTER_TYPES_H
#define GDSDECOMP_REGISTER_TYPES_H

#include "modules/register_module_types.h"

void initialize_gdtr_module(ModuleInitializationLevel p_level);
void uninitialize_gdtr_module(ModuleInitializationLevel p_level);
void init_ver_regex();
void free_ver_regex();
#endif // GDSDECOMP_REGISTER_TYPES_H
