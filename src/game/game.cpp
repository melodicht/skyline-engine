#include <string>
#include <array>

#include <meta_definitions.h>
#include <game.h>
#include <engine.h>
#include <city_builder.h>
#include <scene.h>

#if MARVIN_GAME
#include <marvin_systems.h>
#endif

#include <movement.h>

void OnGameStart(GameState* gameState)
{
    Scene &scene = gameState->scene;
    b32 slowStep = false;

#if MARVIN_GAME
    scene.CreateVariableTimestepSystem<GravityBallsSystem>();
#endif
    scene.CreateSemifixedTimestepSystem<MovementSystem>();
    scene.CreateSemifixedTimestepSystem<BuilderSystem>(slowStep);

    assetUtils.LoadSkyboxAsset({"YokohamaSkybox/posx", "YokohamaSkybox/negx", "YokohamaSkybox/posy", "YokohamaSkybox/negy", "YokohamaSkybox/posz", "YokohamaSkybox/negz"});
}

void OnEditorStart(GameState* gameState) {}