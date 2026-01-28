#include <string>

#include <game.h>
#include <engine.h>
#include <city_builder.h>
#include <scene.h>

#if MARVIN_GAME
#include <marvin_systems.h>
#endif

#include <movement.h>

void OnGameStart(GameState* gameState, std::string mapName)
{
    Scene &scene = gameState->scene;
    b32 slowStep = false;

#if MARVIN_GAME
    scene.CreateVariableTimestepSystem<GravityBallsSystem>();
#endif
    scene.CreateSemifixedTimestepSystem<MovementSystem>();
    scene.CreateSemifixedTimestepSystem<BuilderSystem>(slowStep);
}