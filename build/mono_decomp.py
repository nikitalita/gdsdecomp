import json
import os
import shutil
from subprocess import STDOUT, CalledProcessError, check_output

from .common import get_cmd_env, is_dev_build, get_sources, add_libs_to_env
from .paths import (
    get_module_dir,
    get_build_dir,
    get_godot_mono_decomp_dir,
    GODOT_MONO_DECOMP_PARENT,
    GODOT_MONO_DECOMP_LIBS,
)

from typing import TYPE_CHECKING, LiteralString

if TYPE_CHECKING:
    from SCons.Node.FS import File as SConsFile
    from ....misc.utility.scons_hints import *


def get_godot_mono_decomp_libs(static_lib, platform: str, is_msvc: bool, libs):
    lib_prefix = "" if is_msvc else "lib"
    if static_lib:
        lib_suffix = ".lib" if is_msvc else ".a"
    else:
        if platform == "macos":
            lib_suffix = ".dylib"
        elif is_msvc:
            lib_suffix = ".lib"
        else:
            lib_suffix = ".so"
    return [lib_prefix + lib + lib_suffix for lib in libs]


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


def get_godot_mono_decomp_lib_dir(godot_mono_decomp_dir, target_platform, target_arch, dev_build):
    triplet = get_godot_mono_triplet(target_platform, target_arch)
    target_framework = "net9.0"
    csproj_path = os.path.join(godot_mono_decomp_dir, "GodotMonoDecompNativeAOT.csproj")
    with open(csproj_path, "r", encoding="utf-8") as csproj_file:
        for line in csproj_file:
            if "TargetFramework" in line:
                target_framework = line.split(">")[1].split("<")[0].strip()
                break

    return os.path.join(godot_mono_decomp_dir, "bin", get_dotnet_variant_name(dev_build), target_framework, triplet, "publish")


def get_godot_mono_decomp_lib_paths(build_env, godot_mono_decomp_dir, libs, mono_native_lib_type):
    return _get_godot_mono_decomp_lib_paths(
        build_env["platform"],
        build_env["arch"],
        is_dev_build(build_env),
        build_env.msvc,
        godot_mono_decomp_dir,
        libs,
        mono_native_lib_type,
    )


def _get_godot_mono_decomp_lib_paths(
    platform: str, arch: str, dev_build: bool, is_msvc: bool, godot_mono_decomp_dir, libs, mono_native_lib_type
):
    lib_names = get_godot_mono_decomp_libs(mono_native_lib_type == "Static", platform, is_msvc, libs)
    build_dir = get_godot_mono_decomp_lib_dir(
        godot_mono_decomp_dir,
        platform,
        arch,
        dev_build,
    )
    lib_paths = [os.path.join(build_dir, lib) for lib in lib_names]
    return lib_paths


def get_dotnet_publish_cmd(build_env, mono_native_lib_type, target_arch, godot_mono_decomp_dir):
    mono_triplet = get_godot_mono_triplet(build_env["platform"], target_arch)
    build_variant = get_dotnet_variant_name(is_dev_build(build_env))
    dotnet_publish_cmd = [
        "dotnet",
        "publish",
        f"/p:NativeLib={mono_native_lib_type}",
        "/p:PublishProfile=AOT",
        # f"/p:MSBuildProjectExtensionsPath={obj_dir}/",
        # f"/p:BaseIntermediateOutputPath={obj_dir}/",
        "-c",
        build_variant,
        "-r",
        mono_triplet,
    ]
    if build_env["use_asan"]:
        dotnet_publish_cmd += ["-p:PublishProfile=WithASAN"]
    if build_env["use_lsan"]:
        dotnet_publish_cmd += ["-p:PublishProfile=WithLSAN"]
    if build_env["use_tsan"]:
        dotnet_publish_cmd += ["-p:PublishProfile=WithTSAN"]
    if build_env["use_msan"]:
        dotnet_publish_cmd += ["-p:PublishProfile=WithMSAN"]
    if build_env["platform"] == "android" or mono_native_lib_type == "Static":
        dotnet_publish_cmd += ["-p:DisableUnsupportedError=true", "-p:PublishAotUsingRuntimePack=true"]
    if mono_native_lib_type == "Static":
        dotnet_publish_cmd += ["--use-current-runtime", "--self-contained"]
    # don't restore
    # dotnet_publish_cmd += ["--no-restore"]
    return dotnet_publish_cmd


