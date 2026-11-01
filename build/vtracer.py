import os
import platform
import shutil
from subprocess import check_output

from .common import get_cmd_env, is_dev_build, find_llvm_prebuild_path, get_sources, add_libs_to_env
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from SCons.Node.FS import File as SConsFile
    from ....misc.utility.scons_hints import *

from .paths import VTRACER_LIBS, VTRACER_PREFIX, get_module_dir, get_vtracer_build_dir, get_vtracer_dir


def get_cargo_arch(build_env):
    if build_env["arch"] == "x86_32":
        return "i686"
    if build_env["arch"] == "x86_64":
        return "x86_64"
    if build_env["arch"] == "arm32":
        return "armv7"
    if build_env["arch"] == "arm64":
        return "aarch64"
    if build_env["arch"] == "riscv":
        return "riscv32gc"
    if build_env["arch"] == "riscv64":
        return "riscv64gc"
    raise Exception(f"Unsupported architecture: {build_env['arch']}")


def get_cargo_target(build_env):
    arch_part = get_cargo_arch(build_env)
    if build_env["platform"] == "macos":
        return f"{arch_part}-apple-darwin"
    if build_env["platform"] == "linuxbsd":
        if arch_part == "archv7":
            return f"{arch_part}-unknown-linux-gnueabihf"
        return f"{arch_part}-unknown-linux-gnu"
    if build_env["platform"] == "windows":
        suffix = "msvc" if build_env.msvc else "gnu"
        return f"{arch_part}-pc-windows-{suffix}"
    if build_env["platform"] == "web":
        return "wasm32-unknown-emscripten"
    if build_env["platform"] == "ios":
        return f"{arch_part}-apple-ios"
    if build_env["platform"] == "android":
        return f"{arch_part}-linux-android"
    raise Exception(f"Unsupported platform: {build_env['platform']}")


def write_cargo_config_toml_file(build_env, module_dir, vtracer_prefix):
    is_windows = platform.system().lower().startswith("win")
    llvm_prebuild_path = find_llvm_prebuild_path(build_env).replace("\\", "/")
    ar_suffix = ".exe" if is_windows else ""
    linker_suffix = ".cmd" if is_windows else ""
    ar = llvm_prebuild_path + "/llvm-ar" + ar_suffix
    min_sdk_ver = build_env["ndk_platform"].split("-")[1]
    arm64_linker = llvm_prebuild_path + f"/aarch64-linux-android{min_sdk_ver}-clang{linker_suffix}"
    arm_linker = llvm_prebuild_path + f"/arm7a-linux-android{min_sdk_ver}-clang{linker_suffix}"
    x86_linker = llvm_prebuild_path + f"/i686-linux-android{min_sdk_ver}-clang{linker_suffix}"
    x86_64_linker = llvm_prebuild_path + f"/x86_64-linux-android{min_sdk_ver}-clang{linker_suffix}"

    text = f"""
[target.aarch64-linux-android]
ar = "{ar}"
linker = "{arm64_linker}"

[target.armv7-linux-androideabi]
ar = "{ar}"
linker = "{arm_linker}"

[target.i686-linux-android]
ar = "{ar}"
linker = "{x86_linker}"

[target.x86_64-linux-android]
ar = "{ar}"
linker = "{x86_64_linker}"
"""
    cargo_dir = os.path.join(module_dir, vtracer_prefix, ".cargo")
    os.makedirs(cargo_dir, exist_ok=True)
    with open(os.path.join(cargo_dir, "config.toml"), "w") as config_file:
        config_file.write(text)


def write_cargo_toolchain_toml_file(build_env, module_dir, vtracer_prefix):
    channel = "nightly" if build_env["platform"] == "web" else "stable"
    text = f"""[toolchain]
channel = "{channel}"
"""
    vtracer_dir = os.path.join(module_dir, vtracer_prefix)
    os.makedirs(vtracer_dir, exist_ok=True)
    with open(os.path.join(vtracer_dir, "rust-toolchain.toml"), "w") as toolchain_file:
        toolchain_file.write(text)


def get_vtracer_sanitizers(build_env) -> list[str]:
    sanitizers = []
    # Building the vtracer with ASAN is more trouble than it's worth; if the host machine's installed LLVM does not match
    # the LLVM used by the Rust compiler, the ASAN runtime will not be able to find the correct symbols.
    # Only use it if the user explicitly enables it.
    if build_env.get("use_asan", False) and build_env.get("build_vtracer_with_asan", False):
        sanitizers.append("address")
    if build_env.get("use_lsan", False):
        sanitizers.append("leak")
    if build_env.get("use_tsan", False):
        sanitizers.append("thread")
    if build_env.get("use_msan", False):
        sanitizers.append("memory")
    return sanitizers


def get_vtracer_target_dir(build_env, vtracer_build_dir):
    sanitizers = get_vtracer_sanitizers(build_env)
    if sanitizers:
        return os.path.join(vtracer_build_dir, f"san-{'-'.join(sanitizers)}")
    return vtracer_build_dir


def get_vtracer_libs(build_env, vtracer_libs):
    lib_prefix = "" if build_env.msvc else "lib"
    lib_suffix = ".lib" if build_env.msvc else ".a"
    return [lib_prefix + lib + lib_suffix for lib in vtracer_libs]


def get_vtracer_lib_dir(build_env, vtracer_build_dir):
    rust_target = get_cargo_target(build_env)
    variant_dir = "debug" if is_dev_build(build_env) else "release"
    target_dir = get_vtracer_target_dir(build_env, vtracer_build_dir)
    return os.path.join(target_dir, rust_target, variant_dir)


