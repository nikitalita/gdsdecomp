def can_build(env, platform):
    return True


import os
import sys
import shutil


def _apply_core_patches(env):
    import importlib.util

    patches_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build", "patches.py")
    spec = importlib.util.spec_from_file_location("gdsdecomp_build_patches", patches_path)
    if spec is None or spec.loader is None:
        raise ImportError(f"gdsdecomp: failed to load patches module from {patches_path}")
    patches_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(patches_module)
    patches_module.apply_core_patches(env)


# A terrible hack to force-enable our dependent modules being included on non-editor builds.
# etcpak has dependencies our decompressor requires,
# astcenc has the decompression functions.
# without these, we can't decompress either astc or etc textures.
#
# astcenc and etcpak's can_build functions returns False if the editor_build flag is False,
# and it can't be overridden by any flags. Also, Since they come before us in the modules list,
# we can't monkey patch that.
#
# During configure, env.module_list isn't set, and it's not possible to add modules to it.
# sort_module_list is called right after after env.module_list is set with all the modules,
# so we can monkey patch that to add the modules we need.
def monkey_patch_sort_module_list():
    import methods  # pyright: ignore[reportMissingImports]

    old_sort_module_list = methods.sort_module_list

    def sort_module_list(env):
        if not "etcpak" in env.module_list:
            env.module_list["etcpak"] = "modules/etcpak"
        if not "astcenc" in env.module_list:
            env.module_list["astcenc"] = "modules/astcenc"
            # no need to run configure on etcpak
        if not "tinyexr" in env.module_list:
            env.module_list["tinyexr"] = "modules/tinyexr"
        if not "xatlas_unwrap" in env.module_list:
            env.module_list["xatlas_unwrap"] = "modules/xatlas_unwrap"
        return old_sort_module_list(env)

    methods.sort_module_list = sort_module_list


def configure(env):
    _apply_core_patches(env)
    if not env.editor_build:
        monkey_patch_sort_module_list()
    if not "use_static_godot_mono_decomp" in env:
        env["use_static_godot_mono_decomp"] = False

    if env["use_static_godot_mono_decomp"] and (env["platform"] == "android" or env["platform"] == "macos"):
        print(f"Using shared Mono for {env['platform']} because static Mono is not supported for this platform")
        env["use_static_godot_mono_decomp"] = False

    # hack to force the minimum macOS version to 12.0; it is currently hard-coded to 11.0 on x86_64 builds
    # TODO: remove this hack once the minimum macOS version is updated to 12.0
    if env["platform"] == "macos" and env["arch"] == "x86_64":
        min_version_flag = "-mmacosx-version-min=12.0"
        # remove any existing `-mmacosx-version-min` flags first
        for key in ("CPPFLAGS", "LINKFLAGS", "CCFLAGS", "ASFLAGS"):
            env[key] = [flag for flag in env.get(key, []) if not str(flag).startswith("-mmacosx-version-min")]
        env.Append(CPPFLAGS=[min_version_flag])
        env.Append(LINKFLAGS=[min_version_flag])
        env.Append(CCFLAGS=[min_version_flag])
        env.Append(ASFLAGS=[min_version_flag])


def get_opts(platform):
    from SCons.Variables import BoolVariable

    opts = [
        BoolVariable("disable_godot_mono_decomp", "Disable Godot Mono Decompilation", False),
        BoolVariable("disable_gifski", "Disable Gifski", False),
        BoolVariable("ignore_godot_branch_check", "Ignore Godot branch check", False),
        BoolVariable(
            "build_dotnet_runtime_with_asan",
            "Build a ASAN-enabled dotnet/runtime and use it for the NativeAOT publish (requires use of `use_asan`)",
            False,
        ),
        (
            "dotnet_runtime_repo_dir",
            "Optional path to dotnet/runtime checkout for ASAN-enabled builds; if not provided, a checkout will be cloned from the official repository. (requires use of `use_asan` and `build_dotnet_runtime_with_asan`)",
            "",
        ),
        BoolVariable(
            "build_vtracer_with_asan",
            "Build the vtracer Rust crate with address sanitizers when `use_asan` is enabled. Disabled by default since rustc's ASan instrumentation much match the host machine's used LLVM.",
            False,
        ),
    ]

    if not (platform == "android" or platform == "macos"):
        opts.append(BoolVariable("use_static_godot_mono_decomp", "Build Godot Mono Decomp library as static", False))
    return opts


def get_doc_classes():
    return [
        "GDScriptDecomp",
        "Glob",
        "SemVer",
        "GodotVer",
        "GodotREEditorStandalone",
        "ImportExporter",
        "ImportInfo",
        "PckDumper",
        "ImportExporter",
        "ResourceCompatLoader",
        "Exporter",
        "ResourceExporter",
        "CustomDecryptor",
        "AESContextGDRE",
        "CamelliaContext",
        "AriaContext",
        "GDRESettings",
        "GDREConfig",
        "PckCreator",
    ]


def get_doc_path():
    return "doc_classes"
