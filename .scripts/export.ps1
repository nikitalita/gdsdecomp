echo "Exporting standalone project"
# check if the args are less than 1
if ($args.Length -lt 1) {
    $script_name = $MyInvocation.MyCommand.Name
    echo "Usage: $script_name <export_preset> [-cmd <export_command>] [-path <export_path>] [--debug]"
    exit 1
}

function Get-GodotUserDataDir {
    # - Windows: %APPDATA%\Godot\                    (same as `get_data_dir()`)
    # - macOS: ~/Library/Application Support/Godot/  (same as `get_data_dir()`)
    # - Linux: ~/.config/godot/

    $user_settings_dir = ""
    # if windows, use the appdata path
    if ($env:OS -eq "Windows_NT") {
        $user_settings_dir = Join-Path $env:APPDATA "Godot"
    }
    elseif ($env:OS -eq "OSX") {
        $user_settings_dir = Join-Path $env:HOME "Library/Application Support/Godot"
    }
    else {
        $user_settings_dir = Join-Path $env:HOME ".config/godot"
    }

    # mkdir if it doesn't exist
    if (-not (Test-Path $user_settings_dir)) {
        New-Item -ItemType Directory -Path $user_settings_dir
    }
    return $user_settings_dir
}

function Get-GodotUserSettingsPath {
    $user_settings_dir = Get-GodotUserDataDir

    # list all the files in the directory that begin with "editor_settings-4"
    $user_settings_files = Get-ChildItem $user_settings_dir -Filter "editor_settings-4*"
    if ($user_settings_files.Length -eq 0) {
        return Join-Path $user_settings_dir "editor_settings-4.tres"
    }
    # get the last one that was edited
    $user_settings_file = $user_settings_files | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    return Join-Path $user_settings_dir $user_settings_file.Name
}

function Get-VersionInfo {
    # Try to find git command
    $git = Get-Command git -ErrorAction SilentlyContinue
    $versionInfo = "unknown"
    
    if ($null -eq $git) {
        Write-Host "GDRE WARNING: cannot find git on path, unknown version will be saved in gdre_version.gen.h"
    }
    else {
        # git describe --tags --abbrev=6
        try {
            $versionInfo = & git describe --tags --abbrev=6 2>$null
            if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrEmpty($versionInfo)) {
                Write-Host "GDRE WARNING: git failed to run, unknown version will be saved in gdre_version.gen.h"
                $versionInfo = "unknown"
            }
            else {
                $versionInfo = $versionInfo.Trim()
                
                # git describe --exact-match --tags HEAD
                $res = & git describe --exact-match --tags HEAD 2>$null
                if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrEmpty($res)) {
                    $splits = $versionInfo -split '-'
                    $buildInfo = $splits[-1]
                    $buildNum = $splits[-2]
                    # everything but the last two elements
                    $newVersionInfo = ($splits[0..($splits.Length - 3)] -join '-')
                    
                    $semverRegex = '^[vV]?(?<major>0|[1-9]\d*)\.(?<minor>0|[1-9]\d*)\.(?<patch>0|[1-9]\d*)(?:-(?<prerelease>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+(?<buildmetadata>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$'
                    
                    if ($newVersionInfo -match $semverRegex) {
                        $major = $Matches.major
                        $minor = $Matches.minor
                        $patch = $Matches.patch
                        $prereleaseTag = $Matches.prerelease
                        $buildMetadata = $Matches.buildmetadata
                    }
                    else {
                        Write-Host "WARNING: version string does not match semver format"
                        $splits = $newVersionInfo -split '\.'
                        if ($splits.Length -lt 3) {
                            Write-Host "WARNING: version string is too short"
                            $major = "0"
                            $minor = "0"
                            $patch = "0"
                        }
                        else {
                            $major = $splits[0]
                            $minor = $splits[1]
                            $patch = $splits[2]
                        }
                        $prereleaseTag = ""
                        $buildMetadata = ""
                    }
                    
                    $devStuff = "dev.$buildNum+$buildInfo"
                    if ($prereleaseTag) {
                        $prereleaseNameParts = $prereleaseTag -split '\.'
                        $prereleaseName = $prereleaseNameParts[0]
                        $prereleaseNum = $prereleaseNameParts[-1]
                        if ($prereleaseNum -match '^\d+$') {
                            $prereleaseNum = [string]([int]$prereleaseNum + 1)
                            Write-Host "prerelease_num $prereleaseNum"
                            $prereleaseTag = "$prereleaseName.$prereleaseNum"
                        }
                        else {
                            $prereleaseTag += ".1"
                        }
                        $newVersionInfo = "$major.$minor.$patch-$prereleaseTag+$($devStuff -replace '\+', '-')"
                    }
                    else {
                        if ($patch -match '^\d+$') {
                            $patch = [string]([int]$patch + 1)
                        }
                        else {
                            $patch = "0"
                        }
                        $newVersionInfo = "$major.$minor.$patch-$devStuff"
                    }
                    $versionInfo = $newVersionInfo
                }
                else {
                    $versionInfo = $res.Trim()
                }
            }
        }
        catch {
            Write-Host "GDRE WARNING: git failed to run, unknown version will be saved in gdre_version.gen.h"
            $versionInfo = "unknown"
        }
    }
    # if it begins with v, remove it
    if ($versionInfo.StartsWith("v")) {
        $versionInfo = $versionInfo.Substring(1)
    }
    
    return $versionInfo
}

