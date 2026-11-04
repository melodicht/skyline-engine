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

// Player IDs are handed out in order, so these are playerId 1 and 2 in the maps.
local void AddPlayers(GameMemory* gameMemory)
{
    InputSpecification &specification = *gameMemory->inputSpecification;
    AddMovementInputSpecs(specification);
    specification.AddButtonSpec(InputPaddleUp);
    specification.AddButtonSpec(InputPaddleDown);
    specification.AddButtonSpec(InputFighterLeft);
    specification.AddButtonSpec(InputFighterRight);
    specification.AddButtonSpec(InputFighterAttack);
    specification.AddButtonSpec(InputFighterDash);
    specification.AddButtonSpec(InputReset);

    PlayerId player1 = AddInputPlayer(*gameMemory);
    AddMovementGathering(specification, player1);
    specification.AddButtonGatheringSpec(player1, InputPaddleUp, KeyboardGather(SDLK_W));
    specification.AddButtonGatheringSpec(player1, InputPaddleDown, KeyboardGather(SDLK_S));
    specification.AddButtonGatheringSpec(player1, InputFighterLeft, KeyboardGather(SDLK_A));
    specification.AddButtonGatheringSpec(player1, InputFighterRight, KeyboardGather(SDLK_D));
    specification.AddButtonGatheringSpec(player1, InputFighterAttack, KeyboardGather(SDLK_W));
    specification.AddButtonGatheringSpec(player1, InputFighterDash, KeyboardGather(SDLK_S));
    specification.AddButtonGatheringSpec(player1, InputReset, KeyboardGather(SDLK_R));

    PlayerId player2 = AddInputPlayer(*gameMemory);
    specification.AddButtonGatheringSpec(player2, InputPaddleUp, KeyboardGather(SDLK_I));
    specification.AddButtonGatheringSpec(player2, InputPaddleDown, KeyboardGather(SDLK_K));
    specification.AddButtonGatheringSpec(player2, InputFighterLeft, KeyboardGather(SDLK_J));
    specification.AddButtonGatheringSpec(player2, InputFighterRight, KeyboardGather(SDLK_L));
    specification.AddButtonGatheringSpec(player2, InputFighterAttack, KeyboardGather(SDLK_I));
    specification.AddButtonGatheringSpec(player2, InputFighterDash, KeyboardGather(SDLK_K));
    specification.AddButtonGatheringSpec(player2, InputReset, KeyboardGather(SDLK_R));
}

void OnGameStart(GameState* gameState, GameMemory* gameMemory)
{
    Scene &scene = gameState->scene;
    b32 slowStep = false;

    AddPlayers(gameMemory);

    scene.CreateFixedTimestepSystem<FlyingMovementSystem>();
    scene.CreateFixedTimestepSystem<HorizontalLookSystem>();
    scene.CreateFixedTimestepSystem<VerticalLookSystem>();
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
