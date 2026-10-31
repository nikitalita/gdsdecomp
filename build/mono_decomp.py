import json
import os
import platform
import shutil
import subprocess
import tarfile
from subprocess import STDOUT, CalledProcessError, check_output

from .common import get_cmd_env, is_dev_build, get_sources, add_libs_to_env
from .paths import (
    get_module_dir,
    get_build_dir,
    get_godot_mono_decomp_dir,
    get_dotnet_runtime_dir,
    get_dotnet_runtime_sdk_archive,
    get_dotnet_runtime_sdk_dir,
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


def get_dotnet_publish_cmd(build_env, mono_native_lib_type, target_arch, godot_mono_decomp_dir, runtime_sdk_dir=None):
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
    if has_any_sanitizer(build_env):
        dotnet_publish_cmd += ["-p:PublishProfile=WithSanitizers"]
        if build_env.get("use_asan", False):
            dotnet_publish_cmd += ["-p:UseASAN=true"]
        if build_env.get("use_lsan", False):
            dotnet_publish_cmd += ["-p:UseLSAN=true"]
        if build_env.get("use_tsan", False):
            dotnet_publish_cmd += ["-p:UseTSAN=true"]
        if build_env.get("use_msan", False):
            dotnet_publish_cmd += ["-p:UseMSAN=true"]
        if build_env.get("use_ubsan", False):
            dotnet_publish_cmd += ["-p:UseUBSAN=true"]
        runtime_sanitizers = get_runtime_sanitizers(build_env)
        if runtime_sanitizers:
            dotnet_publish_cmd += [f"-p:EnableNativeSanitizers={','.join(runtime_sanitizers)}"]
        if runtime_sdk_dir:
            dotnet_publish_cmd += [f"-p:DotNetRuntimeSdkDir={runtime_sdk_dir}"]
    if build_env["platform"] == "android" or mono_native_lib_type == "Static":
        dotnet_publish_cmd += ["-p:DisableUnsupportedError=true", "-p:PublishAotUsingRuntimePack=true"]
    if mono_native_lib_type == "Static":
        dotnet_publish_cmd += ["--use-current-runtime", "--self-contained"]
    # don't restore
    # dotnet_publish_cmd += ["--no-restore"]
    return dotnet_publish_cmd


def has_any_sanitizer(build_env) -> bool:
    return any(
        build_env.get(flag, False)
        for flag in ("use_asan", "use_lsan", "use_tsan", "use_msan", "use_ubsan")
    )


def get_runtime_sanitizers(build_env) -> list[str]:
    sanitizers = []
    if build_env.get("use_asan", False):
        sanitizers.append("address")
    if build_env.get("use_ubsan", False):
        sanitizers.append("undefined")
    return sanitizers


def get_host_dotnet_arch() -> str:
    machine = platform.machine().lower()
    if machine in ("arm64", "aarch64"):
        return "arm64"
    return "x64"


def resolve_dotnet_runtime_tag(godot_mono_decomp_parent_dir: str, cmd_env=None) -> str:
    global_json_path = os.path.join(godot_mono_decomp_parent_dir, "global.json")
    sdk_version = "10.0.0"
    if os.path.exists(global_json_path):
        with open(global_json_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            sdk_version = data.get("sdk", {}).get("version", sdk_version)
    version_parts = sdk_version.split(".")
    major_minor = f"{version_parts[0]}.{version_parts[1]}" if len(version_parts) >= 2 else sdk_version

    try:
        output = check_output(
            ["dotnet", "--list-runtimes"],
            cwd=godot_mono_decomp_parent_dir,
            stderr=STDOUT,
            env=cmd_env,
        ).decode("utf-8")
        matching_versions = []
        for line in output.splitlines():
            line = line.strip()
            if line.startswith("Microsoft.NETCore.App "):
                ver_str = line.split()[1]
                if ver_str.startswith(major_minor + ".") or ver_str == sdk_version:
                    matching_versions.append(ver_str)
        if matching_versions:
            def _ver_key(v: str):
                base = v.split("-")[0]
                return [int(x) if x.isdigit() else 0 for x in base.split(".")]
            matching_versions.sort(key=_ver_key)
            return f"v{matching_versions[-1]}"
    except Exception as exc:
        print(f"Warning: failed to query dotnet --list-runtimes ({exc}), falling back to global.json version {sdk_version}")

    return f"v{sdk_version}"


def _get_runtime_artifact_paths(runtime_dir: str, target_platform: str, target_arch: str):
    triplet = get_godot_mono_triplet(target_platform, target_arch)
    os_part, arch_part = triplet.split("-", 1)
    host_arch = get_host_dotnet_arch()
    coreclr_bin_dir = os.path.join(runtime_dir, "artifacts", "bin", "coreclr", f"{os_part}.{arch_part}.Debug")
    ilc_dir = os.path.join(coreclr_bin_dir, host_arch, "ilc")
    aotsdk_dir = os.path.join(coreclr_bin_dir, "aotsdk")
    native_dir = os.path.join(
        runtime_dir,
        "artifacts",
        "bin",
        f"microsoft.netcore.app.runtime.{triplet}",
        "Release",
        "runtimes",
        triplet,
        "native",
    )
    sanitizers_targets = os.path.join(runtime_dir, "eng", "nativeSanitizers.targets")
    return ilc_dir, aotsdk_dir, native_dir, sanitizers_targets


def _runtime_artifacts_exist(runtime_dir: str, target_platform: str, target_arch: str, expected_stamp: str) -> bool:
    ilc_dir, aotsdk_dir, native_dir, sanitizers_targets = _get_runtime_artifact_paths(
        runtime_dir, target_platform, target_arch
    )
    ilc_exe = "ilc.exe" if platform.system().lower().startswith("win") else "ilc"
    lib_ext = ".lib" if target_platform == "windows" else ".a"
    lib_prefix = "" if target_platform == "windows" else "lib"
    stamp_file = os.path.join(runtime_dir, "artifacts", ".sanitizer_stamp")
    if not os.path.exists(stamp_file):
        return False
    try:
        with open(stamp_file, "r", encoding="utf-8") as f:
            if f.read().strip() != expected_stamp:
                return False
    except OSError:
        return False

    required_paths = [
        os.path.join(ilc_dir, ilc_exe),
        os.path.join(aotsdk_dir, f"{lib_prefix}Runtime.WorkstationGC{lib_ext}"),
        os.path.join(aotsdk_dir, "System.Private.CoreLib.dll"),
        os.path.join(native_dir, f"{lib_prefix}System.Native{lib_ext}"),
        sanitizers_targets,
    ]
    return all(os.path.exists(p) for p in required_paths)


def ensure_sanitized_dotnet_runtime(build_env, target_arch: str, tag: str, runtime_sanitizers: list[str]) -> str:
    runtime_dir = get_dotnet_runtime_dir(build_env)
    cmd_env = get_cmd_env(build_env)
    triplet = get_godot_mono_triplet(build_env["platform"], target_arch)
    os_part, arch_part = triplet.split("-", 1)
    sanitizers_str = ",".join(runtime_sanitizers)
    expected_stamp = f"{tag}:{triplet}:{sanitizers_str}"

    if not os.path.exists(os.path.join(runtime_dir, ".git")):
        os.makedirs(os.path.dirname(runtime_dir), exist_ok=True)
        if os.path.exists(runtime_dir):
            shutil.rmtree(runtime_dir)
        clone_cmd = [
            "git",
            "clone",
            "--branch",
            tag,
            "--depth",
            "1",
            "https://github.com/dotnet/runtime.git",
            runtime_dir,
        ]
        print("CLONING DOTNET RUNTIME:", " ".join(clone_cmd))
        subprocess.run(clone_cmd, check=True, env=cmd_env)
    else:
        current_tag = ""
        try:
            current_tag = check_output(
                ["git", "-C", runtime_dir, "describe", "--tags", "--exact-match"],
                stderr=STDOUT,
                env=cmd_env,
            ).decode("utf-8").strip()
        except Exception:
            pass

        if current_tag != tag:
            print(f"DOTNET RUNTIME TAG MISMATCH (current='{current_tag}', expected='{tag}'). Switching to {tag}...")
            for stale_dir in (".dotnet", "artifacts"):
                stale_path = os.path.join(runtime_dir, stale_dir)
                if os.path.exists(stale_path):
                    shutil.rmtree(stale_path)
            subprocess.run(
                ["git", "-C", runtime_dir, "fetch", "--depth", "1", "origin", "tag", tag],
                check=True,
                env=cmd_env,
            )
            subprocess.run(
                ["git", "-C", runtime_dir, "checkout", "--force", tag],
                check=True,
                env=cmd_env,
            )

    if not _runtime_artifacts_exist(runtime_dir, build_env["platform"], target_arch, expected_stamp):
        is_windows = platform.system().lower().startswith("win")
        build_script = os.path.join(runtime_dir, "build.cmd" if is_windows else "build.sh")
        build_cmd = [
            build_script,
            "clr.aot+libs",
            "-rc",
            "Debug",
            "-lc",
            "Release",
            "-arch",
            arch_part,
            "-fsanitize",
            sanitizers_str,
            "/p:NuGetAudit=false",
        ]
        print("BUILDING SANITIZED DOTNET RUNTIME:", " ".join(build_cmd))
        subprocess.run(build_cmd, cwd=runtime_dir, check=True, env=cmd_env)
        stamp_file = os.path.join(runtime_dir, "artifacts", ".sanitizer_stamp")
        os.makedirs(os.path.dirname(stamp_file), exist_ok=True)
        with open(stamp_file, "w", encoding="utf-8") as f:
            f.write(expected_stamp)

    return runtime_dir


def dotnet_runtime_sanitizer_builder(target, source, build_env, target_arch: str, tag: str, runtime_sanitizers: list[str]):
    archive_path = str(target[0].get_abspath())
    print("BUILDING DOTNET RUNTIME SANITIZER SDK ARCHIVE:", archive_path)
    runtime_dir = ensure_sanitized_dotnet_runtime(build_env, target_arch, tag, runtime_sanitizers)
    ilc_dir, aotsdk_dir, native_dir, sanitizers_targets = _get_runtime_artifact_paths(
        runtime_dir, build_env["platform"], target_arch
    )
    os.makedirs(os.path.dirname(archive_path), exist_ok=True)
    with tarfile.open(archive_path, "w:gz") as tar:
        tar.add(ilc_dir, arcname="ilc")
        tar.add(aotsdk_dir, arcname="aotsdk")
        tar.add(native_dir, arcname="native")
        tar.add(sanitizers_targets, arcname="eng/nativeSanitizers.targets")


def extract_dotnet_runtime_sdk_archive(archive_path: str, sdk_dir: str) -> str:
    if not os.path.exists(archive_path):
        raise FileNotFoundError(f"Expected sanitized dotnet runtime SDK archive at {archive_path}")
    st = os.stat(archive_path)
    stamp_content = f"{st.st_size}:{st.st_mtime_ns}"
    stamp_file = os.path.join(sdk_dir, ".extracted_stamp")
    if os.path.exists(stamp_file):
        try:
            with open(stamp_file, "r", encoding="utf-8") as f:
                if f.read().strip() == stamp_content:
                    return sdk_dir
        except OSError:
            pass
    if os.path.exists(sdk_dir):
        shutil.rmtree(sdk_dir)
    os.makedirs(sdk_dir, exist_ok=True)
    print(f"EXTRACTING DOTNET RUNTIME SANITIZER SDK: {archive_path} -> {sdk_dir}")
    with tarfile.open(archive_path, "r:gz") as tar:
        tar.extractall(path=sdk_dir)
    with open(stamp_file, "w", encoding="utf-8") as f:
        f.write(stamp_content)
    return sdk_dir


def godot_mono_builder(
    target,
    source,
    build_env,
    mono_native_lib_type,
    godot_mono_decomp_dir,
    godot_mono_decomp_libs,
    build_dir,
    runtime_sdk_archives=None,
):
    print("GODOT MONO DECOMP BUILD: ", [str(s) for s in target])
    libs = get_godot_mono_decomp_lib_paths(build_env, godot_mono_decomp_dir, godot_mono_decomp_libs, mono_native_lib_type)
    dev_build = is_dev_build(build_env)
    build_variant = get_dotnet_variant_name(dev_build)
    print("BUILD VARIANT ", build_variant)

    cmd_env = get_cmd_env(build_env)
    runtime_sdk_dir = None
    if runtime_sdk_archives and build_env["arch"] in runtime_sdk_archives:
        archive_path, sdk_dir = runtime_sdk_archives[build_env["arch"]]
        runtime_sdk_dir = extract_dotnet_runtime_sdk_archive(archive_path, sdk_dir)

    dotnet_publish_cmd = get_dotnet_publish_cmd(
        build_env, mono_native_lib_type, build_env["arch"], godot_mono_decomp_dir, runtime_sdk_dir=runtime_sdk_dir
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
        other_runtime_sdk_dir = None
        if runtime_sdk_archives and new_arch in runtime_sdk_archives:
            other_archive_path, other_sdk_dir = runtime_sdk_archives[new_arch]
            other_runtime_sdk_dir = extract_dotnet_runtime_sdk_archive(other_archive_path, other_sdk_dir)
        dotnet_publish_cmd = get_dotnet_publish_cmd(
            build_env, mono_native_lib_type, new_arch, godot_mono_decomp_dir, runtime_sdk_dir=other_runtime_sdk_dir
        )
        print("DOTNET PUBLISH CMD", dotnet_publish_cmd)
        dotnet_publish_output = check_output(dotnet_publish_cmd, cwd=godot_mono_decomp_dir, stderr=STDOUT, env=cmd_env)
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

    all_libs: list[str] = [] + libs
    mono_sources = get_sources(
        module_dir,
        godot_mono_decomp_parent,
        src_suffixes,
        exclude_filters,
    )

    runtime_sanitizers = get_runtime_sanitizers(env)
    runtime_sdk_archives = {}
    if runtime_sanitizers:
        godot_mono_decomp_parent_dir = os.path.join(module_dir, godot_mono_decomp_parent)
        runtime_tag = resolve_dotnet_runtime_tag(godot_mono_decomp_parent_dir, get_cmd_env(env))
        sanitizers_key = "-".join(runtime_sanitizers)
        global_json_rel = os.path.join(godot_mono_decomp_parent, "global.json")

        target_archs = [env["arch"]]
        if mono_native_lib_type == "Shared" and env["platform"] == "macos":
            target_archs.append("x86_64" if env["arch"] == "arm64" else "arm64")

        for arch in target_archs:
            triplet = get_godot_mono_triplet(env["platform"], arch)
            archive_path = get_dotnet_runtime_sdk_archive(env, runtime_tag, triplet, sanitizers_key)
            sdk_dir = get_dotnet_runtime_sdk_dir(env, runtime_tag, triplet, sanitizers_key)
            runtime_sdk_archives[arch] = (archive_path, sdk_dir)

            def _make_runtime_action(arch_val):
                def _runtime_action(target, source, env):
                    return dotnet_runtime_sanitizer_builder(
                        target,
                        source,
                        env,
                        arch_val,
                        runtime_tag,
                        runtime_sanitizers,
                    )
                return _runtime_action

            builder_name = f"dotnetRuntimeSanitizerBuilder_{arch}"
            env_gdsdecomp["BUILDERS"][builder_name] = env_gdsdecomp.Builder(
                action=env_gdsdecomp.Run(_make_runtime_action(arch)),
            )
            runtime_archive_node = getattr(env_gdsdecomp, builder_name)(archive_path, [global_json_rel])
            mono_sources.extend(runtime_archive_node)

    def _builder_action(target, source, env):
        return godot_mono_builder(
            target,
            source,
            env,
            mono_native_lib_type,
            godot_mono_decomp_dir,
            godot_mono_decomp_libs,
            build_dir,
            runtime_sdk_archives=runtime_sdk_archives,
        )

    env_gdsdecomp["BUILDERS"]["godotMonoDecompBuilder"] = env_gdsdecomp.Builder(
        action=env_gdsdecomp.Run(_builder_action),
        suffix=lib_suffix,
        src_suffix=src_suffixes,
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
                if env["platform"] == "linuxbsd" and is_dev_build(env):
                    dbg_lib = lib + ".dbg"
                    copied_dbg_lib = copied_lib + ".dbg"
                    all_libs.append(dbg_lib)
                    copy_commands.append(env_gdsdecomp.CommandNoCache(copied_dbg_lib, dbg_lib, Copy("$TARGET", "$SOURCE")))

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
