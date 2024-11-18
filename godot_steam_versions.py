import json
import os
import zipfile
import shutil
import glob
import re

# import something to do a get on the github api
from numpy import mat
import requests
import urllib.request

GITHUB_RELEASES_ENDPOINT = "https://api.github.com/repos/GodotSteam/GodotSteam/releases?per_page=100&page={}"

THIS_DIR = os.path.dirname(os.path.realpath(__file__))
WORKING_DIR = THIS_DIR + "/.tmp"
JSON_PATH = THIS_DIR + "/misc/godotsteam_versions.json"
TEMPLATE_PATH = THIS_DIR + "/misc/godotsteam_versions.h.inc"
HEADER_PATH = THIS_DIR + "/utility/godotsteam_versions.h"


def get_github_releases():
    releases = []
    page = 1
    while True:
        response = requests.get(GITHUB_RELEASES_ENDPOINT.format(page))
        if response.status_code != 200:
            break
        page_releases = json.loads(response.text)
        if not page_releases:
            break
        releases.extend(page_releases)
        page += 1
    # name to object
    # releases_dict: dict[str, dict] = {}
    # for release in releases:
    #     releases_dict[release["name"]] = release
    return releases


import hashlib
from pathlib import Path


def md5_update_from_file(filename, hash):
    assert Path(filename).is_file()
    with open(str(filename), "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash.update(chunk)
    return hash


def md5_file(filename):
    return md5_update_from_file(filename, hashlib.md5()).hexdigest()


def md5_update_from_dir(directory, hash):
    assert Path(directory).is_dir()
    paths = glob.glob(str(directory) + "/**/*", recursive=True)
    paths = sorted(paths, key=lambda p: str(p))
    for path in paths:
        if not Path(path).is_file() or "_CodeSignature" in path:
            continue
        hash = md5_update_from_file(path, hash)
    return hash


def md5_dir(directory):
    return md5_update_from_dir(directory, hashlib.md5()).hexdigest()


def parse_gdnative_gdextension_releases():
    releases: list[dict] = get_github_releases()
    # dump releases to tmp dir
    ext_releases = []
    for release in releases:
        name: str = release["name"].strip()
        if not ("GDNative" in name or "GDExtension" in name):
            continue
        ext_releases.append(release)
    # ensure the dir
    os.makedirs(WORKING_DIR, exist_ok=True)

    with open(WORKING_DIR + "/releases.json", "w") as f:
        json.dump(ext_releases, f, indent=2)
    version_dict: dict[str, dict] = {}
    versions: set[str] = set()
    # ensure dir
    os.makedirs(WORKING_DIR, exist_ok=True)
    # reverse sort the releases
    releases = sorted(releases, key=lambda x: x["created_at"], reverse=False)

    def strip_x(version: str) -> str:
        if version.endswith(".x"):
            if version.count(".") > 1:
                return version[:-2]
        return version

    def get_min_max_godot_version_from_url(url: str, min_godot_version, max_godot_version) -> tuple[str, str]:
        filename: str = url.split("/")[-1]
        # get the basename of the file (minus the extension)
        basename = filename.rsplit(".", 1)[0]
        re_str = r"(?:(\d\.\d\.\d)-addons)|(?:[^\.](\d{2,3}))(?:-(\d{1,2}))?$"
        match = re.search(re_str, basename)
        if match:
            patch_number = 0
            if match.group(2) and len(match.group(2)) >= 2:
                min_godot_version = match.group(2)[0] + "." + match.group(2)[1] + ".0"
                if len(match.group(2)) > 2:
                    patch_number = int(match.group(2)[2:])
            else:
                min_godot_version = match.group(1)
                # remove the patch number and put on .0
                if len(min_godot_version) > 3:
                    orts = min_godot_version.rsplit(".", 1)
                    min_godot_version = orts[0] + ".0"
                    if len(orts) > 1:
                        patch_number = int(orts[1])
            if match.group(3):
                max_godot_version = match.group(3)[0] + "." + match.group(3)[1]
            else:
                max_godot_version = min_godot_version.rsplit(".", 1)[0]
            max_godot_version += f".{patch_number}"
        return min_godot_version, max_godot_version

    for release in releases:
        name: str = release["name"].strip()
        if not ("GDNative" in name or "GDExtension" in name):
            continue
        #  Name goes like this: "Godot 4.1.3 / Godot 4.2.1 - Steamworks 1.58 - GodotSteam GDExtension 4.5.2",
        parts = name.split("-")
        first_part = parts[0].replace("Godot", "").strip()
        asset: dict
        for asset in release["assets"]:
            max_godot_version = first_part.split("/")[-1].strip()
            if max_godot_version == "3.x":
                max_godot_version = "3.6"
                min_godot_version = "3.5"
            else:
                max_godot_version = strip_x(max_godot_version)
                if "/" in first_part:
                    min_godot_version = strip_x(first_part.split("/")[0].strip())
                else:
                    min_godot_version = max_godot_version
            is_gdnative = "GDNative" in name
            steamworks_version = parts[1].lower().replace("steamworks", "").strip()
            godotsteam_version = (
                parts[2].replace("GDNative", "").replace("GDExtension", "").strip().split(" ")[-1].strip()
            )

            # browser_download_url
            url: str = asset["browser_download_url"]
            tag: str = release["tag_name"]
            if not (".zip" in url.lower() or ".7z" in url.lower()):
                continue
            # get the last part of the url
            filename: str = url.split("/")[-1]
            # get the basename of the file (minus the extension)
            basename = filename.rsplit(".", 1)[0]
            basename = basename.replace("-addons", "")
            min_godot_version, max_godot_version = get_min_max_godot_version_from_url(
                url, min_godot_version, max_godot_version
            )
            if name in version_dict:
                name += "_1"

            zip_path = WORKING_DIR + "/" + filename
            unzipped_folder = WORKING_DIR + "/" + name.replace(" ", "_").replace("/", "_")  # filename.rsplit(".", 1)[0]

            new_path, msg = urllib.request.urlretrieve(url, zip_path)
            # unzip the file to a folder with the same name as the release
            with zipfile.ZipFile(zip_path, "r") as zip_ref:
                zip_ref.extractall(unzipped_folder)

            # go to the unzipped_folder/addons/godotsteam and get the first-level folders
            addon_folder = unzipped_folder + "/addons/godotsteam"
            if not os.path.exists(addon_folder):
                print("Addon folder not found in", addon_folder)
                continue
            # get the first-level folders
            addon_folders = os.listdir(addon_folder)
            platform_folders = []
            plugin_bins: list[dict] = []
            steam_dlls: list[dict] = []
            for platform_name in addon_folders:
                addon_folder_path = addon_folder + "/" + platform_name
                if not os.path.isdir(addon_folder_path):
                    continue
                platform_folders.append(platform_name)
            for platform_name in platform_folders:
                addon_folder_path = addon_folder + "/" + platform_name
                if not os.path.isdir(addon_folder_path):
                    continue
                # list the files in the folder
                files = os.listdir(addon_folder_path)
                steam_files = glob.glob("**/*steam_api*", root_dir=addon_folder_path, recursive=True)
                for steam_file in steam_files:
                    steam_path = addon_folder_path + "/" + steam_file
                    md5 = md5_file(steam_path)
                    file_dict = {"name": steam_file, "md5": md5, "platform": platform_name}
                    steam_dlls.append(file_dict)
                for file in files:
                    is_steam_api = "steam_api" in file.lower()
                    if "steam_api" in file.lower() or "steam_appid" in file.lower():
                        continue
                    # get an md5sum of the file/folder
                    file_path = addon_folder_path + "/" + file
                    if os.path.isdir(file_path):
                        md5 = md5_dir(file_path)
                    else:
                        md5 = md5_file(file_path)
                    file_dict = {"name": file, "md5": md5, "platform": platform_name}
                    plugin_bins.append(file_dict)
            version_dict[name] = {
                "platforms": platform_folders,
                "url": url,
                "min_godot_version": min_godot_version,
                "max_godot_version": max_godot_version,
                "steamworks_version": steamworks_version,
                "version": godotsteam_version,
                "steam_dlls": steam_dlls,
                "bins": plugin_bins,
            }
            try:
                pass
                # shutil.rmtree(unzipped_folder)
                os.remove(zip_path)
            except Exception as e:
                print("Error removing files", e)

    return version_dict


def write_godotsteam_versions():
    version_dict = parse_gdnative_gdextension_releases()

    # dump it to a file
    try:
        with open(JSON_PATH, "w") as f:
            json.dump(version_dict, f, indent=2)
    except Exception as e:
        # just print it out
        print(version_dict)


def write_header_file():
    # open the previously created file
    version_dict = {}
    with open(JSON_PATH, "r") as f:
        version_dict = json.load(f)

    if not version_dict:
        print("No version dict found!!!!")
        return
    # read in the TEMPLATE_PATH as string and replace the version data
    with open(TEMPLATE_PATH, "r") as f:
        header = f.read()
    if not header:
        print("No template found!!!!")
        return
    REPLACE_PART = "// _GODOTSTEAM_VERSIONS_BODY_"
    version_data = ""
    for name, data in version_dict.items():
        INDENT = "\n\t\t\t"
        bins = INDENT + f",{INDENT}".join(
            f'{{"{bin["name"]}", "{bin["md5"]}", "{bin["platform"]}"}}' for bin in data["bins"]
        )
        steam_dlls = INDENT + f",{INDENT}".join(
            f'{{"{dll["name"]}", "{dll["md5"]}", "{dll["platform"]}"}}' for dll in data["steam_dlls"]
        )
        version_def = f'\t{{"{name}", "{data["min_godot_version"]}", "{data["max_godot_version"]}", "{data["steamworks_version"]}", "{data["version"]}", "{data["url"]}", "{", ".join(data["platforms"])}",\n\t\t{{{steam_dlls}}},\n\t\t{{{bins}}}}}'
        version_data += version_def + ",\n"

    data = header.replace(REPLACE_PART, version_data)
    with open(HEADER_PATH, "w") as f:
        f.write(data)


write_godotsteam_versions()
write_header_file()
