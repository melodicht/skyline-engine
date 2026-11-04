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

// Whether the loaded map is a game mode played against others over the network.
local b32 NeedsConnection(Scene &scene)
{
    SceneView<PongMovement2D> pong(scene);
    SceneView<Fighter2D> fighters(scene);
    return pong.begin() != pong.end() || fighters.begin() != fighters.end();
}

// Player IDs are handed out in order, so these are playerId 1 and 2 in the maps.
local void AddPlayers(GameState* gameState, GameMemory* gameMemory)
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
    PlayerId player2 = AddInputPlayer(*gameMemory);

    // This device plays one player, with the same scheme as any other device.
    GatheringScheme scheme{
        .buttons = {
            {InputMoveForward, KeyboardGather(SDLK_W)},
            {InputMoveBack, KeyboardGather(SDLK_S)},
            {InputMoveLeft, KeyboardGather(SDLK_A)},
            {InputMoveRight, KeyboardGather(SDLK_D)},
            {InputPaddleUp, KeyboardGather(SDLK_W)},
            {InputPaddleDown, KeyboardGather(SDLK_S)},
            {InputFighterLeft, KeyboardGather(SDLK_A)},
            {InputFighterRight, KeyboardGather(SDLK_D)},
            {InputFighterAttack, KeyboardGather(SDLK_W)},
            {InputFighterDash, KeyboardGather(SDLK_S)},
            {InputReset, KeyboardGather(SDLK_R)},
        },
        .inputs2d = {
            {InputLook, Input2dGatherType::MouseDelta},
        },
    };
    if (NeedsConnection(gameState->scene))
    {
        network.SyncConnection(&specification, { scheme }, { player1, player2 });
    }
    else
    {
        for (const auto& [inputId, info] : scheme.buttons)
        {
            specification.AddButtonGatheringSpec(player1, inputId, info);
        }
        for (const auto& [inputId, info] : scheme.inputs2d)
        {
            specification.AddInput2dGatheringSpec(player1, inputId, info);
        }
    }
}

void OnGameStart(GameState* gameState, GameMemory* gameMemory)
{
    Scene &scene = gameState->scene;
    b32 slowStep = false;

    AddPlayers(gameState, gameMemory);

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
