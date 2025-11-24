def can_build(env, platform):
    return True


import methods
import os
import sys
import shutil


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


# A hack to have "generate_bundle" copy the library to the frameworks dir in the bundle
def monkey_patch_macos_generate_bundle():
    # get the current directory
    current_dir = os.path.dirname(os.path.abspath(__file__))
    platform_macos_builders_dir = os.path.abspath(os.path.join(current_dir, "../../platform/macos"))
    sys.path.insert(0, platform_macos_builders_dir)
    import platform_macos_builders

    old_generate_bundle = platform_macos_builders.generate_bundle

    def generate_bundle(target, source, env):
        if "disable_godot_mono_decomp" in env and env["disable_godot_mono_decomp"]:
            old_generate_bundle(target, source, env)
            return
        frameworks_dir = ""
        if env.editor_build:
            templ = env.Dir("#misc/dist/macos_tools.app").abspath
            frameworks_dir = os.path.join(templ, "Contents/Frameworks")
        else:
            templ = env.Dir("#misc/dist/macos_template.app").abspath
            frameworks_dir = os.path.join(templ, "Contents/Frameworks")
        remove_fw_dir = False
        if not os.path.isdir(frameworks_dir):
            remove_fw_dir = True
            os.mkdir(frameworks_dir)
        # Copy frameworks
        monolib = env.Dir("#bin").abspath + "/libGodotMonoDecompNativeAOT.dylib"
        shutil.copy(monolib, frameworks_dir + "/libGodotMonoDecompNativeAOT.dylib")
        # run the original generate_bundle
        old_generate_bundle(target, source, env)
        # remove the library from the frameworks dir
        os.remove(frameworks_dir + "/libGodotMonoDecompNativeAOT.dylib")
        if remove_fw_dir:
            os.rmdir(frameworks_dir)

    platform_macos_builders.generate_bundle = generate_bundle

    # remove the platform_macos_builders from the path
    sys.path.remove(platform_macos_builders_dir)


def monkey_patch_generate_android_binaries():
    current_dir = os.path.dirname(os.path.abspath(__file__))
    platform_android_builders_dir = os.path.abspath(os.path.join(current_dir, "../../platform/android"))
    sys.path.insert(0, platform_android_builders_dir)
    import platform_android_builders

    old_generate_android_binaries = platform_android_builders.generate_android_binaries

    def generate_android_binaries(target, source, env):
        if "disable_godot_mono_decomp" in env and env["disable_godot_mono_decomp"]:
            old_generate_android_binaries(target, source, env)
            return

        lib_arch_dir = ""
        if env["arch"] == "arm32":
            lib_arch_dir = "armeabi-v7a"
        elif env["arch"] == "arm64":
            lib_arch_dir = "arm64-v8a"
        elif env["arch"] == "x86_32":
            lib_arch_dir = "x86"
        elif env["arch"] == "x86_64":
            lib_arch_dir = "x86_64"
        else:
            print("Architecture not suitable for embedding into APK; keeping .so at \\bin")

        if env.editor_build:
            lib_tools_dir = "tools/"
        else:
            lib_tools_dir = ""
        if env.dev_build:
            lib_type_dir = "dev"
        elif env.debug_features:
            if env.editor_build and env["store_release"]:
                lib_type_dir = "release"
            else:
                lib_type_dir = "debug"
        else:  # Release
            lib_type_dir = "release"

        jni_libs_dir = "#platform/android/java/lib/libs/" + lib_tools_dir + lib_type_dir + "/"
        out_dir = jni_libs_dir + lib_arch_dir

        # env_android.CommandNoCache(out_dir + "/libgodot_android.so", lib, Move("$TARGET", "$SOURCE"))
        lib = env.Dir("#bin").abspath + "/libGodotMonoDecompNativeAOT.so"
        out_dir_abs = env.Dir(out_dir).abspath
        # ensure out_dir_abs
        if not os.path.exists(out_dir_abs):
            os.makedirs(out_dir_abs, exist_ok=True)
        # copy it directly using shutil
        shutil.copy(lib, env.Dir(out_dir).abspath + "/libGodotMonoDecompNativeAOT.so")

        old_generate_android_binaries(target, source, env)

    platform_android_builders.generate_android_binaries = generate_android_binaries

    # remove the platform_android_builders from the path
    sys.path.remove(platform_android_builders_dir)


def configure(env):
    if not env.editor_build:
        monkey_patch_sort_module_list()
    if env["platform"] == "macos":
        monkey_patch_macos_generate_bundle()
    elif env["platform"] == "android":
        monkey_patch_generate_android_binaries()


def get_doc_classes():
    return [
        "GDScriptDecomp",
        "GDScriptDecomp_0b806ee",
        "GDScriptDecomp_8c1731b",
        "GDScriptDecomp_31ce3c5",
        "GDScriptDecomp_703004f",
        "GDScriptDecomp_8cab401",
        "GDScriptDecomp_e82dc40",
        "GDScriptDecomp_2185c01",
        "GDScriptDecomp_97f34a1",
        "GDScriptDecomp_be46be7",
        "GDScriptDecomp_65d48d6",
        "GDScriptDecomp_48f1d02",
        "GDScriptDecomp_30c1229",
        "GDScriptDecomp_7d2d144",
        "GDScriptDecomp_64872ca",
        "GDScriptDecomp_6174585",
        "GDScriptDecomp_23441ec",
        "GDScriptDecomp_7124599",
        "GDScriptDecomp_1add52b",
        "GDScriptDecomp_4ee82a2",
        "GDScriptDecomp_513c026",
        "GDScriptDecomp_23381a5",
        "GDScriptDecomp_85585c7",
        "GDScriptDecomp_ed80f45",
        "GDScriptDecomp_8b912d1",
        "GDScriptDecomp_62273e5",
        "GDScriptDecomp_f8a7c46",
        "GDScriptDecomp_c24c739",
        "GDScriptDecomp_5e938f0",
        "GDScriptDecomp_015d36d",
        "GDScriptDecomp_c6120e7",
        "GDScriptDecomp_d28da86",
        "GDScriptDecomp_216a8aa",
        "GDScriptDecomp_91ca725",
        "GDScriptDecomp_054a2ac",
        "GDScriptDecomp_ff1e7cf",
        "GDScriptDecomp_a56d6ff",
        "GDScriptDecomp_3ea6d9f",
        "GDScriptDecomp_8e35d93",
        "GDScriptDecomp_a3f1ee5",
        "GDScriptDecomp_8aab9a0",
        "GDScriptDecomp_d6b31da",
        "GDScriptDecomp_1ca61a3",
        "GDScriptDecomp_1a36141",
        "GDScriptDecomp_514a3fb",
        "GDScriptDecomp_7f7d97f",
        "GDScriptDecomp_620ec47",
        "GDScriptDecomp_c00427a",
        "GDScriptDecomp_a60f242",
        "GDScriptDecomp_6694c11",
        "GDScriptDecomp_506df14",
        "GDScriptDecomp_5565f55",
        "GDScriptDecomp_f3f05dc",
        "GDRECLIMain",
        "GodotREEditorStandalone",
        "ImportExporter",
        "ImportInfo",
        "NewPackDialog",
        "PackDialog",
        "PckDumper",
        "ScriptCompDialog",
        "ScriptDecompDialog",
    ]


def get_doc_path():
    return "doc_classes"