function Get-NumberOnlyVersion {
    $version = Get-VersionInfo
    $number_only_version = $version -split '-', 2 | Select-Object -First 1
    Write-Host "number_only_version: $number_only_version"
    return $number_only_version
}

function Get-BuildNum {
    $version = Get-VersionInfo
    $build_num = $version -split '-', 3 | Select-Object -Index 1
    if ($build_num -ne $null) {
        $build_num = $build_num -split '\.' | Select-Object -Index 1
    }
    if ($build_num -eq $null) {
        $build_num = 0
    } else {
        $build_num = $build_num -split '\+' | Select-Object -First 1
    }
    return $build_num
}


$current_dir = Get-Location

$export_preset = $args[0]

$export_command = ""
$export_path = ""

$debug = $false

# check if the args are more than 1
if ($args.Length -gt 1) {
    for ($i = 1; $i -lt $args.Length; $i++) {
        $arg = $args[$i]
        switch ($arg) {
            "-cmd" {
                $i++
                $export_command = $args[$i]
                # ensure it is an absolute path
                if (-not ([System.IO.Path]::IsPathRooted($export_command))) {
                    $export_command = Join-Path $current_dir $export_command
                }
            }
            "-path" {
                $i++
                $export_path = $args[$i]
            }
            "--debug" {
                $debug = $true
            }
            "--no-debug" {
                $debug = $false
            }
            default {
                if ($arg -ne ""){
                    echo "Unknown argument: $arg"
                    exit 1
                }
            }
        }
    }
}

if ($export_path -ne "") {
    # check if it is an absolute path; if not, make it absolute
    if (-not ([System.IO.Path]::IsPathRooted($export_path))) {
        $curr_dir = Get-Location
        $export_path = Join-Path $curr_dir $export_path
    }
}

# get the current script path
$scriptPath = $MyInvocation.MyCommand.Path
$scriptDir = Split-Path $scriptPath
$scriptDir = $scriptDir -replace '\\', '/'
$standaloneDir = $scriptDir -replace '/.scripts', '/standalone'
# we're in ${workspaceDir}/modules/gdsdecomp/.scripts, need to be in ${workspaceDir}/bin
$godotBinDir = $scriptDir -replace '/modules/gdsdecomp/.scripts', '/bin'

# check if $export_command is empty
if ($export_command -eq "") {
    $godotEditor = Get-ChildItem $godotBinDir -Filter "*editor*" -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($godotEditor -eq $null) {
        echo "Godot editor path not given and not found in $godotBinDir"
        cd $current_dir
        exit 1
    }
    # get the path of the godot executable
    $export_command = $godotEditor.FullName
}

