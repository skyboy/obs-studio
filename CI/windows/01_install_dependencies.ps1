Param(
    [Switch]$Help = $(if (Test-Path variable:Help) { $Help }),
    [Switch]$Quiet = $(if (Test-Path variable:Quiet) { $Quiet }),
    [Switch]$Verbose = $(if (Test-Path variable:Verbose) { $Verbose }),
    [ValidateSet("32-bit", "64-bit")]
    [String]$BuildArch = $(if (Test-Path variable:BuildArch) { "${BuildArch}" } else { (Get-CimInstance CIM_OperatingSystem).OSArchitecture })
)

##############################################################################
# Windows dependency management function
##############################################################################
#
# This script file can be included in build scripts for Windows or run
# directly
#
##############################################################################

$ErrorActionPreference = "Stop"

Function Install-obs-deps {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )

    Write-Status "Setup for pre-built Windows OBS dependencies v${Version}"
    Ensure-Directory $DepsBuildDir

    $ArchSuffix = "$(if ($BuildArch -eq "64-bit") { "x64" } else { "x86" })"

    if (!(Test-Path "${DepsBuildDir}/windows-deps-${Version}-${ArchSuffix}")) {

        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { "SilentlyContinue" } else { "Continue" })
        Invoke-WebRequest -Uri "https://github.com/obsproject/obs-deps/releases/download/win-${Version}/windows-deps-${Version}-${ArchSuffix}.zip" -UseBasicParsing -OutFile "windows-deps-${Version}-${ArchSuffix}.zip"
        $ProgressPreference = "Continue"

        Write-Step "Unpack..."

        Expand-Archive -Path "windows-deps-${Version}-${ArchSuffix}.zip" -DestinationPath "${DepsBuildDir}/windows-deps-${Version}-${ArchSuffix}" -Force
    } else {
        Write-Step "Found existing pre-built dependencies..."
    }
}

function Install-qt-deps {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )

    Write-Status "Setup for pre-built dependency Qt v${Version}"
    Ensure-Directory $DepsBuildDir

    $ArchSuffix = "$(if ($BuildArch -eq "64-bit") { "x64" } else { "x86" })"

    if (!(Test-Path "${DepsBuildDir}/windows-deps-${Version}-${ArchSuffix}/mkspecs")) {

        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        Invoke-WebRequest -Uri "https://cdn-fastly.obsproject.com/downloads/windows-deps-qt-${Version}-${ArchSuffix}.zip" -UseBasicParsing -OutFile "windows-deps-qt-${Version}-${ArchSuffix}.zip"
        $ProgressPreference = "Continue"
        
        Write-Step "Unpack..."

        Expand-Archive -Path "windows-deps-qt-${Version}-${ArchSuffix}.zip" -DestinationPath "${DepsBuildDir}/windows-deps-${Version}-${ArchSuffix}" -Force
    } else {
        Write-Step "Found existing pre-built Qt..."
    }
}

function Install-vlc {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )

    Write-Status "Setup for dependency VLC v${Version}"
    Ensure-Directory $DepsBuildDir

    if (!((Test-Path "$DepsBuildDir/vlc-${Version}") -and (Test-Path "$DepsBuildDir/vlc-${Version}/include/vlc/vlc.h"))) {
        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        Invoke-WebRequest -Uri "https://cdn-fastly.obsproject.com/downloads/vlc.zip" -UseBasicParsing -OutFile "vlc_${Version}.zip"
        $ProgressPreference = "Continue"

        Write-Step "Unpack..."
        # Expand-Archive -Path "vlc_${Version}.zip"
        Invoke-Expression "7z x vlc_${Version}.zip -ovlc"
        Move-Item -Path vlc -Destination "vlc-${Version}"
    } else {
        Write-Step "Found existing VLC..."
    }
}

