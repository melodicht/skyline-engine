#include <game.h>

#include <string>
#include <array>

#include <meta_definitions.h>
#include <engine.h>
#include <city_builder.h>
#include <scene.h>
#include <scene_view.h>

#include <movement.h>

#include <game_components.h>

#include <physics.h>
#include <utils.h>

void OnGameStart(GameState* gameState, GameMemory* gameMemory)
{
    Scene &scene = gameState->scene;
    b32 slowStep = false;

    scene.CreateSemifixedTimestepSystem<MovementSystem>();
    scene.CreateSemifixedTimestepSystem<BuilderSystem>(slowStep);

    assetUtils.LoadSkyboxAsset({"YokohamaSkybox/posx", "YokohamaSkybox/negx", "YokohamaSkybox/posy", "YokohamaSkybox/negy", "YokohamaSkybox/posz", "YokohamaSkybox/negz"});
}

void OnGameLoad(GameMemory* gameMemory) {}

void OnEditorStart(GameState* gameState) {}
