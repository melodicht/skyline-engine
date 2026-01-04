#pragma once

#include "meta_definitions.h"
#include "game_platform.h"
#include "memory.h"
#include "ecs.h"
#include "skl_thread_safe_primitives.h"
#include "debug.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// NOTE(marvin): Took Jolt includes from
// https://github.com/jrouwe/JoltPhysicsHelloWorld/blob/main/Source/HelloWorld.cpp

// The Jolt headers don't include Jolt.h. Always include Jolt.h before including any other Jolt header.
// You can use Jolt.h in your precompiled header to speed up compilation.
#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include "renderer/render_types.h"

// TODO(marvin): Does overlay mode really belong in this file?
enum OverlayMode
{
    overlayMode_none      = 0,
    overlayMode_ecsEditor = 1,
    overlayMode_memory    = 2,
};

struct GameState
{
    Scene scene;

    // TODO(marvin): Overlay mode is a shared between ecs editor and debug mode. Ideally in a different struct or compiled away for the actual game release. However, because ecs editor is part of game release, cannot be compiled away.
    // NOTE(marvin): In actual release, overlay mode should only be none, and is never checked.
    OverlayMode overlayMode;
};
