import glob
import os
import sys
import platform
from typing import Any


def is_dev_build(build_env):
    return build_env["dev_build"] if "dev_build" in build_env else False


def get_sources(module_dir, rel_path, filters=None, exclude_filters=None):
    if filters is None:
        filters = ["*.h", "*.hpp", "*.cpp"]
    if exclude_filters is None:
        exclude_filters = []

    abs_path = os.path.join(module_dir, rel_path)
    if not os.path.exists(abs_path):
        raise Exception(
            f"Path {abs_path} does not exist, please run `git submodule update --init --recursive` in the patchwork_editor directory"
        )

    sources = []
    for suffix in filters:
        globstr = os.path.join(abs_path, "**", suffix)
        new_sources = glob.glob(globstr, recursive=True)
        sources += [
            source
            for source in new_sources
            if (not exclude_filters or not any(exclude_filter in source for exclude_filter in exclude_filters))
        ]
    return [os.path.relpath(source, module_dir) for source in sources]


def host_is_windows():
    return platform.system().lower().startswith("win")


def find_llvm_prebuild_path(build_env):
    host_os_name = platform.system().lower()
    if host_os_name.startswith("win"):
        host_os_name = "win"
    ndk_llvm_path = os.path.join(build_env["ANDROID_NDK_ROOT"], "toolchains", "llvm", "prebuilt")
    for folder_name in os.listdir(ndk_llvm_path):
        if folder_name.startswith(host_os_name):
            return os.path.join(ndk_llvm_path, folder_name, "bin")
    raise Exception(f"Could not find LLVM prebuild path for {host_os_name} in {ndk_llvm_path}")


def get_cmd_env(build_env):
    cmd_env = os.environ.copy()
    if build_env["platform"] != "android":
        return cmd_env
    ndk_llvm_path = find_llvm_prebuild_path(build_env)
    print("NDK_LLVM_PATH", ndk_llvm_path)
    path_joiner = ":" if not host_is_windows() else ";"
    cmd_env["PATH"] = ndk_llvm_path + path_joiner + os.environ.get("PATH", "")
    cmd_env["ANDROID_HOME"] = build_env["ANDROID_HOME"]
    cmd_env["ANDROID_SDK_ROOT"] = cmd_env["ANDROID_HOME"]
    cmd_env["ANDROID_NDK_ROOT"] = build_env["ANDROID_NDK_ROOT"]
    return cmd_env


def add_libs_to_env(env, libs):
    for lib in libs:
        if env.msvc:
            if not lib.endswith(".lib"):
                lib = lib.rsplit(".", 1)[0] + ".lib"
            env.Append(LINKFLAGS=[lib])
            continue

        lib_name = os.path.basename(lib).split(".")[0]
        env.Append(LIBS=[lib_name])


# TODO: This helper is currently unused in SCsub.
# Keep it for future external CMake dependencies instead of deleting it.
def cmake_builder(external_dir, source_dir, build_dir, libs, check_output_fn, config_options=None):
    output = bytes()
    config_cmd = ["cmake", "-S", source_dir, "-B", build_dir]
    if config_options:
        config_cmd += config_options
    try:
        output += check_output_fn(["cmake", "-E", "make_directory", external_dir]) + b"\n"
        output += check_output_fn(["cmake", "-E", "make_directory", build_dir]) + b"\n"
        output += check_output_fn(config_cmd) + b"\n"
        output += check_output_fn(["cmake", "--build", build_dir]) + b"\n"
        for lib in libs:
            lib_path = os.path.join(external_dir, lib)
            if os.path.exists(lib_path):
                os.remove(lib_path)
        output += check_output_fn(["cmake", "--install", build_dir, "--prefix", external_dir]) + b"\n"
    except Exception as exc:
        print(f"Failed to build external CMake project: {exc}")
        print(f"Output: {output.decode('utf-8')}")
        raise


def get_dotnet_arch(env_arch):
    if env_arch == "arm64":
        return "arm64"
    elif env_arch == "x86_64":
        return "x64"
    else:
        raise Exception(f"Unsupported architecture: {env_arch}")


def get_build_arch(dotnet_arch):
    if dotnet_arch == "arm64":
        return "arm64"
    elif dotnet_arch == "x64":
        return "x86_64"
    else:
        raise Exception(f"Unsupported architecture: {dotnet_arch}")


def get_godot_mono_triplet(target_platform, target_arch):
    if target_platform == "macos":
        platform_part = "osx"
        arch_part = "arm64" if target_arch == "arm64" else "x64"
    elif target_platform == "linuxbsd":
        platform_part = "linux"
        arch_part = "arm64" if target_arch == "arm64" else "x64"
    elif target_platform == "windows":
        platform_part = "win"
        arch_part = "arm64" if target_arch == "arm64" else "x64"
    elif target_platform == "android":
        platform_part = "linux-bionic"
        arch_part = "arm64" if target_arch == "arm64" else "x64"
    elif target_platform == "web":
        platform_part = "browser"
        arch_part = "wasm"
    else:
        raise Exception(f"Unsupported platform: {target_platform}")
    return f"{platform_part}-{arch_part}"


def get_dotnet_variant_name(dev_build):
    return "Debug" if dev_build else "Release"


def append_cpppaths(build_env, paths):
    for include_path in paths:
        build_env.Append(CPPPATH=[include_path])


def add_source_groups(build_env, module_obj, source_globs):
    for source_glob in source_globs:
        build_env.add_source_files(module_obj, source_glob)


def get_host_platform() -> str:
    host_platform = "windows"
    if (
        sys.platform.startswith("linux")
        or sys.platform.startswith("dragonfly")
        or sys.platform.startswith("freebsd")
        or sys.platform.startswith("netbsd")
        or sys.platform.startswith("openbsd")
    ):
        host_platform = "linuxbsd"
    elif sys.platform == "darwin":
        host_platform = "macos"
    return host_platform
