#pragma once

#include <algorithm>
#include <chipmunk/cpVect.h>
#include <iostream>
#include <numbers>
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

    f32 rotation() const { return cpBodyGetAngle(body) * 180.0 / std::numbers::pi_v<f64>; }
};

struct Sprite {
    AssetHandle handle;
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

        auto ship_texture = api.assets.textures.load("./assets/gameplay/ship_block.bmp");
        api.on_file_dropped = [this, &api, ship_texture](const char *file_path, f32 x, f32 y) {
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
            auto moment = cpMomentForBox(mass, 32, 32);

            cpBody *hub = cpSpaceAddBody(m_space, cpBodyNew(mass, moment));
            cpBodySetPosition(hub, cpVect{.x = x, .y = y});
            cpSpaceAddShape(m_space, cpBoxShapeNew(hub, 32, 32, 0));

            cpBody *attached_body = cpSpaceAddBody(m_space, cpBodyNew(mass, moment));
            cpBodySetPosition(attached_body, cpVect{.x = x - 32, .y = y});
            cpSpaceAddShape(m_space, cpBoxShapeNew(attached_body, 32, 32, 0));

            cpSpaceAddConstraint(m_space,
                                 cpDampedSpringNew(hub, attached_body, cpVect{.x = -16, .y = 0},
                                                   cpVect{.x = 16, .y = 0}, 0, 1000, 10));

            m_world.spawn(
                RigidBody{
                    .body = attached_body,
                },
                Sprite{
                    .handle = ship_texture,
                });

            auto ship_id = m_world.spawn(
                RigidBody{
                    .body = hub,
                },
                Sprite{
                    .handle = ship_texture,
                },
                ShipMarker{});

            m_ships[ship_id] = ShipScript{
                .name = *name,
                .update = update,
            };
        };

        api.on_file_dropped("/Users/thekatze/Development/me-when-lua/assets/scripting/script.lua",
                            64.0f, 64.0f);
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
        m_world.query<const RigidBody &, const Sprite &>(
            [&api](EntityId id, const RigidBody &body, const Sprite &sprite) {
                auto texture = api.assets.textures.get(sprite.handle);
                auto pos = body.position();
                f32 w, h;
                SDL_GetTextureSize(texture, &w, &h);

                SDL_FRect dest{
                    .x = static_cast<f32>(pos.x) - w / 2.0f,
                    .y = static_cast<f32>(pos.y) - h / 2.0f,
                    .w = w,
                    .h = h,
                };

                SDL_FPoint center{.x = w / 2, .y = h / 2};

                SDL_RenderTextureRotated(api.renderer, texture, NULL, &dest, body.rotation(),
                                         &center, SDL_FLIP_NONE);
            });
    }
};
