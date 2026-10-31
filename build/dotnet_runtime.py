import json
import os
import platform
import shutil
import subprocess
import tarfile
from subprocess import STDOUT, check_output

from .common import get_cmd_env, get_godot_mono_triplet
from .paths import (
    GODOT_MONO_DECOMP_PARENT,
    get_dotnet_runtime_dir,
    get_dotnet_runtime_sdk_archive,
    get_dotnet_runtime_sdk_dir,
    get_module_dir,
)


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
    _, arch_part = triplet.split("-", 1)
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


def setup_dotnet_runtime_sanitizer_builders(env, env_gdsdecomp, mono_native_lib_type: str):
    runtime_sanitizers = get_runtime_sanitizers(env)
    runtime_sdk_archives = {}
    runtime_archive_nodes = []
    if not runtime_sanitizers:
        return runtime_sdk_archives, runtime_archive_nodes

    module_dir = get_module_dir(env)
    godot_mono_decomp_parent_dir = os.path.join(module_dir, GODOT_MONO_DECOMP_PARENT)
    runtime_tag = resolve_dotnet_runtime_tag(godot_mono_decomp_parent_dir, get_cmd_env(env))
    sanitizers_key = "-".join(runtime_sanitizers)
    global_json_rel = os.path.join(GODOT_MONO_DECOMP_PARENT, "global.json")

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
        runtime_archive_nodes.extend(runtime_archive_node)

    return runtime_sdk_archives, runtime_archive_nodes
