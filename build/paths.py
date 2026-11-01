import os


EXTERNAL_STEM = "external_install"

MBEDTLS_THIRDPARTY_DIR = "#thirdparty/mbedtls/include/"
PSA_CRYPTO_THIRDPARTY_DIR = "#thirdparty/mbedtls/tf-psa-crypto/include/"
PSA_CRYPTO_THIRDPARTY_DRIVERS_BUILTIN_DIR = "#thirdparty/mbedtls/tf-psa-crypto/drivers/builtin/include/"
MMP3_THIRDPARTY_DIR = "#thirdparty/minimp3/"
LIBOGG_THIRDPARTY_DIR = "#thirdparty/libogg/"
LIBTHEORA_THIRDPARTY_DIR = "#thirdparty/libtheora/"
LIBVORBIS_THIRDPARTY_DIR = "#thirdparty/libvorbis/"
SPIRV_INTEROP_DIR = "#modules/gdsdecomp/spirv-interop/"
SPIRV_CROSS_THIRDPARTY_DIR = "#thirdparty/spirv-cross/"
SPIRV_CROSS_THIRDPARTY_INCLUDE_DIR = "#thirdparty/spirv-cross/include/"
SPIRV_HEADERS_THIRDPARTY_DIR = "#thirdparty/spirv-headers/include/spirv/unified1/"
WEBP_THIRDPARTY_DIR = "#thirdparty/libwebp/"
THORSVG_THIRDPARTY_DIR = "#thirdparty/thorsvg/"
UNSIGNED_HASH_DIR = "#modules/gdsdecomp/external/unsigned-hash-cpp"
UNSIGNED_HASH_INCLUDE_DIR = f"{UNSIGNED_HASH_DIR}/include"
UNSIGNED_HASH_SRC_DIR = f"{UNSIGNED_HASH_DIR}/src"
MODULE_INCLUDE_DIR = "#modules/gdsdecomp/"
VTRACER_INCLUDE_DIR = "#modules/gdsdecomp/external/vtracer/include"
GODOT_MONO_DECOMP_INCLUDE_DIR = "#modules/gdsdecomp/godot-mono-decomp/GodotMonoDecompNativeAOT/include"
AXML_DEC_INCLUDE_DIR = "#modules/gdsdecomp/external/axmldec/include"

VTRACER_PREFIX = "external/vtracer"
VTRACER_LIBS = ["vtracer"]

GODOT_MONO_DECOMP_PARENT = "godot-mono-decomp"
GODOT_MONO_DECOMP_PREFIX = "godot-mono-decomp/GodotMonoDecompNativeAOT"
GODOT_MONO_DECOMP_LIBS = ["GodotMonoDecompNativeAOT"]


def get_module_dir(env):
    return env.Dir("#modules/gdsdecomp").abspath


def get_build_dir(env):
    return env.Dir("#bin").abspath


def get_external_dir(env):
    return os.path.join(get_module_dir(env), EXTERNAL_STEM)


def get_vtracer_dir(env):
    return os.path.join(get_module_dir(env), VTRACER_PREFIX)


def get_vtracer_build_dir(env):
    return os.path.join(get_vtracer_dir(env), "target")


def get_godot_mono_decomp_dir(env):
    return os.path.join(get_module_dir(env), GODOT_MONO_DECOMP_PREFIX)


def get_dotnet_runtime_repo_dir(env):
    custom_dir = env.get("dotnet_runtime_repo_dir", "")
    if custom_dir:
        return os.path.abspath(custom_dir)
    return os.path.join(get_external_dir(env), "dotnet-runtime")


def get_dotnet_runtime_sdk_archive(env, tag, triplet, sanitizers):
    return os.path.join(get_external_dir(env), f"dotnet-runtime-sdk-{tag}-{triplet}-{sanitizers}.tar.gz")


def get_dotnet_runtime_sdk_dir(env, tag, triplet, sanitizers):
    return os.path.join(get_external_dir(env), "dotnet-runtime-sdk", f"{tag}-{triplet}-{sanitizers}")