cd $standaloneDir
if ($export_path -ne "") {
    $export_dir = Split-Path $export_path
} else {
    $export_dir = ".export"
}
mkdir -p $export_dir

$version = Get-VersionInfo
$number_only_version = Get-NumberOnlyVersion
$build_num = Get-BuildNum

echo "Godot editor binary: $export_command"
echo "Export preset: $export_preset"
$echoed_path = $export_path
if ($export_path -eq "") {
    $echoed_path = "<DEFAULT>"
}
echo "Export path: $echoed_path"
echo "Version: $version"
echo "Number only version: $number_only_version"
echo "Build number: $build_num"

$export_presets = Get-Content export_presets.cfg
$export_presets = $export_presets -replace 'application/short_version=".*"', "application/short_version=""$version"""
$export_presets = $export_presets -replace 'application/version=".*"', "application/version=""$version"""
$export_presets = $export_presets -replace 'version/name=".*"', "version/name=""$version"""
$export_presets = $export_presets -replace 'application/file_version=".*"', "application/file_version=""$number_only_version.$build_num"""
$export_presets = $export_presets -replace 'application/product_version=".*"', "application/product_version=""$number_only_version.$build_num"""
$gradle_build_enabled = $false
if ($export_preset -eq "Android") {
    # check if gradle_build/use_gradle_build is true
    if ($export_presets -match 'gradle_build/use_gradle_build=true') {
        $build_dir = "$standaloneDir/../android_build" -replace '\\', '/'
        $export_presets = $export_presets -replace 'gradle_build/gradle_build_directory=".*"', "gradle_build/gradle_build_directory=""$build_dir"""
        echo "Gradle build is enabled, using gradle build directory: $build_dir"
        $gradle_build_enabled = $true
    } else {
        echo "Gradle build is disabled"
    }
}

#output the processed export_presets.cfg
$export_presets | Set-Content export_presets.cfg


