# Project context for Claude

## What this repo is

Source SDK 2013 Singleplayer mod, fork of **[Source SDK 2013 Community Edition](https://github.com/Nbc66/source-sdk-2013-ce)** (itself a clean fork of Valve's `valveSoftware/source-sdk-2013`). Upstream is **not** Mapbase — individual Mapbase fixes are cherry-picked with `[Mapbase port]` in the commit message, but `MAPBASE` is not defined anywhere in the tree.

The mod itself is **"Half-Life: BB"** (`title "HALF-LIFE'"`, `game "Half-Life: BB"` in `game/mod_sdk2013ce/gameinfo.txt`). Runs on Steam AppID **243730** (Source SDK Base 2013 Singleplayer). SP only — MP has been explicitly stripped (`6fff51b Removed MP subbranch`).

Git remote: `git@github.com:bssth/source-sdk-bb-temp.git`. Branch: `master`.

## Repo layout

The git repo root is **`D:/dev/source-sdk-bb-temp/`** (one level up from the working directory).
Inside it:

```
/                        ← repo root, holds .gitignore, .github/, README.md, LICENSE
├── sp/                  ← working directory; this is the mod
│   ├── CLAUDE.md        ← this file
│   ├── launch.ps1       ← build + launch script (see "Build & run")
│   ├── launch.bat       ← cmd wrapper for launch.ps1
│   ├── launch.local.example.ps1
│   ├── game/
│   │   ├── mod_sdk2013ce/      ← THE mod game folder (gameinfo.txt, cfg/, maps, etc.)
│   │   │   └── bin/            ← client.dll / server.dll land here via post-build copy
│   │   ├── hl2/, ep2/, episodic/, lostcoast/   ← vanilla game data (reference)
│   │   └── mod_hl2/, mod_episodic/              ← other mod folder variants
│   └── src/
│       ├── Everything.sln             ← THE solution (committed, not generated anymore)
│       ├── .run/Launch Mod.run.xml    ← Rider run configuration (committed)
│       ├── .idea/                     ← per-user, gitignored
│       ├── game/
│       │   ├── client/    → builds Client project → client.dll
│       │   ├── server/    → builds Server project → server.dll
│       │   ├── shared/    → code compiled into both
│       │   └── gamepadui/ → builds GamepadUI project
│       ├── tier1/, mathlib/, raytrace/, fgdlib/, vgui2/vgui_controls/
│       ├── utils/         → vbsp, vrad, vvis, vtfdiff, captioncompiler, etc.
│       ├── public/        → headers shared across the SDK
│       ├── lib/public/    → prebuilt static libs (tier0, vstdlib, mathlib, tier1, ...)
│       ├── materialsystem/, togl/, thirdparty/, gcsdk/, common/
│       └── devtools/      → build helpers (perl shader scripts etc.)
└── .github/workflows/
    ├── SP_windows.yml    ← CI for this branch
    └── MP_windows.yml    ← stale; MP was removed from the codebase
```

## Build system

- **Toolset:** `v143` (VS2022) on all 21 `.vcxproj` files. Projects were migrated through `v142 → v143` in the upstream (commits `8beb696`, `ebbf56e`). Do not "retarget" or downgrade.
- **Configuration / platform:** only `Debug|Win32` and `Release|Win32`. No x64. No MultiByte → Unicode switch (`CharacterSet` is `MultiByte`).
- **Prebuilt libs in `src/lib/public/`** (tier0, vstdlib, mathlib, tier1, raytrace, fgdlib, appframework, bitmap, choreoobjects, matsys_controls, particles, …) are updated alongside toolset bumps. Last refresh: `aea44b2 Public libs update`. If link errors appear after a toolset change, the libs are the first suspect.
- **`libprotobuf.lib`** ships in two variants: `src/lib/public/2010/` and `src/lib/public/2012/`. Pay attention when touching protobuf-related code — the wrong variant gives LNK2038.
- **Output dirs:** `src/<project>/{Debug,Release}_mod_sdk2013ce/`. Post-build events copy `client.dll` / `server.dll` / `.pdb` into `sp/game/mod_sdk2013ce/bin/`. No manual copy needed.
- **Solution projects (16):** Client, Server, GamepadUI, Tier1, Mathlib, Raytrace, Fgdlib, Vgui_controls, and the utils set (Vbsp, Vrad_dll, Vrad_launcher, Vvis_dll, Vvis_launcher, Vtfdiff, Glview, Motionmapper, Tgadiff, Captioncompiler, Vice, Height2normal). **Tier1/Mathlib/Raytrace/Fgdlib/Vgui_controls rebuild their lib from source** — these override the prebuilt copies in `src/lib/public/`.

### No more VPC

VPC (`devtools/bin/vpc.exe`, `vpc_scripts/`, all `*.vpc` files, `createallprojects[.bat]`, `creategameprojects[.bat]`) was **removed from this repo**. Everything.sln + all 21 .vcxproj/.filters are committed directly. `.gitignore` was patched to un-ignore `*.sln`/`*.vcxproj`/`*.filters`.

**Known fallout:** `.github/workflows/SP_windows.yml` still runs `devtools/bin/vpc.exe /sdk2013ce /2013 +game /mksln sdk2013ce.sln` — this will fail on CI. When/if CI matters again, rewrite the workflow to do `msbuild sp/src/Everything.sln /p:Configuration=Release /p:Platform=Win32` instead. The user is aware; decision is "fix later".

## Build & run (the `launch.ps1` system)

One-shot build+launch lives in `sp/launch.ps1`. It's what the Rider **Launch Mod** run config invokes. The Rider config is a `ShConfigurationType` shell-script config; it deliberately has no before-launch Build step because the script runs MSBuild itself (so running from Rider and running from a terminal behave identically).

**`launch.ps1` resolution order for the SDK install path:**
1. `-SdkPath` parameter
2. `$env:SOURCE_SDK_BASE_2013`
3. `$SourceSdkBase2013` set inside `sp/launch.local.ps1` (gitignored; see `launch.local.example.ps1`)
4. Auto: `HKCU\Software\Valve\Steam` → parse `config/libraryfolders.vdf` → probe every library for `steamapps\common\Source SDK Base 2013 Singleplayer\hl2.exe`

**Mod dir resolution:** `-ModDir` → `$env:BB_MOD_DIR` → `$ModDir` from local cfg → default `sp/game/mod_sdk2013ce`.

**MSBuild resolution:** `msbuild.exe` on PATH → `vswhere.exe` (`-latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe'`).

**Build target:** `/t:Client;Server /p:Configuration=$Configuration /p:Platform=Win32 /m` against `src/Everything.sln`. Only builds the two dll projects — deliberately narrow because that's what the mod needs at runtime. For utils (vbsp/vrad/vvis) you build manually through Rider or MSBuild.

**Launch command:** `hl2.exe -game "<mod>" -dev -console -allowdebug` (arguments replaceable via `-ExtraArgs`).

**Parameters:** `-SdkPath`, `-ModDir`, `-Configuration Debug|Release` (default Debug), `-NoBuild`, `-ExtraArgs`.

**Verified on this machine (2026-04-11):** Steam lives at `c:/program files (x86)/steam`, library folders include `E:\SteamLibrary`, SDK resolved to `E:\SteamLibrary\steamapps\common\Source SDK Base 2013 Singleplayer`. `vswhere` finds MSBuild at `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`.

## Mod-specific code

Files that are this fork's own work (not upstream SDK, not upstream CE):

- `src/game/server/bb_inventory.{cpp,h}` — self-made inventory system (`CBBInventory`, `CBBItemType`). No GUI yet (`6a5b1a5 Base self-made inventory system (no GUI for now)`).
- `src/game/shared/hl2/bb_gamerules.{cpp,h}` — game rules class for the mod.
- Assorted behavior tweaks in `src/game/server/hl2/` (flaregun, metrocop behaviors, player reaction to events, NPC hover info, gamepad support, HUD edits — see recent commit subjects for context).

Treat anything named `bb_*` or mentioned in the last ~20 commits as the mod's own code; everything else is stock SDK2013 CE / Valve code that should be touched conservatively.

## Conventions / gotchas

- **Asserts:** the user has been systematically removing asserts that fire but can't be fixed right now (`2432cfc Another asserts remove`, `9f83c14 Removed some asserts we cannot fix for now`). Prefer deleting a noisy assert over adding a workaround, and match the existing style.
- **`#if 0` cleanup is in progress** (`df85f23 #if 0 cleanup`). Don't add new `#if 0` blocks; delete dead code outright.
- **Crosshair removed** in this branch (`a97b7af`) — don't reintroduce upstream crosshair behavior without checking.
- **MP code is gone** (`6fff51b`). `hl2mp/` and `Multiplayer/` directories still exist under `src/game/{client,server,shared}/` because they're not referenced by the Client/Server projects — treat them as dead weight, don't build against them.
- **Line endings:** repo contains CRLF vcxproj files; git `autocrlf` warnings are expected and harmless.
- **`.gitignore`:** lives at the **repo root** (`D:/dev/source-sdk-bb-temp/.gitignore`), not inside `sp/`. When adding ignores, patch that file. `.idea/`, `*.iml`, `sp/launch.local.ps1` are the per-user entries. `*.vpc_crc` and `*.sentinel` are ignored but no longer generated.

## Modernizing / upgrading notes

- Upgrade to `v143` is already done. If `MSB8020` shows up, it's the local VS2022 tooling missing, not the code:
  - VS2022 Installer → Desktop development with C++
  - Components: `MSVC v143 - VS 2022 C++ x86/x64 build tools`, `Windows 11 SDK`, `C++ MFC/ATL for v143` (the SDK brushes ATL in places)
- No ongoing v120/v142 compatibility — nothing to preserve there.

## Useful commands

```bash
# From sp/:
./launch.bat                            # build Debug + launch
powershell -File launch.ps1 -NoBuild    # just launch, skip MSBuild
powershell -File launch.ps1 -Configuration Release

# Find all mod-specific code:
grep -rn "class C\w*BB\|bb_" src/game --include="*.h"
```

## Open threads / future work

- CI workflow `SP_windows.yml` needs to be rewritten to build `Everything.sln` directly (VPC removal fallout — see above).
- GUI for the inventory system is not yet implemented.
- `hl2mp/` subtrees under `src/game/` should probably be deleted at some point — they're noise when searching.
