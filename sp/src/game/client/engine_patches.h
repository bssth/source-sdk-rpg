// Small fixes to engine.dll that can't be made in source, applied in memory
// when the client DLL loads.

#ifndef ENGINE_PATCHES_H
#define ENGINE_PATCHES_H
#ifdef _WIN32
#pragma once
#endif

void EnginePatches_Init();
void EnginePatches_Shutdown();

#endif // ENGINE_PATCHES_H
