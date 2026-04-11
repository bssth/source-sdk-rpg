<#
.SYNOPSIS
    Creates NTFS junctions inside the mod folder pointing at the user's
    installed Half-Life 2 / Episode One / Episode Two / SDK Base content.

.DESCRIPTION
    Source SDK 2013 SP doesn't actually mount HL2/EP1/EP2 content from Steam
    when running standalone tools (vbsp/vrad/vvis built from this repo) — and
    the AdditionalContentId gameinfo key is a no-op in this codebase (the
    MountSteamContent call is commented out in filesystem_init.cpp).

    To make both the engine AND the compile tools see the real game files, we
    map them into the mod folder via NTFS directory junctions:

        sp/game/mod_sdk2013ce/_steam/hl2       -> <HL2 install>/hl2
        sp/game/mod_sdk2013ce/_steam/episodic  -> <HL2 install>/episodic   (or <EP1 install>/episodic)
        sp/game/mod_sdk2013ce/_steam/ep2       -> <HL2 install>/ep2        (or <EP2 install>/ep2)
        sp/game/mod_sdk2013ce/_steam/platform  -> <SDK Base>/platform

    gameinfo.txt references these via |gameinfo_path|_steam/... so the same
    paths work for hl2.exe, vbsp.exe, vrad.exe, vvis.exe — regardless of cwd.

    Both the new combined "Half-Life 2" Steam app (containing hl2/episodic/ep2
    as subdirs) and the legacy separate "Half-Life 2 Episode One"/"...Episode
    Two" apps are supported.

.PARAMETER SdkPath
    Path to "Source SDK Base 2013 Singleplayer" (folder containing hl2.exe).
.PARAMETER Hl2Path
    Path to the HL2 install (folder containing hl2/, optionally episodic/ and ep2/).
.PARAMETER Ep1Path
    Path to the EP1 install (folder containing episodic/). Optional override.
.PARAMETER Ep2Path
    Path to the EP2 install (folder containing ep2/). Optional override.
.PARAMETER ModDir
    Mod folder. Default: <sp>/game/mod_sdk2013ce.
.PARAMETER Force
    Recreate junctions even if they already exist and point at the right place.
#>
[CmdletBinding()]
param(
    [string] $SdkPath,
    [string] $Hl2Path,
    [string] $Ep1Path,
    [string] $Ep2Path,
    [string] $ModDir,
    [switch] $Force
)

$ErrorActionPreference = 'Stop'

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$LocalCfg = Join-Path $ScriptDir 'launch.local.ps1'
if (Test-Path $LocalCfg) { . $LocalCfg }