function Install-cef {
    Param(
        [Parameter(Mandatory=$true)]
        [String]$Version
    )
    Write-Status "Setup for dependency CEF v${Version} - ${BuildArch}"

    Ensure-Directory $DepsBuildDir
    $ArchSuffix = "$(if ($BuildArch -eq "64-bit") { "x64" } else { "x86" })"

    if (!((Test-Path "${DepsBuildDir}/cef_binary_${Version}_windows_${ArchSuffix}") -and (Test-Path "${DepsBuildDir}/cef_binary_${Version}_windows_${ArchSuffix}/build/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib"))) {
        Write-Step "Download..."
        $ProgressPreference = $(if ($Quiet.isPresent) { 'SilentlyContinue' } else { 'Continue' })
        #Invoke-WebRequest -Uri "https://cdn-fastly.obsproject.com/downloads/cef_binary_${Version}_windows_${ArchSuffix}.zip" -UseBasicParsing -OutFile "cef_binary_${Version}_windows_${ArchSuffix}.zip"
        Invoke-WebRequest -Uri "https://github.com/chromiumembedded/cef/archive/refs/heads/${Version}.zip" -UseBasicParsing -OutFile "cef_dummy.zip"
        Invoke-WebRequest -Uri "https://cef-builds.spotifycdn.com/cef_binary_109.1.18%2Bgf1c41e4%2Bchromium-109.0.5414.120_windows64.tar.bz2" -UseBasicParsing -OutFile "cef_binary.tar.bz2"
        $ProgressPreference = "Continue"

        Write-Step "Unpack..."
        Expand-Archive -Path "cef_dummy.zip" -Force
        Move-Item -Path "${DepsBuildDir}/cef_dummy/cef-${Version}" -Destination "${DepsBuildDir}/cef_binary_${Version}_windows_${ArchSuffix}" -Force
        #Expand-Tar "cef_binary.tar.bz2" "cef_binary${ArchSuffix}.tar"
        Tar -xzf "cef_binary.tar.bz2" "cef_binary"
        Write-Step "$(Test-Path `"${DepsBuildDir}/cef_binary`")"
        Write-Step "$(Test-Path `"${DepsBuildDir}/cef_binary/cef_binary_109.1.18%2Bgf1c41e4%2Bchromium-109.0.5414.120_windows64`")"
        Move-Item -Path "${DepsBuildDir}/cef_binary/cef_binary_109.1.18%2Bgf1c41e4%2Bchromium-109.0.5414.120_windows64" -Destination "${DepsBuildDir}/cef_binary_${Version}_windows_${ArchSuffix}" -Force

        Write-Step "Create Project files..."
        & "${DepsBuildDir}/cef_binary_${Version}_windows_${ArchSuffix}/cef_create_projects.bat"
    } else {
        Write-Step "Found existing CEF framework and loader library..."
    }
}

function Expand-Tar($tarFile, $dest) {

    if (-not (Get-Command Expand-7Zip -ErrorAction Ignore)) {
        Install-Package -Scope CurrentUser -Force 7Zip4PowerShell -ProviderName 'PowerShellGet' > $null
    }

    Expand-7Zip $tarFile $dest
}

function Install-Dependencies {
    Param(
        [String]$BuildArch = $(if (Test-Path variable:BuildArch) { "${BuildArch}" })
    )

    Install-Windows-Dependencies

    $BuildDependencies = @(
        @('obs-deps', $WindowsDepsVersion),
        @('qt-deps', $WindowsDepsVersion),
        @('vlc', $WindowsVlcVersion),
        @('cef', $WindowsCefVersion)
    )

    Foreach($Dependency in ${BuildDependencies}) {
        $DependencyName = $Dependency[0]
        $DependencyVersion = $Dependency[1]

        $FunctionName = "Install-${DependencyName}"
        Invoke-Expression "${FunctionName} -Version ${DependencyVersion}"
    }

    Ensure-Directory ${CheckoutDir}
}

function Install-Dependencies-Standalone {
    $ProductName = "OBS-Studio"
    $CheckoutDir = Resolve-Path -Path "$PSScriptRoot\..\.."
    $DepsBuildDir = "${CheckoutDir}/../obs-build-dependencies"
    $ObsBuildDir = "${CheckoutDir}/../obs-studio"

    . ${CheckoutDir}/CI/include/build_support_windows.ps1

    Write-Status "Setup of OBS build dependencies"
    Install-Dependencies
}

function Print-Usage {
    $Lines = @(
        "Usage: ${_ScriptName}",
        "-Help                    : Print this help",
        "-Quiet                   : Suppress most build process output",
        "-Verbose                 : Enable more verbose build process output",
        "-Choco                   : Enable automatic dependency installation via Chocolatey - Default: off"
        "-BuildArch               : Build architecture to use (32-bit or 64-bit) - Default: local architecture"
    )

    $Lines | Write-Host
}

if(!(Test-Path variable:_RunObsBuildScript)) {
    $_ScriptName = "$($MyInvocation.MyCommand.Name)"
    if($Help.isPresent) {
        Print-Usage
        exit 0
    }

    Install-Dependencies-Standalone
}
