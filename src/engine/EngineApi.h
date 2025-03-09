#pragma once

#include "SceneStack.h"
#include "defines.h"
#include "engine/AssetManager.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

struct Time {
    f32 delta_time;
    f32 elapsed;
};

struct EngineApi {
    EngineApi(i32 window_width, i32 window_height, const char *title)
        : assets(*this), scenes(*this) {
        SDL_Init(SDL_INIT_VIDEO);
        SDL_CreateWindowAndRenderer(title, window_width, window_height, 0, &window, &renderer);

        time = {
            .delta_time = 1.0f / 120.0f,
            .elapsed = 0.0f,
        };
    }

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;

    AssetManager assets;
    SceneStack scenes;

    std::function<void(const char *file_path, f32 x, f32 y)> on_file_dropped;

    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    Time time;
};
