#pragma once

#include "renderer/render_types.h"

#include <vector>

struct MeshAsset
{
    MeshID id;
    AABB aabb;
};

struct TextureAsset
{
    u32 width;
    u32 height;

    TextureID id;
};