def get_vtracer_lib_paths(build_env, vtracer_build_dir, vtracer_libs):
    build_dir = get_vtracer_lib_dir(build_env, vtracer_build_dir)
    lib_paths = [os.path.join(build_dir, lib) for lib in get_vtracer_libs(build_env, vtracer_libs)]
    return lib_paths


def _has_rust_std_src(cargo_env) -> bool:
    try:
        sysroot = check_output(["rustc", "--print", "sysroot"], env=cargo_env).decode("utf-8").strip()
        std_src_dir = os.path.join(sysroot, "lib", "rustlib", "src", "rust", "library")
        if os.path.isdir(std_src_dir):
            return True
        if shutil.which("rustup", path=cargo_env.get("PATH")):
            print("Installing rust-src component via rustup for sanitizer build-std...")
            check_output(["rustup", "component", "add", "rust-src"], env=cargo_env)
            return os.path.isdir(std_src_dir)
    except Exception as exc:
        print(f"Warning: could not verify/install rust-src for -Zbuild-std: {exc}")
    return False


def cargo_builder(
    build_env,
    source_dir: str,
    build_dir: str,
    libs,
    module_dir: str,
    vtracer_prefix: str,
):
    print("BUILDING VTRACER, LIBRARY PATH: ", str(libs[0].get_abspath()))
    build_variant = "Debug" if is_dev_build(build_env) else "Release"
    print("BUILD VARIANT", build_variant)
    cargo_target = get_cargo_target(build_env)
    cbindgen_dir = os.path.join(source_dir, "include", "vtracer")

    cargo_cmd = ["cargo", "build", "--lib", f"--target={cargo_target}"]
    if not is_dev_build(build_env):
        cargo_cmd.append("--release")
    if "disable_gifski" in build_env and build_env["disable_gifski"]:
        cargo_cmd.extend(["--no-default-features"])

    cargo_env = get_cmd_env(build_env)
    if build_env["platform"] == "android":
        write_cargo_config_toml_file(build_env, module_dir, vtracer_prefix)
    write_cargo_toolchain_toml_file(build_env, module_dir, vtracer_prefix)
    cargo_env["CARGO_TARGET_DIR"] = get_vtracer_target_dir(build_env, build_dir)
    cargo_env["CBINDGEN_TARGET_DIR"] = cbindgen_dir

    if build_env["platform"] == "web":
        cargo_env["RUSTFLAGS"] = (
            "-Cpanic=abort -Ctarget-feature=+atomics,+bulk-memory,+mutable-globals "
            "-Clink-arg=-fno-exceptions -Clink-arg=-sDISABLE_EXCEPTION_THROWING=1 "
            "-Clink-arg=-sDISABLE_EXCEPTION_CATCHING=1 -Cllvm-args=-enable-emscripten-cxx-exceptions=0 "
            "-Clink_arg=--no-entry -Zlink-native-libraries=no"
        )
        cargo_cmd.append("-Zbuild-std=std,panic_abort")

    sanitizers = get_vtracer_sanitizers(build_env)
    if sanitizers:
        cargo_env["RUSTC_BOOTSTRAP"] = "1"
        san_flags = [f"-Zsanitizer={','.join(sanitizers)}"]
        if build_env.get("use_msan", False):
            san_flags.append("-Zsanitizer-memory-track-origins")
        existing_rustflags = cargo_env.get("RUSTFLAGS", "").strip()
        cargo_env["RUSTFLAGS"] = (existing_rustflags + " " + " ".join(san_flags)).strip()
        if build_env["platform"] != "web" and _has_rust_std_src(cargo_env):
            cargo_cmd.append("-Zbuild-std")

    print("CARGO CMD", cargo_cmd)
    output = check_output(cargo_cmd, cwd=source_dir, env=cargo_env)
    print(output.decode("utf-8"))

    if build_env.msvc:
        destination_lib_dir = get_vtracer_lib_dir(build_env, build_dir)
        for lib in libs:
            lib_path = os.path.join(
                destination_lib_dir, os.path.basename(str(lib)).split(".")[0] + build_env["LIBSUFFIX"]
            )
            if os.path.exists(str(lib.get_abspath())):
                shutil.copy(lib, lib_path)


def build_vtracer(
    root_env,
    env_gdsdecomp,
    module_obj,
):
    module_dir = get_module_dir(root_env)
    vtracer_prefix = VTRACER_PREFIX
    vtracer_dir = get_vtracer_dir(root_env)
    vtracer_build_dir = get_vtracer_build_dir(root_env)
    vtracer_libs = VTRACER_LIBS

    root_env.Append(LIBPATH=[get_vtracer_lib_dir(root_env, vtracer_build_dir)])
    source_suffixes = ["*.h", "*.cpp", "*.rs", "*.txt", "cargo.toml"]
    libs = get_vtracer_lib_paths(root_env, vtracer_build_dir, vtracer_libs)
    lib_suffix = ".lib" if root_env.msvc else ".a"

    def vtracer_builder(target, source, env):
        cargo_builder(
            env,
            vtracer_dir,
            vtracer_build_dir,
            target,
            module_dir,
            vtracer_prefix,
        )

    env_gdsdecomp["BUILDERS"]["vtracerBuilder"] = env_gdsdecomp.Builder(
        action=env_gdsdecomp.Run(vtracer_builder),
        src_suffix=source_suffixes,
    )
    vtracer_sources = get_sources(module_dir, vtracer_prefix, source_suffixes, ["target/"])
    vtracer_obj = env_gdsdecomp.vtracerBuilder(libs, vtracer_sources)
    env_gdsdecomp.Alias("vtracer", [vtracer_obj])
    add_libs_to_env(root_env, libs)
    env_gdsdecomp.Requires(module_obj, vtracer_obj)
    if root_env.msvc:
        root_env.Append(LINKFLAGS=["userenv.lib"])
