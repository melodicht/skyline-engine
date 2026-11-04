#include <meta_definitions.h>
#include <filesystem>
#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <sdl_input.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

#if EMSCRIPTEN
#include <emscripten/html5.h>
#endif

#include <skl_debug.h>
#include <profiler.h>
#include <game_platform.h>
#include <render_backend.h>
#include <platform_loader.h>
#include <main.h>

#ifdef PLATFORM_WINDOWS
#define EXECUTABLE_FILE_NAME "skyline-engine.exe"
#else
#define EXECUTABLE_FILE_NAME "./skyline-engine"
#endif

#if SKL_DEBUG_MEMORY_VIEWER
DebugState globalDebugState_;
DebugState* globalDebugState = &globalDebugState_;
#endif

struct AppInformation
{
    GameCode gameCode;
    GameMemory gameMemory;
    SDL_Event e;
    const char* mapName;
    SDL_Window *window;
    u64 now;
    u64 last;
    b32 editor;
    bool playing;

    AppInformation(SDL_Window *setWindow, GameCode &gameCode, GameMemory &gameMemory, SDL_Event &setE, const char* mapName, bool setPlaying, u64 setNow, u64 setLast, b32 setEditor) :
        window(setWindow),
        gameCode(gameCode),
        gameMemory(gameMemory),
        e(setE),
        mapName(mapName),
        playing(setPlaying),
        now(setNow),
        last(setLast),
        editor(setEditor)
    { }
};

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 1200

// Kills the previous game process if it exists before creating the new game process.
local void LaunchGame(SDLState* state, const char* mapName)
{
    #if !EMSCRIPTEN
    if (state->gameProcess)
    {
        bool forceful = false;
        SDL_KillProcess(state->gameProcess, forceful);
    }

    const char* arguments[] = { EXECUTABLE_FILE_NAME, "-map", mapName, NULL };
    bool pipe_stdio = false;
    state->gameProcess = SDL_CreateProcess(arguments, pipe_stdio);
    if (!state->gameProcess)
    {
        LOG_ERROR("Failed to launch game process! SDL_ERROR: " << SDL_GetError());
    }
    #endif
}

void updateLoop(void* appInfo) {
    PROFILE_FRAMEMARK("Frame");
    AppInformation* info = (AppInformation* )appInfo;
    info->last = info->now;
    info->now = SDL_GetPerformanceCounter();

    f32 frameTime = (f32)((info->now - info->last) / (f32)SDL_GetPerformanceFrequency());

    info->gameCode.UpdateGameCode(info->gameMemory, info->editor);

    for (PlayerData& data : info->gameMemory.playerData)
    {
        data.m_inputButtonPrevDown = data.m_inputButtonDown;
    }

    b8 forceReloadGameCode = false;

    while (SDL_PollEvent(&info->e))
    {
        ImGui_ImplSDL3_ProcessEvent(&info->e);
        GatherInputsFromEvent(*info->gameMemory.inputSpecification, info->e, info->gameMemory.playerData);

        switch (info->e.type)
        {
            case SDL_EVENT_QUIT:
                info->playing = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (info->e.key.key == SDLK_ESCAPE)
                {
                    info->playing = false;
                }
#if SKL_INTERNAL
                else if (info->e.key.key == SDLK_0)
                {
                    if (!info->e.key.repeat)
                    {
                        LoopUtils::ToggleLoopedLiveEditingState(&globalSDLState, info->gameMemory.playerData, &info->gameMemory.accumulatedPlayerData);
                    }
                }
                else if (info->e.key.key == SDLK_1)
                {
                    if (LoopUtils::GetIsStateInPlayback(&globalSDLState))
                    {
                        forceReloadGameCode = true;
                    }
                }
#endif
                else if (info->editor && info->e.key.key == SDLK_R && (SDL_GetModState() & SDL_KMOD_CTRL))
                {
                    LaunchGame(&globalSDLState, info->mapName);
                }
                break;
        }
    }

    GatherInputs(*info->gameMemory.inputSpecification, info->gameMemory.playerData);

    s32 windowWidth = WINDOW_WIDTH;
    s32 windowHeight = WINDOW_HEIGHT;
    SDL_GetWindowSize(info->window, &windowWidth, &windowHeight);

    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    b32 shouldReloadGameCode = LoopUtils::ProcessInputWithLooping(&globalSDLState, *info->gameMemory.inputSpecification, info->gameMemory.playerData, &info->gameMemory.accumulatedPlayerData, forceReloadGameCode);
    if (shouldReloadGameCode)
    {
        info->gameCode.Load(info->gameMemory, info->editor, true);
    }
    
    info->gameCode.UpdateAndRender(info->gameMemory, frameTime);

    f32 msPerFrame =  1000.0f * frameTime;
    f32 fps = 1 / frameTime;
    //printf("%.02f ms/frame (FPS: %.02f)\n", msPerFrame, fps);
    // Advance Tracy's default frame track once per application frame, not per
    // fixed tick or profiling zone. This also survives game-module reloads.
    PROFILE_FRAME_END();
    return;
}

