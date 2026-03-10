#include <meta_definitions.h>
#include <format>
#include <filesystem>
#include <iostream>

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

#if EMSCRIPTEN
#include <emscripten/html5.h>
#endif

#include <debug.h>
#include <game_platform.h>
#include <render_backend.h>
#include <platform_loader.h>
#include <main.h>

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

file_global std::set<std::string> keysDown;
file_global f32 mouseDeltaX = 0;
file_global f32 mouseDeltaY = 0;

file_global f32 mouseX = 0;
file_global f32 mouseY = 0;

// Kills the previous game process if it exists before creating the new game process.
local void LaunchGame(SDLState* state, const char* mapName)
{
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
}

void updateLoop(void* appInfo) {
    AppInformation* info = (AppInformation* )appInfo;
    info->last = info->now;
    info->now = SDL_GetPerformanceCounter();

    f32 frameTime = (f32)((info->now - info->last) / (f32)SDL_GetPerformanceFrequency());

    info->gameCode.updateGameCode(info->gameMemory, info->editor);

    GameInput gameInput;
    gameInput.keysDownPrevFrame = keysDown;

    while (SDL_PollEvent(&info->e))
    {
        ImGui_ImplSDL3_ProcessEvent(&info->e);

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
                else if (info->e.key.key == SDLK_L)
                {
                    LoopUtils::ToggleLoopedLiveEditingState(&globalSDLState);
                }
#endif
                else if (info->editor && info->e.key.key == SDLK_R && (SDL_GetModState() & SDL_KMOD_CTRL))
                {
                    LaunchGame(&globalSDLState, info->mapName);
                }
                keysDown.insert(SDL_GetKeyName(info->e.key.key));
                break;
            case SDL_EVENT_KEY_UP:
                keysDown.erase(SDL_GetKeyName(info->e.key.key));
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                keysDown.insert(std::format("Mouse {}", info->e.button.button));
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                keysDown.erase(std::format("Mouse {}", info->e.button.button));
                break;
        }
    }

    SDL_GetRelativeMouseState(&mouseDeltaX, &mouseDeltaY);
    SDL_GetMouseState(&mouseX, &mouseY);

    s32 windowWidth = WINDOW_WIDTH;
    s32 windowHeight = WINDOW_HEIGHT;
    SDL_GetWindowSize(info->window, &windowWidth, &windowHeight);

    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    
    if (io.WantCaptureMouse)
    {
        keysDown.erase("Mouse 1");
    }

    gameInput.mouseDeltaX = mouseDeltaX;
    gameInput.mouseDeltaY = mouseDeltaY;
    gameInput.mouseX = mouseX;
    gameInput.mouseY = mouseY;
    gameInput.keysDownThisFrame = keysDown;

    b32 shouldReloadGameCode = LoopUtils::ProcessInputWithLooping(&globalSDLState, &gameInput);
    if (shouldReloadGameCode)
    {
        info->gameCode.gameLoad(info->gameMemory, info->editor, true);
    }
    
    info->gameCode.gameUpdateAndRender(info->gameMemory, gameInput, frameTime);

    mouseDeltaX = 0;
    mouseDeltaY = 0;

    f32 msPerFrame =  1000.0f * frameTime;
    f32 fps = 1 / frameTime;
    //printf("%.02f ms/frame (FPS: %.02f)\n", msPerFrame, fps);
    return;
}


int main(int argc, char** argv)
{
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

    for (int i = 0; i < argc; i++)
    {
        if (!strcmp(argv[i], "-editor"))
        {
            editor = true;
        }
        if ((!strcmp(argv[i], "-map")) && ((++i) < argc))
        {
            mapName = argv[i];
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
    gameMemory.platformAPI.assetUtils = constructPlatformAssetUtils();
    gameMemory.platformAPI.renderer = constructPlatformRenderer();
    gameMemory.platformAPI.allocator = constructPlatformAllocator();
    gameCode.gameLoad(gameMemory, editor, false);
    gameCode.gameInitialize(gameMemory, mapName, editor);

    SDL_Event e;
    bool playing = true;

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
