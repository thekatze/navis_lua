#pragma once

#include <algorithm>
#include <chipmunk/cpVect.h>
#include <iostream>
#include <unordered_map>

#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>

#include <chipmunk/chipmunk.h>
#include <sol/sol.hpp>

#include "engine/EngineApi.h"
#include "engine/IScene.h"

#include "ecs.h"

struct RigidBody {
    cpBody *body;

    cpVect position() const { return cpBodyGetPosition(body); }
    cpVect velocity() const { return cpBodyGetVelocity(body); }
    cpVect direction() const { return cpBodyGetRotation(body); }

    f32 rotation() const { return cpBodyGetAngle(body); }
};

struct ShipMarker {};

struct ShipScript {
    std::string name;
    sol::protected_function update;
};

struct ShipSimulationScene : public IScene {
    World m_world;

    cpSpace *m_space;

    std::unordered_map<EntityId, ShipScript> m_ships;
    sol::state m_lua;

    void on_enter(EngineApi &api) override {
        m_lua.open_libraries(sol::lib::base);
        m_world = World{};

        m_space = cpSpaceNew();
        cpSpaceSetGravity(m_space, cpVect{.x = 0.0, .y = 100.0});

        api.on_file_dropped = [this, &api](const char *file_path) {
            auto script =
                m_lua.script_file(file_path, [](lua_State *, sol::protected_function_result pfr) {
                    std::cerr << "Invalid lua file dropped" << std::endl;
                    return pfr;
                });

            if (!script.valid()) {
                std::cerr << "Invalid lua file dropped" << std::endl;
                return;
            }

            sol::table table = script;
            auto name = table.get<sol::optional<std::string>>("name");

            if (!name) {
                std::cerr << "No name" << std::endl;
                return;
            }

            sol::protected_function update = table["update"];
            if (!update.valid()) {
                std::cerr << "No update fn" << std::endl;
                return;
            }

            auto mass = 1.0f;
            auto moment = cpMomentForCircle(mass, 0, 5, cpvzero);

            cpBody *body = cpSpaceAddBody(m_space, cpBodyNew(mass, moment));

            auto ship_id = m_world.spawn(
                RigidBody{
                    .body = body,
                },
                ShipMarker{});

            m_ships[ship_id] = ShipScript{
                .name = *name,
                .update = update,
            };
        };

        api.on_file_dropped("/Users/thekatze/Development/me-when-lua/assets/scripting/script.lua");
    }

    void update(EngineApi &api) override {

        m_world.query<RigidBody &, ShipMarker &>(
            [this](EntityId id, RigidBody &body, ShipMarker &_) {
                auto &ship = m_ships[id];
                m_lua.set_function("thrust", [&body](f32 percentage) {
                    auto thrust = std::clamp(percentage, 0.0f, 100.0f);
                    auto dir = body.direction();
                    cpBodyApplyForceAtLocalPoint(body.body, cpvmult(dir, thrust), cpvzero);
                });

                ship.update();
            });

        cpSpaceStep(m_space, api.time.delta_time);
    }

    void render(EngineApi &api) override {
        SDL_SetRenderDrawColor(api.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
        m_world.query<const RigidBody &>([&api](EntityId id, const RigidBody &body) {
            auto pos = body.position();
            SDL_FRect rect{
                .x = static_cast<f32>(pos.x),
                .y = static_cast<f32>(pos.y),
                .w = 10,
                .h = 10,
            };

            SDL_RenderFillRect(api.renderer, &rect);
        });
    }
};
