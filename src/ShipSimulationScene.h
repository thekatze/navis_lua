#pragma once

#include <algorithm>
#include <chipmunk/chipmunk_types.h>
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
    cpVect relative_position;

    cpVect position() const { return cpBodyGetPosition(body); }
    cpVect velocity() const { return cpBodyGetVelocity(body); }
    cpVect direction() const { return cpBodyGetRotation(body); }

    f32 rotation() const { return cpBodyGetAngle(body) * 180.0 / std::numbers::pi_v<f64>; }
};

enum struct BlockType {
    Hub = 0,
    Hull = 1,
    Thruster = 2,
    Radar = 3,
    Gun = 4,
};

inline cpVect get_block_dimensions(BlockType type) {
    switch (type) {
    case BlockType::Hub:
    case BlockType::Hull:
    case BlockType::Radar:
    case BlockType::Gun:
        return cpVect{.x = 32, .y = 32};
    case BlockType::Thruster:
        return cpVect{.x = 16, .y = 32};
    }
}

struct Sprite {
    AssetHandle handle;
};

struct ShipId {
    EntityId id;
};

struct ShipBrain {};

struct ShipScript {
    std::string name;
    std::vector<RigidBody> parts;
    sol::protected_function update;
};

struct ShipSimulationScene : public IScene {
    World m_world;

    cpSpace *m_space;

    std::unordered_map<EntityId, ShipScript> m_ships;
    sol::state m_lua;

    void on_enter(EngineApi &api) override {
        m_lua.open_libraries(sol::lib::base);
        m_lua["BLOCK_HUB"] = BlockType::Hub;
        m_lua["BLOCK_HULL"] = BlockType::Hull;

        m_world = World{};

        m_space = cpSpaceNew();

        auto ship_hub_texture = api.assets.textures.load("./assets/gameplay/ship_block_hub.bmp");
        auto ship_hull_texture = api.assets.textures.load("./assets/gameplay/ship_block_hull.bmp");

        api.on_file_dropped = [&, this, ship_hub_texture, ship_hull_texture](const char *file_path,
                                                                             f32 cx, f32 cy) {
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

            sol::protected_function construct = table["construct"];
            if (!construct.valid()) {
                std::cerr << "No construct fn" << std::endl;
                return;
            }

            sol::protected_function update = table["update"];
            if (!update.valid()) {
                std::cerr << "No update fn" << std::endl;
                return;
            }

            cpVect center{.x = cx, .y = cy};

            EntityId ship_id = 0;
            ShipScript *ship = nullptr;

            m_lua.set_function("place", [&](BlockType type, int dx, int dy) {
                cpVect relative_pos{.x = dx * 32.0, .y = dy * 32.0};
                auto dimensions = get_block_dimensions(type);

                auto mass = 1.0f;
                auto moment = cpMomentForBox(mass, dimensions.x, dimensions.y);

                RigidBody block{.body = cpSpaceAddBody(m_space, cpBodyNew(mass, moment)),
                                .relative_position = relative_pos};
                cpSpaceAddShape(m_space, cpBoxShapeNew(block.body, dimensions.x, dimensions.y, 0));

                cpBodySetPosition(block.body, cpvadd(center, relative_pos));
                cpBodySetAngle(block.body, 0);

                if (ship) {
                    for (auto &part : ship->parts) {
                        auto diff = cpvsub(part.relative_position, block.relative_position);
                        auto distance_sq = cpvlengthsq(diff);
                        always_assert(distance_sq > 0.1, "cant place blocks on top of each other");

                        if (distance_sq > 32.5 * 32.5)
                            continue;

                        auto half_diff = cpvmult(diff, 0.5);
                        cpSpaceAddConstraint(m_space,
                                             cpDampedSpringNew(block.body, part.body, half_diff,
                                                               cpvneg(half_diff), 0, 1000, 10));
                    }
                }

                switch (type) {
                case BlockType::Hub: {
                    always_assert(ship_id == 0, "only one hub per ship allowed");
                    ship_id = m_world.spawn(block,
                                            Sprite{
                                                .handle = ship_hub_texture,
                                            },
                                            ShipId{.id = 0}, ShipBrain{});

                    // need to first spawn to be able to set the ship id
                    std::get<ShipId &>(*m_world.get<ShipId &>(ship_id)).id = ship_id;

                    m_ships[ship_id] = ShipScript{
                        .name = *name,
                        .parts = {},
                        .update = update,
                    };

                    ship = &m_ships[ship_id];
                    break;
                }
                case BlockType::Hull:
                case BlockType::Thruster:
                case BlockType::Radar:
                case BlockType::Gun: {
                    always_assert(ship_id != 0 && ship != nullptr, "hub must be placed first");
                    m_world.spawn(block, ShipId{.id = ship_id},
                                  Sprite{
                                      .handle = ship_hull_texture,
                                  });
                    break;
                }
                }

                ship->parts.push_back(block);
            });

            construct();
            m_lua.set_function("place", []() {});
        };

        api.on_file_dropped("/Users/thekatze/Development/me-when-lua/assets/scripting/script.lua",
                            64.0f, 64.0f);
    }

    void update(EngineApi &api) override {
        m_world.query<RigidBody &, ShipBrain &>([this](EntityId id, RigidBody &body, ShipBrain &_) {
            auto &ship = m_ships[id];
            m_lua.set_function("set_thruster", [&body](usize id, f32 percentage) {
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
