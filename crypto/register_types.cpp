#include "register_types.h"
#include "core/object/class_db.h"

#include "crypto/crypto_core_gdre_contexts.h"
#include "crypto/custom_decryptor.h"
#include "crypto/file_access_encrypted_custom.h"

void initialize_gdsdecomp_crypto_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	// crypto classes
	ClassDB::register_class<CustomDecryptor>(true);
	ClassDB::register_class<FileAccessEncryptedCustom>();
	ClassDB::register_class<AESContextGDRE>();
	ClassDB::register_class<CamelliaContext>();
	ClassDB::register_class<AriaContext>();
}

void uninitialize_gdsdecomp_crypto_submodule(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
