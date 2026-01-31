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

#define GAME_CODE_SRC_FILE_NAME "game-module"
#define GAME_CODE_USE_FILE_NAME "game-module-locked"

#define EXECUTABLE_FILE_NAME "skyline-engine.exe"

#define JOLT_LIB_SRC_FILE_NAME "Jolt"

#include <debug.h>
#include <game_platform.h>
#include <render_backend.h>
#include <main.h>

#if SKL_INTERNAL
DebugState globalDebugState_;
DebugState* globalDebugState = &globalDebugState_;
#endif


struct SDLGameCode
{
    SDL_SharedObject *sharedObjectHandle;

    SDL_Time fileLastWritten;
    SDL_Time fileNewLastWritten_;

    game_initialize_t *gameInitialize;
    game_load_t *gameLoad;
    game_update_and_render_t *gameUpdateAndRender;
};

struct AppInformation
{
    SDL_Window *window;
    SDLGameCode &gameCode;
    GameMemory &gameMemory;
    SDL_Event &e;
    const char* mapName;
    bool playing;
    u64 now;
    u64 last;
    b32 editor;

    AppInformation(SDL_Window *setWindow, SDLGameCode &gameCode, GameMemory &gameMemory, SDL_Event &setE, const char* mapName, bool setPlaying, u64 setNow, u64 setLast, b32 setEditor) :
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

file_global u32 reloadCount = 0;

local const char* SDLGetGameCodeSrcFilePath()
{
    const char* result;
#if defined(PLATFORM_WINDOWS)
    result = GAME_CODE_SRC_FILE_NAME ".dll";
#else
    result = "./lib" GAME_CODE_SRC_FILE_NAME ".so";
#endif
    return result;
}

local const char* SDLGetJoltLibSrcFilePath()
{
    const char* result;
#if defined(PLATFORM_WINDOWS)
    result = JOLT_LIB_SRC_FILE_NAME ".dll";
#elif defined(PLATFORM_APPLE)
    result = "./lib" JOLT_LIB_SRC_FILE_NAME ".dylib";
#else
    result = "./lib" JOLT_LIB_SRC_FILE_NAME ".so";
#endif
    return result;
}

local inline SDL_Time SDLGetFileLastWritten(const char *path)
{
    SDL_Time result;
    SDL_PathInfo pathInfo;
    if (SDL_GetPathInfo(path, &pathInfo))
    {
        result = pathInfo.modify_time;
    }
    else
    {
        LOG_ERROR("Unable to get path info of game code.");
    }
    return result;
}

// The reason for the tag in the name is that if we are running the
// editor and the game at the same time, they both each need their own
// DLL.
local SDLGameCode SDLLoadGameCode(SDL_Time newFileLastWritten, b32 editor)
{
    SDLGameCode result = {};
    std::string tag = "game";
    if (editor)
    {
        tag = "editor";
    }
    const char *gameCodeSrcFilePath = SDLGetGameCodeSrcFilePath();
    // NOTE(marvin): Could make a macro to generalize, but lazy and
    // unsure of impact on compile time.
    std::string gameCodeUseFilePath;
#if defined(PLATFORM_WINDOWS)
    gameCodeUseFilePath = std::format(GAME_CODE_USE_FILE_NAME "_{}_{}.dll", tag.c_str(), reloadCount);
#else
    gameCodeUseFilePath = std::format("./lib" GAME_CODE_USE_FILE_NAME "_{}_{}.so", tag.c_str(), reloadCount);
#endif

    // NOTE(marvin): Need to have a copy for the platform executable
    // to use so that when recompile, allowed to rewrite the source
    // without it being locked by the platform executable.
    if(!SDL_CopyFile(gameCodeSrcFilePath, gameCodeUseFilePath.c_str()))
    {
        LOG_ERROR("Unable to copy game module source to used.");
        LOG_ERROR(SDL_GetError());
    }
    result.sharedObjectHandle = SDL_LoadObject(gameCodeUseFilePath.c_str());
    if (!result.sharedObjectHandle)
    {
        LOG_ERROR("Game code loading failed.");
        LOG_ERROR(SDL_GetError());
    }

    result.gameInitialize = (game_initialize_t *)SDL_LoadFunction(result.sharedObjectHandle, "GameInitialize");
    result.gameLoad = (game_load_t *)SDL_LoadFunction(result.sharedObjectHandle, "GameLoad");
    result.gameUpdateAndRender = (game_update_and_render_t *)SDL_LoadFunction(result.sharedObjectHandle, "GameUpdateAndRender");
    if (result.gameInitialize && result.gameLoad && result.gameUpdateAndRender)
    {
        result.fileLastWritten = newFileLastWritten;
    }
    else
    {
        LOG_ERROR("Unable to load symbols from game shared object.");
        result.gameInitialize = 0;
        result.gameLoad = 0;
        result.gameUpdateAndRender = 0;

    }
    return result;
}

local SDLGameCode SDLLoadGameCode(b32 editor)
{
    const char *gameCodeSrcFilePath = SDLGetGameCodeSrcFilePath();
    return SDLLoadGameCode(SDLGetFileLastWritten(gameCodeSrcFilePath), editor);
}

local void SDLUnloadGameCode(SDLGameCode *gameCode)
{
    if(gameCode->sharedObjectHandle)
    {
        SDL_UnloadObject(gameCode->sharedObjectHandle);        
        gameCode->sharedObjectHandle = 0;
    }
    gameCode->gameInitialize = 0;
    gameCode->gameLoad = 0;
    gameCode->gameUpdateAndRender = 0;
    reloadCount++;
}

local b32 SDLGameCodeChanged(SDLGameCode *gameCode)
{
    b32 result;
    const char *gameCodeSrcFilePath = SDLGetGameCodeSrcFilePath();
    gameCode->fileNewLastWritten_ = SDLGetFileLastWritten(gameCodeSrcFilePath);

    if (gameCode->fileNewLastWritten_)
    {
        result = gameCode->fileNewLastWritten_ > gameCode->fileLastWritten;
    }

    return result;
}

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
}