function Get-SteamLibraries {
    $steamKey = Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue
    if (-not $steamKey) {
        $steamKey = Get-ItemProperty -Path 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam' -ErrorAction SilentlyContinue
    }
    if (-not $steamKey) { return @() }

    $steamRoot = @($steamKey.SteamPath, $steamKey.InstallPath | Where-Object { $_ })[0]
    if (-not $steamRoot) { return @() }
    $steamRoot = ([string]$steamRoot).Replace('/','\')

    $libs = @($steamRoot)
    foreach ($vdf in @(
        (Join-Path $steamRoot 'config\libraryfolders.vdf'),
        (Join-Path $steamRoot 'steamapps\libraryfolders.vdf')
    )) {
        if (Test-Path $vdf) {
            $text = Get-Content -LiteralPath $vdf -Raw
            foreach ($m in [regex]::Matches($text, '"path"\s*"([^"]+)"')) {
                $p = $m.Groups[1].Value -replace '\\\\','\'
                if ($libs -notcontains $p) { $libs += $p }
            }
            break
        }
    }
    return $libs
}

function Find-InLibraries {
    param([string[]] $Libs, [string[]] $Candidates)
    foreach ($lib in $Libs) {
        foreach ($name in $Candidates) {
            $p = Join-Path $lib (Join-Path 'steamapps\common' $name)
            if (Test-Path $p) { return $p }
        }
    }
    return $null
}

# --- Resolve all the source paths -----------------------------------------
$libs = Get-SteamLibraries

if (-not $SdkPath) { $SdkPath = $env:SOURCE_SDK_BASE_2013 }
if (-not $SdkPath -and $SourceSdkBase2013) { $SdkPath = $SourceSdkBase2013 }
if (-not $SdkPath) { $SdkPath = Find-InLibraries $libs @('Source SDK Base 2013 Singleplayer') }
if (-not $SdkPath -or -not (Test-Path (Join-Path $SdkPath 'hl2.exe'))) {
    throw "Source SDK Base 2013 Singleplayer not found. Pass -SdkPath '<path>'."
}

if (-not $Hl2Path) { $Hl2Path = Find-InLibraries $libs @('Half-Life 2') }
if (-not $Hl2Path -or -not (Test-Path (Join-Path $Hl2Path 'hl2'))) {
    throw "Half-Life 2 install not found. Pass -Hl2Path '<path>'."
}

# EP1: prefer override > $Hl2Path/episodic > legacy "Half-Life 2 Episode One/episodic"
$ep1Source = $null
if ($Ep1Path) {
    $ep1Source = Join-Path $Ep1Path 'episodic'
} elseif (Test-Path (Join-Path $Hl2Path 'episodic')) {
    $ep1Source = Join-Path $Hl2Path 'episodic'
} else {
    $legacy = Find-InLibraries $libs @('Half-Life 2 Episode One')
    if ($legacy) { $ep1Source = Join-Path $legacy 'episodic' }
}

# EP2: same idea
$ep2Source = $null
if ($Ep2Path) {
    $ep2Source = Join-Path $Ep2Path 'ep2'
} elseif (Test-Path (Join-Path $Hl2Path 'ep2')) {
    $ep2Source = Join-Path $Hl2Path 'ep2'
} else {
    $legacy = Find-InLibraries $libs @('Half-Life 2 Episode Two')
    if ($legacy) { $ep2Source = Join-Path $legacy 'ep2' }
}

# --- Resolve mod dir -------------------------------------------------------
if (-not $ModDir) { $ModDir = $env:BB_MOD_DIR }
if (-not $ModDir) { $ModDir = Join-Path $ScriptDir 'game\mod_sdk2013ce' }
if (-not (Test-Path (Join-Path $ModDir 'gameinfo.txt'))) {
    throw "gameinfo.txt not found in mod dir: $ModDir"
}
$ModDir = (Resolve-Path -LiteralPath $ModDir).Path

$SteamRoot = Join-Path $ModDir '_steam'
if (-not (Test-Path $SteamRoot)) {
    New-Item -ItemType Directory -Path $SteamRoot | Out-Null
}

# --- Junction definitions --------------------------------------------------
# IMPORTANT: the primary source for hl2/ep2/episodic/platform is SDK Base 2013 SP,
# because it ships content files (shaders, scripts, HudLayout.res, HudAnimations_HL2.txt)
# that MATCH its own stdshader_dx9.dll and KeyValues parser.
#
# Half-Life 2 "Anniversary" ships updated versions of the same files that break the
# vanilla SDK 2013 engine ([!$DECK] conditionals in HudAnimations, shader combos the
# old stdshader_dx9.dll doesn't know about, different HudLayout entry names, etc).
#
# SDK Base is however missing hl2_pak.vpk (the main HL2 models/maps pak), so we mount
# Anniversary HL2's hl2/ separately as _steam/hl2_game and pull only hl2_pak.vpk from it.

$junctions = @(
    @{ Name = 'hl2';      Target = (Join-Path $SdkPath 'hl2') }
    @{ Name = 'episodic'; Target = (Join-Path $SdkPath 'episodic') }
    @{ Name = 'ep2';      Target = (Join-Path $SdkPath 'ep2') }
    @{ Name = 'platform'; Target = (Join-Path $SdkPath 'platform') }
    @{ Name = 'hl2_game'; Target = (Join-Path $Hl2Path 'hl2') }
)

function Get-JunctionTarget {
    param([string] $Path)
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
    if (-not $item) { return $null }
    if (-not ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { return $null }
    return $item.Target
}

foreach ($j in $junctions) {
    $link   = Join-Path $SteamRoot $j.Name
    $target = $j.Target

    if (-not $target) {
        Write-Host ("[skip] {0,-8} - source not found" -f $j.Name) -ForegroundColor Yellow
        continue
    }
    if (-not (Test-Path -LiteralPath $target)) {
        Write-Host ("[skip] {0,-8} - target missing: {1}" -f $j.Name, $target) -ForegroundColor Yellow
        continue
    }

    $existing = Get-JunctionTarget $link
    if ($existing) {
        $same = ($existing -is [array] -and $existing[0] -ieq $target) -or ($existing -ieq $target)
        if ($same -and -not $Force) {
            Write-Host ("[ok]   {0,-8} -> {1}" -f $j.Name, $target) -ForegroundColor DarkGray
            continue
        }
        # Remove stale junction (this only removes the link, not the target)
        cmd /c rmdir """$link""" | Out-Null
    } elseif (Test-Path -LiteralPath $link) {
        throw "$link exists and is not a junction. Remove it manually."
    }

    cmd /c mklink /J """$link""" """$target""" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to create junction: $link -> $target"
    }
    Write-Host ("[new]  {0,-8} -> {1}" -f $j.Name, $target) -ForegroundColor Green
}

Write-Host ""
Write-Host "Done. Mod content root: $SteamRoot" -ForegroundColor Cyan
