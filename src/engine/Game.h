#pragma once

#include "EngineApi.h"

#include <SDL3/SDL_timer.h>
#include <ctime>

struct Game {
    i32 m_window_width, m_window_height;

    bool m_should_close = false;
    EngineApi m_api;

    u64 tick;
    u64 ns_per_tick;
    u64 start_time;

    Game(i32 window_width, i32 window_height, const char *title)
        : m_window_width(),
          m_window_height(),
          m_should_close(false),
          m_api(window_width, window_height, title) {}

    void initialize() {
        tick = 0;
        ns_per_tick = SDL_NS_PER_SECOND / 60;
        start_time = SDL_GetTicksNS();
    }

    void handle_events() {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_should_close = true;
            }

            if (event.type == SDL_EVENT_DROP_FILE) {
                // TODO: event
                if (m_api.on_file_dropped) {
                    m_api.on_file_dropped(event.drop.data, event.drop.x, event.drop.y);
                }
            }
        }
    }

    void update() { m_api.scenes.current()->update(m_api); }

    void render() {
        SDL_SetRenderDrawColor(m_api.renderer, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderClear(m_api.renderer);

        m_api.scenes.current()->render(m_api);

        SDL_RenderPresent(m_api.renderer);
    }

    void game_loop() {
        initialize();

        while (!m_should_close) {
            handle_events();

            // fixed update
            auto target_tick = (SDL_GetTicksNS() - start_time) / ns_per_tick;
            while (tick < target_tick) {
                update();
                tick += 1;
                m_api.time.elapsed += m_api.time.delta_time;
            }

            render();
        }
    }

    template <typename Scene, typename... Args> void run(Args... args) {
        m_api.scenes.push<Scene>(&args...);

        game_loop();
    }
};
