#pragma once

struct SDLGameCode
{
    SDL_SharedObject *sharedObjectHandle;

    SDL_Time fileLastWritten;
    SDL_Time fileNewLastWritten_;
  
    game_initialize_t *gameInitialize;
    game_update_and_render_t *gameUpdateAndRender;
};

struct AppInformation
{
    SDL_Window *window;
    SDLGameCode &gameCode;
    Scene &scene;
    SDL_Event &e;
    bool playing;
    u64 now;
    u64 last;

AppInformation(SDL_Window *setWindow, SDLGameCode &gameCode, Scene &setScene, SDL_Event &setE, bool setPlaying, u64 setNow, u64 setLast) :
        window(setWindow),
        gameCode(gameCode),
        scene(setScene),
        e(setE),
        playing(setPlaying),
        now(setNow),
        last(setLast)
    { }
};