# TODO: use an override.cfg instead
# if preset is "Android", open project.godot and replace the rendering method with "mobile"
if ($export_preset -eq "Android") {
    $project_godot = Get-Content project.godot
    $rendering_method = "mobile"
    # check if "renderer/rendering_method" actually exists
    if ($project_godot -match 'renderer/rendering_method=".*"') {
        $project_godot = $project_godot -replace 'renderer/rendering_method=".*"', "renderer/rendering_method=""$rendering_method"""
    } else {
        # we have to add it after the [rendering] section
        $project_godot = $project_godot -replace '\[rendering\]', "[rendering]`n`nrenderer/rendering_method=""$rendering_method""`n"
    }
    $project_godot | Set-Content project.godot
    # check if JAVA_HOME is set
    if ($env:JAVA_HOME -eq $null) {
        echo "JAVA_HOME is not set, please set it to the path to the Java SDK"
        cd $current_dir
        exit 1
    }
    if ($env:ANDROID_HOME -eq $null) {
        echo "ANDROID_HOME is not set, please set it to the path to the Android SDK"
        cd $current_dir
        exit 1
    }
    $user_settings_path = Get-GodotUserSettingsPath
    Write-Host "user_settings_path: $user_settings_path"
    $user_settings = $null
    $java_home = $env:JAVA_HOME -replace '\\', '/'
    $android_home = $env:ANDROID_HOME -replace '\\', '/'

    if (Test-Path $user_settings_path) {
        $user_settings = Get-Content $user_settings_path
        $user_settings = $user_settings -replace 'export/android/java_sdk_path = ".*"', "export/android/java_sdk_path = ""$java_home"""
        $user_settings = $user_settings -replace 'export/android/android_sdk_path = ".*"', "export/android/android_sdk_path = ""$android_home"""
    } else { 
        New-Item -ItemType File -Path $user_settings_path
        $user_settings = "[gd_resource type=""EditorSettings"" format=3]
[resource]

export/android/java_sdk_path = ""$java_home""
export/android/android_sdk_path = ""$android_home""
"
    }
    $user_settings | Set-Content $user_settings_path

    #   echo "GODOT_ANDROID_KEYSTORE_RELEASE_PATH=$HOME/release.keystore" >> "$GITHUB_ENV"
    #   echo "GODOT_ANDROID_KEYSTORE_RELEASE_USER=${{ secrets.ANDROID_KEY_ALIAS }}" >> "$GITHUB_ENV"
    #   echo "GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD=${{ secrets.ANDROID_KEY_PASSWORD }}" >> "$GITHUB_ENV"
    # check for the existence of the keystore vars
    if ($debug -eq $false) {
        # if the keystore vars are not set, use the debug keystore
        if ($env:GODOT_ANDROID_KEYSTORE_RELEASE_PATH -eq $null -or $env:GODOT_ANDROID_KEYSTORE_RELEASE_USER -eq $null -or $env:GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD -eq $null) {
            echo "WARNING: GODOT_ANDROID_KEYSTORE_RELEASE_* variables are not set:"
            if ($env:GODOT_ANDROID_KEYSTORE_RELEASE_PATH -eq $null) {
                echo "GODOT_ANDROID_KEYSTORE_RELEASE_PATH is not set"
            }
            if ($env:GODOT_ANDROID_KEYSTORE_RELEASE_USER -eq $null) {
                echo "GODOT_ANDROID_KEYSTORE_RELEASE_USER is not set"
            }
            if ($env:GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD -eq $null) {
                echo "GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD is not set"
            }
            echo "Using default debug keystore"
            $keystore_path = Join-Path (Get-GodotUserDataDir) "keystores/debug.keystore"
            # check if it exists
            if (-not (Test-Path $keystore_path)) {
                echo "Debug keystore does not exist, creating it"
                # create the keystore
                $keytool_path = Join-Path $env:JAVA_HOME "bin/keytool"
                & $keytool_path -genkey -keystore $keystore_path -storepass android -alias androiddebugkey -keypass android -keyalg RSA -keysize 2048 -validity 10000 -dname "cn=Godot, ou=Godot Engine, o=Stichting Godot, c=NL"
                if ($LASTEXITCODE -ne 0) {
                    echo "Failed to create debug keystore"
                    exit 1
                }
            }
            ${env:GODOT_ANDROID_KEYSTORE_RELEASE_PATH} = $keystore_path
            echo "Release path: ${env:GODOT_ANDROID_KEYSTORE_RELEASE_PATH}"
            ${env:GODOT_ANDROID_KEYSTORE_RELEASE_USER} = "androiddebugkey"
            ${env:GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD} = "android"
        }
    }
}

# turn echo on

$ErrorActionPreference = "Stop"

echo "running: $export_command --headless -e --quit"
# Set-PSDebug -Trace 1
$proc = Start-Process -NoNewWindow -PassThru -FilePath "$export_command" -ArgumentList '--headless -e --quit'
Wait-Process -Id $proc.id -Timeout 300
# Set-PSDebug -Trace 0

$export_flag = "--export-release"
if ($debug) {
    $export_flag = "--export-debug"
}

$export_args = "--headless $export_flag `"$export_preset`" `"$export_path`""
if ($gradle_build_enabled -and $export_preset -eq "Android") {
    $export_args += " --install-android-build-template"
}
echo "running: $export_command $export_args"
# Set-PSDebug -Trace 1
$proc = Start-Process -NoNewWindow -PassThru -FilePath "$export_command" -ArgumentList "$export_args"
Wait-Process -Id $proc.id -Timeout 300
# Set-PSDebug -Trace 0
if ($proc.ExitCode -ne 0) {
    cd $current_dir
    $exit_code = $proc.ExitCode
    echo "Export failed with exit code ""$exit_code"""
    exit 1
}

# if the platform is not macos, we need to copy the *GodotMonoDecompNativeAOT.* libraries to the export_dir
if ($export_preset -ne "macOS") {
    Get-ChildItem $godotBinDir -Filter "*GodotMonoDecompNativeAOT.*" -Recurse | ForEach-Object {
        Copy-Item $_.FullName $export_dir
    }
}

# list the exported files in the $export_dir
echo "Exported files:"
Get-ChildItem $export_dir -Recurse | ForEach-Object {
    echo $_.FullName
}

cd $current_dir
