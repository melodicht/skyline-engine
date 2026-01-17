#pragma once

#include <game_platform.h>
#include <meta_definitions.h>
#include <scene.h>

enum OverlayMode
{
    overlayMode_none      = 0,
    overlayMode_ecsEditor = 1,
    overlayMode_memory    = 2,
};

struct GameState
{
    Scene scene;

    EntityID currentCamera = -1;
    b32 isEditor;

    // TODO(marvin): Overlay mode is a shared between ecs editor and debug mode. Ideally in a different struct or compiled away for the actual game release. However, because ecs editor is part of game release, cannot be compiled away.
    // NOTE(marvin): In actual release, overlay mode should only be none, and is never checked.
    OverlayMode overlayMode;
};

extern PlatformAssetUtils assetUtils;
extern PlatformRenderer renderer;
extern PlatformAllocator allocator;