def godot_mono_builder(
    target,
    source,
    build_env,
    mono_native_lib_type,
    godot_mono_decomp_dir,
    godot_mono_decomp_libs,
    build_dir,
):
    print("GODOT MONO DECOMP BUILD: ", [str(s) for s in target])
    libs = get_godot_mono_decomp_lib_paths(build_env, godot_mono_decomp_dir, godot_mono_decomp_libs, mono_native_lib_type)
    dev_build = is_dev_build(build_env)
    build_variant = get_dotnet_variant_name(dev_build)
    print("BUILD VARIANT ", build_variant)

    cmd_env = get_cmd_env(build_env)
    dotnet_publish_cmd = get_dotnet_publish_cmd(
        build_env, mono_native_lib_type, build_env["arch"], godot_mono_decomp_dir
    )

    print("DOTNET PUBLISH CMD", " ".join(dotnet_publish_cmd))
    try:
        dotnet_publish_output = check_output(dotnet_publish_cmd, cwd=godot_mono_decomp_dir, stderr=STDOUT, env=cmd_env)
    except CalledProcessError as exc:
        print("ERROR PUBLISHING GODOT MONO DECOMP", exc)
        print(exc.output.decode("utf-8"))
        raise
    except Exception as exc:
        print("ERROR PUBLISHING GODOT MONO DECOMP", exc)
        print(exc)
        raise

    print("DOTNET PUBLISH OUTPUT", dotnet_publish_output.decode("utf-8"))
    if build_env["platform"] == "macos" and mono_native_lib_type == "Shared":
        new_arch = "x86_64" if build_env["arch"] == "arm64" else "arm64"
        dotnet_publish_cmd = get_dotnet_publish_cmd(build_env, mono_native_lib_type, new_arch, godot_mono_decomp_dir)
        print("DOTNET PUBLISH CMD", dotnet_publish_cmd)
        dotnet_publish_output = check_output(dotnet_publish_cmd, cwd=godot_mono_decomp_dir)
        print("DOTNET PUBLISH OUTPUT", dotnet_publish_output.decode("utf-8"))


def get_godot_mono_static_linker_args_from_json(json_output):
    json_blob = json.loads(json_output)
    linker_args = []
    library_args = []
    framework_args = []
    for item in json_blob["Items"]["LinkerArg"]:
        line = item["Identity"].strip()
        if line.startswith("/") or line.startswith("\\") or line[0].isalpha():
            if "libbootstrapper" in line:
                continue
            linker_args.append(line)
        elif line.startswith("-l"):
            library_args.append(line.removeprefix("-l").strip())
        elif line.startswith("-framework"):
            framework_args.append(line.removeprefix("-framework").strip())
    return linker_args, library_args, framework_args


def get_macos_template_lib_dest(env) -> str:
    templ = ""
    if env.editor_build:
        templ = env.Dir("#misc/dist/macos_tools.app").abspath
    else:
        templ = env.Dir("#misc/dist/macos_template.app").abspath
    frameworks_dir = os.path.join(templ, "Contents/Frameworks")
    return frameworks_dir


def get_android_lib_dest(build_env) -> str:
    lib_arch_dir = ""
    if build_env["arch"] == "arm32":
        lib_arch_dir = "armeabi-v7a"
    elif build_env["arch"] == "arm64":
        lib_arch_dir = "arm64-v8a"
    elif build_env["arch"] == "x86_32":
        lib_arch_dir = "x86"
    elif build_env["arch"] == "x86_64":
        lib_arch_dir = "x86_64"
    else:
        print("Architecture not suitable for embedding into APK; keeping .so at \\bin")

    lib_tools_dir = "tools/" if build_env.editor_build else ""
    if build_env.dev_build:
        lib_type_dir = "dev"
    elif build_env.debug_features:
        if build_env.editor_build and build_env["store_release"]:
            lib_type_dir = "release"
        else:
            lib_type_dir = "debug"
    else:
        lib_type_dir = "release"

    jni_libs_dir = "#platform/android/java/lib/libs/" + lib_tools_dir + lib_type_dir + "/"
    out_dir = jni_libs_dir + lib_arch_dir
    return build_env.Dir(out_dir).abspath