int main(int argc, char** argv)
{
    // Initialize and retain the shared Tracy client from the non-reloadable
    // executable. Reloading game code must never start/stop a profiler client.
    PROFILE_INITIALIZE();
    std::cout << "Current path: " << std::filesystem::current_path() << std::endl;
    srand(static_cast<unsigned>(time(0)));

    InitSDLState(&globalSDLState);

    SDL_Window *window = NULL;
    SDL_Surface *screenSurface = NULL;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Skyline Engine", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE | GetRenderWindowFlags());
    if (window == NULL)
    {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    ImGuiContext *imGuiContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplSDL3_InitForOther(window);

    std::string mapName = "start";
    bool editor = false;
    bool recordOnStart = false;
    bool useNetwork = false;

    for (int i = 0; i < argc; i++)
    {
        if (!strcmp(argv[i], "-editor"))
        {
            editor = true;
        }
        else if ((!strcmp(argv[i], "-map")) && ((++i) < argc))
        {
            mapName = argv[i];
        }
        else if (!strcmp(argv[i], "-network"))
        {
            useNetwork = true;
        }
        else if (!strcmp(argv[i], "-record-on-start"))
        {
            recordOnStart = true;
        }
    }

    if (!editor)
    {
        SDL_SetWindowRelativeMouseMode(window, true);
    }


    RenderInitInfo initDesc
    {
        .window = window,
        .startWidth = WINDOW_WIDTH,
        .startHeight = WINDOW_HEIGHT,
        .editor = editor
    };
    InitRenderer(initDesc);

    RenderPipelineInitInfo pipelinesInfo;
    InitPipelines(pipelinesInfo);

    GameCode gameCode{ editor };

    GameMemory gameMemory = {};
#if SKL_DEBUG_MEMORY_VIEWER
    gameMemory.debugState = globalDebugState;
#endif
    gameMemory.imGuiContext = imGuiContext;
    gameMemory.useNetwork = useNetwork;
    InputSpecification inputSpecification(0, 0, {}, {});
    gameMemory.inputSpecification = &inputSpecification;
    gameMemory.platformAPI.assetUtils = constructPlatformAssetUtils();
    gameMemory.platformAPI.renderer = constructPlatformRenderer();
    gameMemory.platformAPI.network = constructPlatformNetwork();
    gameMemory.platformAPI.allocator = constructPlatformAllocator();
    gameCode.Load(gameMemory, editor, false);
    gameCode.Initialize(gameMemory, mapName, editor);

    SDL_Event e;
    bool playing = true;

    if (recordOnStart)
    {
        LoopUtils::ToggleLoopedLiveEditingState(&globalSDLState, gameMemory.playerData, &gameMemory.accumulatedPlayerData);
    }

    u64 now = SDL_GetPerformanceCounter();
    u64 last = 0;
    AppInformation app = AppInformation(window, gameCode, gameMemory, e, mapName.c_str(), playing, now, last, editor);
    #if EMSCRIPTEN
    emscripten_set_main_loop_arg(
        [](void* userData) {
            updateLoop(userData);
        }, 
        (void*)&app, 
        0, true
    );
    #else
    while (app.playing)
    {
        updateLoop(&app);
    }
    #endif
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
