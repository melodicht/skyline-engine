#include <game.h>

#include <string>
#include <array>

#include <meta_definitions.h>
#include <engine.h>
#include <city_builder.h>
#include <scene.h>
#include <scene_view.h>

#include <movement.h>
#include <demo_systems.h>

#include <game_components.h>
#include <engine_components.h>

#include <physics.h>
#include <utils.h>

void OnGameStart(GameState* gameState, GameMemory* gameMemory)
{
    Scene &scene = gameState->scene;
    b32 slowStep = false;

    scene.CreateFixedTimestepSystem<MovementSystem>();
    scene.CreateFixedTimestepSystem<BuilderSystem>(slowStep);
    scene.CreateFixedTimestepSystem<PongMovement2DSystem>();
    scene.CreateFixedTimestepSystem<RectCollision2DSystem>();
    scene.CreateFixedTimestepSystem<PongBall2DSystem>();
    scene.CreateFixedTimestepSystem<Fighter2DSystem>();
    scene.CreateFixedTimestepSystem<FightBall2DSystem>();

    assetUtils.LoadSkyboxAsset({"YokohamaSkybox/posx", "YokohamaSkybox/negx", "YokohamaSkybox/posy", "YokohamaSkybox/negy", "YokohamaSkybox/posz", "YokohamaSkybox/negz"});
}

void OnGameLoad(GameMemory* gameMemory) {}

void OnGameGetPersistentDLLPaths(const char** pathBuffer) {}

void OnEditorStart(GameState* gameState) {}
