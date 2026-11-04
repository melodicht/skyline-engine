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

// The controls of the demo game modes, for a single player.
local GatheringScheme MakeDemoScheme(SDL_Keycode up, SDL_Keycode down, SDL_Keycode left, SDL_Keycode right)
{
    return GatheringScheme{
        .buttons = {
            {InputPaddleUp, KeyboardGather(up)},
            {InputPaddleDown, KeyboardGather(down)},
            {InputFighterLeft, KeyboardGather(left)},
            {InputFighterRight, KeyboardGather(right)},
            {InputFighterAttack, KeyboardGather(up)},
            {InputFighterDash, KeyboardGather(down)},
            {InputReset, KeyboardGather(SDLK_R)},
        },
    };
}

local void ApplyScheme(InputSpecification &specification, PlayerId playerId, const GatheringScheme &scheme)
{
    for (const auto& [inputId, info] : scheme.buttons)
    {
        specification.AddButtonGatheringSpec(playerId, inputId, info);
    }
    for (const auto& [inputId, info] : scheme.inputs2d)
    {
        specification.AddInput2dGatheringSpec(playerId, inputId, info);
    }
}

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
    PlayerId player2 = AddInputPlayer(*gameMemory);

    if (gameMemory->useNetwork)
    {
        // This device plays one player, with the same scheme as any other device.
        GatheringScheme scheme = MakeDemoScheme(SDLK_W, SDLK_S, SDLK_A, SDLK_D);
        scheme.buttons.insert(scheme.buttons.end(), {
            {InputMoveForward, KeyboardGather(SDLK_W)},
            {InputMoveBack, KeyboardGather(SDLK_S)},
            {InputMoveLeft, KeyboardGather(SDLK_A)},
            {InputMoveRight, KeyboardGather(SDLK_D)},
        });
        scheme.inputs2d.push_back({InputLook, Input2dGatherType::MouseDelta});
        network.SyncConnection(&specification, { scheme }, { player1, player2 });
    }
    else
    {
        // Both players share this keyboard.
        AddMovementGathering(specification, player1);
        ApplyScheme(specification, player1, MakeDemoScheme(SDLK_W, SDLK_S, SDLK_A, SDLK_D));
        ApplyScheme(specification, player2, MakeDemoScheme(SDLK_I, SDLK_K, SDLK_J, SDLK_L));
    }
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