def lipo_libs(target, source, env):
    if len(target) != 1:
        raise Exception("Lipo command requires exactly 1 target file")
    if len(source) != 2:
        raise Exception("Lipo command requires exactly 2 source files")
    output_path = str(target[0].get_abspath())

    lipo_cmd = [
        "lipo",
        "-create",
        str(source[0].get_abspath()),
        str(source[1].get_abspath()),
        "-output",
        output_path,
    ]
    print("LIPOING LIBS TO ", output_path)
    return check_output(lipo_cmd)


def build_godot_mono_decomp(
    env,
    env_gdsdecomp,
    module_obj,
):
    from SCons.Defaults import Copy
    module_dir: str = get_module_dir(env)
    build_dir: str = get_build_dir(env)
    godot_mono_decomp_parent: str = GODOT_MONO_DECOMP_PARENT
    godot_mono_decomp_dir: str = get_godot_mono_decomp_dir(env)
    godot_mono_decomp_libs: list[str] = GODOT_MONO_DECOMP_LIBS

    lib_path = get_godot_mono_decomp_lib_dir(
        godot_mono_decomp_dir, env["platform"], env["arch"], env.get("dev_build", False)
    )
    env.Append(LIBPATH=[lib_path])

    mono_native_lib_type = "Static" if env["use_static_godot_mono_decomp"] else "Shared"
    libs = get_godot_mono_decomp_lib_paths(env, godot_mono_decomp_dir, godot_mono_decomp_libs, mono_native_lib_type)
    if mono_native_lib_type == "Static":
        lib_suffix = ".lib" if env.msvc else ".a"
    else:
        if env.msvc:
            lib_suffix = ".lib"
        elif env["platform"] == "macos":
            lib_suffix = ".dylib"
        else:
            lib_suffix = ".so"

    src_suffixes = ["*.h", "*.cs", "*.csproj", "*.props", "*.targets", "*.pubxml", "*.config", "*.nupkg", "*.sln"]
    exclude_filters = [
        "obj/",
        "bin/",
        "obj\\",
        "bin\\",
        "collection_examples",
        "GodotMonoDecompCLI",
        "godot-mono-decomp-aot-test",
        "wrapper/",
        "wrapper\\",
        "register_types.cpp",
        "register_types.h",
    ]

    def _builder_action(target, source, env):
        return godot_mono_builder(
            target,
            source,
            env,
            mono_native_lib_type,
            godot_mono_decomp_dir,
            godot_mono_decomp_libs,
            build_dir,
        )

    env_gdsdecomp["BUILDERS"]["godotMonoDecompBuilder"] = env_gdsdecomp.Builder(
        action=env_gdsdecomp.Run(_builder_action),
        suffix=lib_suffix,
        src_suffix=src_suffixes,
    )

    all_libs: list[str] = [] + libs
    mono_sources = get_sources(
        module_dir,
        godot_mono_decomp_parent,
        src_suffixes,
        exclude_filters,
    )
    copy_commands = []
    if mono_native_lib_type == "Shared":
        if env.msvc:
            all_libs += [lib.replace(".lib", ".dll") for lib in all_libs]
        if env["platform"] == "macos":
            new_arch = "x86_64" if env["arch"] == "arm64" else "arm64"
            other_libs = _get_godot_mono_decomp_lib_paths(
                env["platform"],
                new_arch,
                is_dev_build(env),
                env.msvc,
                godot_mono_decomp_dir,
                godot_mono_decomp_libs,
                mono_native_lib_type,
            )
            all_libs += other_libs
            copy_commands.append(
                env_gdsdecomp.CommandNoCache(
                    os.path.join(build_dir, os.path.basename(libs[0])), all_libs, env_gdsdecomp.Run(lipo_libs)
                )
            )
        elif env["platform"] != "android":
            for lib in libs:
                copied_lib = os.path.join(build_dir, os.path.basename(lib))
                if env.msvc:
                    lib = lib.replace(".lib", ".dll")
                    copied_lib = copied_lib.replace(".lib", ".dll")
                copy_commands.append(env_gdsdecomp.CommandNoCache(copied_lib, lib, Copy("$TARGET", "$SOURCE")))

    godot_mono_decomp_obj = [env_gdsdecomp.godotMonoDecompBuilder(all_libs, mono_sources)]
    env_gdsdecomp.Alias("GodotMonoDecomp", godot_mono_decomp_obj)
    env_gdsdecomp.Requires(module_obj, godot_mono_decomp_obj)
    if len(copy_commands) > 0:
        env_gdsdecomp.Requires(copy_commands, godot_mono_decomp_obj)
        env_gdsdecomp.Requires(module_obj, prerequisite=copy_commands)

    if mono_native_lib_type == "Shared":
        if env["platform"] == "macos" and env["generate_bundle"]:
            bundle_commands = []
            for lib in libs:
                lipo_path = os.path.join(build_dir, os.path.basename(lib))
                dest_path = os.path.join(get_macos_template_lib_dest(env), os.path.basename(lib))
                bundle_commands.append(env_gdsdecomp.CommandNoCache(dest_path, lipo_path, Copy("$TARGET", "$SOURCE")))
            env.Requires(bundle_commands, copy_commands)
            env.Requires("platform/macos/generate_bundle", prerequisite=bundle_commands)
        elif env["platform"] == "android" and env["generate_android_binaries"]:
            bundle_commands = []
            for lib in libs:
                dest_path = os.path.join(get_android_lib_dest(env), os.path.basename(lib))
                bundle_commands.append(env_gdsdecomp.CommandNoCache(dest_path, lib, Copy("$TARGET", "$SOURCE")))
            env.Requires(bundle_commands, godot_mono_decomp_obj)
            env.Requires("platform/android/generate_android_binaries", prerequisite=bundle_commands)

    add_libs_to_env(env, libs)

    if env["platform"] == "linuxbsd" and mono_native_lib_type == "Shared":
        env.Append(LINKFLAGS=["-z", "origin"])
        env.Append(RPATH=env.Literal("\\$$ORIGIN"))

    if mono_native_lib_type == "Static":
        # TODO: Keep static-linking path intact until dotnet static issues are resolved.
        if env.msvc:
            env.Append(LINKFLAGS=["/FORCE:MULTIPLE"])
        elif env["platform"] != "macos" and not env["CXX"].lower().endswith("clang++"):
            env.Append(LINKFLAGS=["-Wl,--allow-multiple-definition"])

        cmd_env = get_cmd_env(env)
        properties = {
            "RuntimeIdentifier": get_godot_mono_triplet(env["platform"], env["arch"]),
            "UseCurrentRuntimeIdentifier": "True",
            "NativeLib": "Static",
            "PublishProfile": "AOT",
            "SelfContained": "true",
            "_IsPublishing": "true",
            "Configuration": get_dotnet_variant_name(is_dev_build(env)),
        }
        # TODO: This is intentionally unconditional for now to preserve current behavior.
        properties["DisableUnsupportedError"] = "true"
        properties["PublishAotUsingRuntimePack"] = "true"

        dotnet_get_linker_args_cmd = [
            "dotnet",
            "msbuild",
            "-restore",
            "-target:PrintLinkerArgs",
            "-getItem:LinkerArg",
        ]
        for property_name, value in properties.items():
            dotnet_get_linker_args_cmd.append(f"-p:{property_name}={value}")
        for property_name, value in properties.items():
            dotnet_get_linker_args_cmd.append(f"-restoreProperty:{property_name}={value}")
        print("RUNNING RESTORE: ", " ".join(dotnet_get_linker_args_cmd))
        ret = check_output(args=dotnet_get_linker_args_cmd, cwd=godot_mono_decomp_dir, env=cmd_env)

        linker_args, library_args, framework_args = get_godot_mono_static_linker_args_from_json(ret.decode("utf-8"))
        if len(linker_args) == 0:
            raise Exception("No linker args found in msbuild.log!!!!!!!!!!!!")
        print("MONO DECOMP LINKER ARGS", linker_args)
        print("MONO DECOMP LIBRARY ARGS", library_args)
        print("MONO DECOMP FRAMEWORK ARGS", framework_args)
        env.Append(LINKFLAGS=linker_args)
        env.Append(LIBS=library_args)
        if env["platform"] == "macos":
            for framework in framework_args:
                env.Append(LINKFLAGS=["-framework", framework])
