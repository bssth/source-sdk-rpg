<#
.SYNOPSIS
    Builds the mod (incrementally) and launches it in Source SDK Base 2013 Singleplayer.

.DESCRIPTION
    Resolution order for the Source SDK Base 2013 Singleplayer install directory:
        1. -SdkPath parameter
        2. $env:SOURCE_SDK_BASE_2013 environment variable
        3. $SourceSdkBase2013 defined in launch.local.ps1 (next to this script, gitignored)
        4. Steam registry + libraryfolders.vdf lookup (Steam AppID 243730)

    The same resolution order applies for the mod game directory via $ModDir / $env:BB_MOD_DIR,
    defaulting to <sp>\game\mod_sdk2013ce.

.PARAMETER SdkPath
    Path to "Source SDK Base 2013 Singleplayer" (the folder containing hl2.exe).

.PARAMETER ModDir
    Absolute path to the mod game folder (the one with gameinfo.txt).

.PARAMETER Configuration
    MSBuild configuration. Default: Debug.

.PARAMETER NoBuild
    Skip the MSBuild step — just launch.

.PARAMETER ExtraArgs
    Extra arguments appended to hl2.exe. Default: -dev -console -allowdebug.
#>
[CmdletBinding()]
param(
    [string]   $SdkPath,
    [string]   $ModDir,
    [ValidateSet('Debug','Release')]
    [string]   $Configuration = 'Debug',
    [switch]   $NoBuild,
    [string[]] $ExtraArgs = @('-dev','-console','-allowdebug')
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SrcDir    = Join-Path $ScriptDir 'src'
$Solution  = Join-Path $SrcDir 'Everything.sln'

# ---------------------------------------------------------------------------
# Local overrides (gitignored). Define any of the following there:
#   $SourceSdkBase2013 = 'D:\Games\Steam\steamapps\common\Source SDK Base 2013 Singleplayer'
#   $ModDir            = 'D:\dev\source-sdk-bb-temp\sp\game\mod_sdk2013ce'
#   $Configuration     = 'Release'
# ---------------------------------------------------------------------------
$LocalCfg = Join-Path $ScriptDir 'launch.local.ps1'
if (Test-Path $LocalCfg) { . $LocalCfg }

function Resolve-SourceSdkBase2013 {
    # 1. Steam registry
    $steamKey = Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue
    if (-not $steamKey) {
        $steamKey = Get-ItemProperty -Path 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam' -ErrorAction SilentlyContinue
    }
    if (-not $steamKey) { return $null }

    $steamRoot = @($steamKey.SteamPath, $steamKey.InstallPath | Where-Object { $_ })[0]
    if (-not $steamRoot) { return $null }
    $steamRoot = ([string]$steamRoot).Replace('/','\')

    # 2. Collect library folders
    $libs = @($steamRoot)
    $vdfCandidates = @(
        (Join-Path $steamRoot 'config\libraryfolders.vdf'),
        (Join-Path $steamRoot 'steamapps\libraryfolders.vdf')
    )
    foreach ($vdf in $vdfCandidates) {
        if (Test-Path $vdf) {
            $text = Get-Content -LiteralPath $vdf -Raw
            foreach ($m in [regex]::Matches($text, '"path"\s*"([^"]+)"')) {
                $p = $m.Groups[1].Value -replace '\\\\','\'
                if ($libs -notcontains $p) { $libs += $p }
            }
            break
        }
    }

    # 3. Probe each library for hl2.exe of Source SDK Base 2013 Singleplayer
    foreach ($lib in $libs) {
        $candidate = Join-Path $lib 'steamapps\common\Source SDK Base 2013 Singleplayer'
        if (Test-Path (Join-Path $candidate 'hl2.exe')) { return $candidate }
    }
    return $null
}

function Resolve-MSBuild {
    $cmd = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
                            -find 'MSBuild\**\Bin\MSBuild.exe' 2>$null | Select-Object -First 1
        if ($found) { return $found }
    }
    throw "MSBuild not found. Install Visual Studio / Build Tools or add msbuild to PATH."
}

# --- Resolve SDK path ------------------------------------------------------
if (-not $SdkPath)           { $SdkPath = $env:SOURCE_SDK_BASE_2013 }
if (-not $SdkPath -and $SourceSdkBase2013) { $SdkPath = $SourceSdkBase2013 }
if (-not $SdkPath)           { $SdkPath = Resolve-SourceSdkBase2013 }
if (-not $SdkPath) {
    throw @"
Source SDK Base 2013 Singleplayer not found.
Install it via Steam (AppID 243730) or point to it explicitly:
  * -SdkPath '<path>'
  * `$env:SOURCE_SDK_BASE_2013 = '<path>'
  * create $LocalCfg with: `$SourceSdkBase2013 = '<path>'
"@
}
$hl2 = Join-Path $SdkPath 'hl2.exe'
if (-not (Test-Path $hl2)) { throw "hl2.exe not found at: $hl2" }

# --- Resolve mod directory -------------------------------------------------
if (-not $ModDir) { $ModDir = $env:BB_MOD_DIR }
if (-not $ModDir) { $ModDir = Join-Path $ScriptDir 'game\mod_sdk2013ce' }
$ModDir = (Resolve-Path -LiteralPath $ModDir).Path
if (-not (Test-Path (Join-Path $ModDir 'gameinfo.txt'))) {
    throw "gameinfo.txt not found in mod dir: $ModDir"
}

# --- Ensure HL2/EP1/EP2/platform junctions exist ---------------------------
$SteamRoot = Join-Path $ModDir '_steam'
if (-not (Test-Path (Join-Path $SteamRoot 'hl2'))) {
    $setup = Join-Path $ScriptDir 'setup-game-content.ps1'
    if (Test-Path $setup) {
        Write-Host ">> First-run: creating game content junctions" -ForegroundColor Cyan
        & $setup -SdkPath $SdkPath -ModDir $ModDir
    }
}

# --- Build (incremental) ---------------------------------------------------
if (-not $NoBuild) {
    $msbuild = Resolve-MSBuild
    Write-Host ">> Building Client;Server ($Configuration|Win32) via $msbuild" -ForegroundColor Cyan
    & $msbuild $Solution `
        /t:Client`;Server `
        /p:Configuration=$Configuration `
        /p:Platform=Win32 `
        /m `
        /nologo `
        /verbosity:minimal
    if ($LASTEXITCODE -ne 0) { throw "MSBuild failed with exit code $LASTEXITCODE." }
}

# --- Launch ----------------------------------------------------------------
$launchArgs = @('-game', "`"$ModDir`"") + $ExtraArgs
Write-Host ">> $hl2 $($launchArgs -join ' ')" -ForegroundColor Cyan
Start-Process -FilePath $hl2 -ArgumentList $launchArgs -WorkingDirectory $SdkPath