void updateLoop(void* appInfo) {
    AppInformation* info = (AppInformation* )appInfo;
    info->last = info->now;
    info->now = SDL_GetPerformanceCounter();

    f32 frameTime = (f32)((info->now - info->last) / (f32)SDL_GetPerformanceFrequency());

    SDLGameCode &gameCode = info->gameCode;
    if (SDLGameCodeChanged(&gameCode))
    {
        SDLUnloadGameCode(&gameCode);
        info->gameCode = SDLLoadGameCode(gameCode.fileNewLastWritten_, info->editor);
        gameCode = info->gameCode;
        gameCode.gameLoad(info->gameMemory, info->editor, true);
    }

    while (SDL_PollEvent(&info->e))
    {
        // Cut off Imgui until we actually implement a base renderer for WGPU
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
                    ToggleLoopedLiveEditingState(&globalSDLState);
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

    // Cut off Imgui until we actually implement a base renderer for WGPU
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    
    if (io.WantCaptureMouse)
    {
        keysDown.erase("Mouse 1");
    }

    GameInput gameInput;
    gameInput.mouseDeltaX = mouseDeltaX;
    gameInput.mouseDeltaY = mouseDeltaY;
    gameInput.mouseX = mouseX;
    gameInput.mouseY = mouseY;
    gameInput.keysDown = keysDown;

    ProcessInputWithLooping(&globalSDLState, &gameInput);
    
    gameCode.gameUpdateAndRender(info->gameMemory, gameInput, frameTime);

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

    // NOTE(marvin): Platform to have a handle to Jolt to keep it alive as game gets hot reloaded.
    // TODO(marvin): Platform doesn't need a handle to Jolt if on final release. Do we wrap this in SKL_INTERNAL?
    const char* joltLibSrcFilePath = SDLGetJoltLibSrcFilePath();
    SDL_SharedObject* joltSharedObjectHandle = SDL_LoadObject(joltLibSrcFilePath);
    if (!joltSharedObjectHandle)
    {
        LOG_ERROR("Jolt loading failed.");
        LOG_ERROR(SDL_GetError());
    }

    SDLGameCode gameCode = SDLLoadGameCode(editor);
    GameMemory gameMemory = {};
#if SKL_INTERNAL
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
