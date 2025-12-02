#if defined(__unix__) || defined(__unix) || defined(unix) ||    \
    (defined(__APPLE__) && defined(__MACH__))
    #define PLATFORM_UNIX
#elif defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>
#include <random>

#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>
#include <SDL3/SDL_surface.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "renderer/render_backend.h"
#include "game_platform.h"

#if SKL_ENABLED_EDITOR
#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#endif

#include "asset_types.h"
#include "asset_utils.cpp"
#include "math/skl_math_utils.h"

#include "main.h"

#if EMSCRIPTEN
#include <emscripten/html5.h>
#endif

#define GAME_CODE_SRC_FILE_NAME "game-module"
#define GAME_CODE_USE_FILE_NAME "game-module-locked"

global_variable std::set<std::string> keysDown;
global_variable f32 mouseDeltaX = 0;
global_variable f32 mouseDeltaY = 0;

local const char *SDLGetGameCodeSrcFilePath()
{
    const char *result;
#ifdef PLATFORM_WINDOWS
    result = GAME_CODE_SRC_FILE_NAME ".dll";
#else
    result = "./lib" GAME_CODE_SRC_FILE_NAME ".so";
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

local SDLGameCode SDLLoadGameCode(SDL_Time newFileLastWritten)
{
    SDLGameCode result = {};
    const char *gameCodeSrcFilePath = SDLGetGameCodeSrcFilePath();
    // NOTE(marvin): Could make a macro to generalize, but lazy and
    // unsure of impact on compile time.
    const char *gameCodeUseFilePath;
#ifdef PLATFORM_WINDOWS
    gameCodeUseFilePath = GAME_CODE_USE_FILE_NAME ".dll";
#else
    gameCodeUseFilePath = "./lib" GAME_CODE_USE_FILE_NAME ".so";
#endif

    // NOTE(marvin): Need to have a copy for the platform executable
    // to use so that when recompile, allowed to rewrite the source
    // without it being locked by the platform executable.
    if(!SDL_CopyFile(gameCodeSrcFilePath, gameCodeUseFilePath))
    {
        LOG_ERROR("Unable to copy game module source to used.");
        LOG_ERROR(SDL_GetError());
    }
    result.sharedObjectHandle = SDL_LoadObject(gameCodeUseFilePath);
    if (!result.sharedObjectHandle)
    {
        LOG_ERROR("Game code loading failed.");
        LOG_ERROR(SDL_GetError());
    }

    result.gameInitialize = (game_initialize_t *)SDL_LoadFunction(result.sharedObjectHandle, "GameInitialize");
    result.gameUpdateAndRender = (game_update_and_render_t *)SDL_LoadFunction(result.sharedObjectHandle, "GameUpdateAndRender");
    if (result.gameInitialize && result.gameUpdateAndRender)
    {
        result.fileLastWritten = newFileLastWritten;
    }
    else
    {
        LOG_ERROR("Unable to load symbols from game shared object.");
        result.gameInitialize = 0;
        result.gameUpdateAndRender = 0;

    }
    return result;
}

local SDLGameCode SDLLoadGameCode()
{
    const char *gameCodeSrcFilePath = SDLGetGameCodeSrcFilePath();
    return SDLLoadGameCode(SDLGetFileLastWritten(gameCodeSrcFilePath));
}

local void SDLUnloadGameCode(SDLGameCode *gameCode)
{
    if(gameCode->sharedObjectHandle)
    {
        SDL_UnloadObject(gameCode->sharedObjectHandle);
        gameCode->sharedObjectHandle = 0;
    }
    gameCode->gameInitialize = 0;
    gameCode->gameUpdateAndRender = 0;
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

void updateLoop(void* appInfo) {
    AppInformation* info = (AppInformation* )appInfo;
    info->last = info->now;
    info->now = SDL_GetPerformanceCounter();

    f32 deltaTime = (f32)((info->now - info->last) / (f32)SDL_GetPerformanceFrequency());

    SDLGameCode gameCode = info->gameCode;
    if (SDLGameCodeChanged(&gameCode))
    {
        SDLUnloadGameCode(&gameCode);
        info->gameCode = SDLLoadGameCode(gameCode.fileNewLastWritten_);
        gameCode = info->gameCode;
    }

    while (SDL_PollEvent(&info->e))
    {
        // Cut off Imgui until we actually implement a base renderer for WGPU
        #if SKL_ENABLED_EDITOR
        ImGui_ImplSDL3_ProcessEvent(&info->e);
        #endif
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
                keysDown.insert(SDL_GetKeyName(info->e.key.key));
                break;
            case SDL_EVENT_KEY_UP:
                keysDown.erase(SDL_GetKeyName(info->e.key.key));
                break;
        }
    }

    SDL_GetRelativeMouseState(&mouseDeltaX, &mouseDeltaY);

    s32 windowWidth = WINDOW_WIDTH;
    s32 windowHeight = WINDOW_HEIGHT;
    SDL_GetWindowSize(info->window, &windowWidth, &windowHeight);

    // Cut off Imgui until we actually implement a base renderer for WGPU
    #if SKL_ENABLED_EDITOR
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    #endif

    GameInput gameInput;
    gameInput.mouseDeltaX = mouseDeltaX;
    gameInput.mouseDeltaY = mouseDeltaY;
    gameInput.keysDown = keysDown;
    gameCode.gameUpdateAndRender(info->scene, gameInput, deltaTime);

    mouseDeltaX = 0;
    mouseDeltaY = 0;

    f32 msPerFrame =  1000.0f * deltaTime;
    f32 fps = 1 / deltaTime;
    // printf("%.02f ms/frame (FPS: %.02f)\n", msPerFrame, fps);
    return;
}


#include <filesystem>

int main()
{
    std::cout << "Current path: " << std::filesystem::current_path() << std::endl;
    srand(static_cast<unsigned>(time(0)));

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

    #if SKL_ENABLED_EDITOR
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplSDL3_InitForOther(window);
    #endif

    SDL_SetWindowRelativeMouseMode(window, true);

    RenderInitInfo initDesc {
            .window = window,
            .startWidth = WINDOW_WIDTH,
            .startHeight = WINDOW_HEIGHT
    };
    InitRenderer(initDesc);

    SDLGameCode gameCode = SDLLoadGameCode();
    GameMemory gameMemory = {};
    PlatformAPI platformAPI = {};
    platformAPI.platformLoadMeshAsset = &LoadMeshAsset;
    platformAPI.platformLoadTextureAsset = &LoadTextureAsset;

    Scene scene;
    gameCode.gameInitialize(scene, gameMemory, platformAPI);

    SDL_Event e;
    bool playing = true;

    u64 now = SDL_GetPerformanceCounter();
    u64 last = 0;
    AppInformation app = AppInformation(window, gameCode, scene, e, playing, now, last);
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
