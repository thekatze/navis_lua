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

const f64 RAD2DEG = 180.0 / std::numbers::pi_v<f64>;
const f64 DEG2RAD = std::numbers::pi_v<f64> / 180.0;

struct RigidBody {
    cpBody *body;
    cpVect relative_position;

    cpVect position() const { return cpBodyGetPosition(body); }
    cpVect velocity() const { return cpBodyGetVelocity(body); }
    cpVect direction() const { return cpBodyGetRotation(body); }

    f32 rotation() const { return cpBodyGetAngle(body); }
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
struct ShipThruster {
    f64 max_thrust;
};

struct ShipRadar {
    AssetHandle dish_handle;
    f32 rotation;
    f32 rotation_speed;
    bool rotated = false;
};

struct Lifetime {
    f32 until;
};

struct ShipGun {
    AssetHandle gun_handle;
    f32 rotation;
    f32 rotation_speed;
    f32 cooldown;
    f32 last_shot;
    bool rotated = false;
};

struct ShipScript {
    std::string name;
    std::vector<RigidBody> parts;
    sol::protected_function update;
};

struct Projectile {};

struct ShipSimulationScene : public IScene {
    World m_world;

    cpSpace *m_space;

    AssetHandle m_gun_shot_texture;

    std::unordered_map<EntityId, ShipScript> m_ships;
    sol::state m_lua;

    void on_enter(EngineApi &api) override {
        m_lua.open_libraries(sol::lib::base, sol::lib::math);
        m_lua["BLOCK_HUB"] = BlockType::Hub;
        m_lua["BLOCK_HULL"] = BlockType::Hull;
        m_lua["BLOCK_THRUSTER"] = BlockType::Thruster;
        m_lua["BLOCK_RADAR"] = BlockType::Radar;
        m_lua["BLOCK_GUN"] = BlockType::Gun;

        m_world = World{};
        m_space = cpSpaceNew();

        m_gun_shot_texture = api.assets.textures.load("./assets/gameplay/gun_shot.bmp");

        auto ship_hub_texture = api.assets.textures.load("./assets/gameplay/ship_block_hub.bmp");
        auto ship_hull_texture = api.assets.textures.load("./assets/gameplay/ship_block_hull.bmp");
        auto ship_thruster_texture =
            api.assets.textures.load("./assets/gameplay/ship_block_thruster.bmp");
        auto ship_radar_base_texture =
            api.assets.textures.load("./assets/gameplay/ship_block_radar_base.bmp");
        auto ship_radar_dish_texture =
            api.assets.textures.load("./assets/gameplay/ship_block_radar_dish.bmp");
        auto ship_gun_base_texture =
            api.assets.textures.load("./assets/gameplay/ship_block_gun_base.bmp");
        auto ship_gun_gun_texture =
            api.assets.textures.load("./assets/gameplay/ship_block_gun_gun.bmp");

        api.on_file_dropped = [&, this, ship_hub_texture, ship_hull_texture,
                               ship_radar_base_texture, ship_radar_dish_texture,
                               ship_thruster_texture, ship_gun_base_texture,
                               ship_gun_gun_texture](const char *file_path, f32 cx, f32 cy) {
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
                EntityId block_id = 0;

                auto dimensions = get_block_dimensions(type);
                cpVect relative_pos{.x = dx * 32.0 + (32.0 - dimensions.x) / 2.0,
                                    .y = dy * 32.0 + (32.0 - dimensions.y) / 2.0};

                auto mass = 1.0f;
                auto moment = cpMomentForBox(mass, dimensions.x, dimensions.y);

                RigidBody block{.body = cpSpaceAddBody(m_space, cpBodyNew(mass, moment)),
                                .relative_position = relative_pos};
                auto shape = cpSpaceAddShape(
                    m_space, cpBoxShapeNew(block.body, dimensions.x, dimensions.y, 0));

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
                                                               cpvneg(half_diff), 0, 3000, 10));
                        cpSpaceAddConstraint(
                            m_space, cpDampedRotarySpringNew(block.body, part.body, 0, 100000, 10));
                    }
                }

                switch (type) {
                case BlockType::Hub: {
                    always_assert(ship_id == 0, "only one hub per ship allowed");
                    block_id = ship_id = m_world.spawn(block,
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
                case BlockType::Thruster: {
                    always_assert(ship_id != 0 && ship != nullptr, "hub must be placed first");
                    block_id = m_world.spawn(block, ShipId{.id = ship_id},
                                             ShipThruster{.max_thrust = 50.0},
                                             Sprite{
                                                 .handle = ship_thruster_texture,
                                             });

                    break;
                }
                case BlockType::Radar: {
                    always_assert(ship_id != 0 && ship != nullptr, "hub must be placed first");
                    block_id = m_world.spawn(block, ShipId{.id = ship_id},
                                             ShipRadar{
                                                 .dish_handle = ship_radar_dish_texture,
                                                 .rotation = 0,
                                                 .rotation_speed = 8.0f * api.time.delta_time,
                                             },
                                             Sprite{
                                                 .handle = ship_radar_base_texture,
                                             });
                    break;
                }
                case BlockType::Gun: {
                    always_assert(ship_id != 0 && ship != nullptr, "hub must be placed first");
                    block_id = m_world.spawn(block, ShipId{.id = ship_id},
                                             ShipGun{
                                                 .gun_handle = ship_gun_gun_texture,
                                                 .rotation = 0,
                                                 .rotation_speed = 3.0f * api.time.delta_time,
                                                 .cooldown = 0.8f,
                                                 .last_shot = api.time.elapsed,
                                             },
                                             Sprite{
                                                 .handle = ship_gun_base_texture,
                                             });
                    break;
                }
                case BlockType::Hull: {
                    always_assert(ship_id != 0 && ship != nullptr, "hub must be placed first");
                    block_id = m_world.spawn(block, ShipId{.id = ship_id},
                                             Sprite{
                                                 .handle = ship_hull_texture,
                                             });
                    break;
                }
                }

                ship->parts.push_back(block);

                cpShapeSetFilter(shape, cpShapeFilterNew(ship_id, 0xFFFFFFFF, 0xFFFFFFFF));

                debug_assert(block_id != 0, "block_id must be set");
                return block_id;
            });

            construct();
            m_lua.set_function("place", []() {});
        };

        // api.on_file_dropped("/Users/thekatze/Development/me-when-lua/assets/scripting/script.lua",
        //                     64.0f, 64.0f);
    }

    void update(EngineApi &api) override {
        auto ships_count = m_world.query_count<ShipBrain>();

        std::vector<std::tuple<EntityId, cpVect, f32>> shots_to_spawn{};

        m_world.query<RigidBody &, ShipBrain &>([this, &api, ships_count,
                                                 &shots_to_spawn](EntityId ship_id, RigidBody &body,
                                                                  ShipBrain &_) {
            auto &ship = m_ships[ship_id];

            m_lua.set_function("ships_count", [ships_count]() { return ships_count; });
            m_lua.set_function("ship_angle", [body]() { return body.rotation(); });
            m_lua.set_function("ship_position", [body]() {
                auto pos = body.position();
                return std::tuple(pos.x, pos.y);
            });

            m_lua.set_function("ship_velocity", [body]() {
                auto vel = body.velocity();
                return std::tuple(vel.x, vel.y);
            });

            m_lua.set_function("radar_angle", [this, &ship_id](usize radar_id) {
                auto components = m_world.get<const ShipId &, const ShipRadar &>(radar_id);
                if (!components || std::get<const ShipId &>(*components).id != ship_id) {
                    std::cerr << "invalid radar id\n";
                    return 0.0f;
                }

                return std::get<const ShipRadar &>(*components).rotation;
            });

            m_lua.set_function("gun_angle", [this, &ship_id](usize gun_id) {
                auto components = m_world.get<const ShipId &, const ShipGun &>(gun_id);
                if (!components || std::get<const ShipId &>(*components).id != ship_id) {
                    std::cerr << "invalid gun id\n";
                    return 0.0f;
                }

                return std::get<const ShipGun &>(*components).rotation;
            });

            m_lua.set_function("radar_rotate", [this, &ship_id](usize radar_id, f32 percentage) {
                auto components = m_world.get<const ShipId &, ShipRadar &>(radar_id);
                if (!components || std::get<const ShipId &>(*components).id != ship_id) {
                    std::cerr << "invalid radar id\n";
                    return;
                }

                percentage = std::clamp(percentage, -100.0f, 100.0f);

                auto &radar = std::get<ShipRadar &>(*components);
                if (radar.rotated) {
                    std::cerr << "already issued a rotation call this frame\n";
                    return;
                }

                radar.rotation = radar.rotation + radar.rotation_speed * percentage / 100.0f;

                radar.rotated = true;
            });

            m_lua.set_function("gun_rotate", [this, &ship_id](usize gun_id, f32 percentage) {
                auto components = m_world.get<const ShipId &, ShipGun &>(gun_id);
                if (!components || std::get<const ShipId &>(*components).id != ship_id) {
                    std::cerr << "invalid gun id\n";
                    return;
                }

                percentage = std::clamp(percentage, -100.0f, 100.0f);

                auto &gun = std::get<ShipGun &>(*components);
                if (gun.rotated) {
                    std::cerr << "already issued a rotation call this frame\n";
                    return;
                }

                gun.rotation = gun.rotation + gun.rotation_speed * percentage / 100.0f;

                gun.rotated = true;
            });

            m_lua.set_function("radar_ping", [this, &body, &ship_id, &api](usize radar_id) {
                auto components =
                    m_world.get<const ShipId &, const RigidBody &, const ShipRadar &>(radar_id);
                if (!components || std::get<const ShipId &>(*components).id != ship_id) {
                    std::cerr << "invalid radar id\n";
                    return -1.0f;
                }

                auto &radar_body = std::get<const RigidBody &>(*components);
                auto &radar = std::get<const ShipRadar &>(*components);

                auto total_rotation = body.rotation() + radar.rotation;
                auto origin = radar_body.position();
                auto target = origin + cpvmult(cpvforangle(total_rotation), 2000);

                cpSegmentQueryInfo result;
                if (!cpSpaceSegmentQueryFirst(m_space, origin, target, 10.0f,
                                              cpShapeFilterNew(ship_id, 0xFFFFFFFF, 0xFFFFFFFF),
                                              &result)) {
                    return 0.0f;
                }

                return static_cast<f32>(cpvlength(cpvsub(result.point, origin)));
            });

            m_lua.set_function("gun_cooled_down", [this, &ship_id, &api](usize gun_id) {
                auto components = m_world.get<const ShipId &, const ShipGun &>(gun_id);
                if (!components || std::get<const ShipId &>(*components).id != ship_id) {
                    std::cerr << "invalid gun id\n";
                    return false;
                }

                auto &gun = std::get<const ShipGun &>(*components);
                return !(gun.last_shot + gun.cooldown >= api.time.elapsed);
            });

            m_lua.set_function("gun_shoot", [this, &ship_id, &api, &shots_to_spawn](usize gun_id) {
                auto components = m_world.get<const ShipId &, const RigidBody &, ShipGun &>(gun_id);
                if (!components || std::get<const ShipId &>(*components).id != ship_id) {
                    std::cerr << "invalid gun id\n";
                    return;
                }

                auto &gun_body = std::get<const RigidBody &>(*components);
                auto &gun = std::get<ShipGun &>(*components);

                if (gun.last_shot + gun.cooldown >= api.time.elapsed) {
                    std::cerr << "tried shooting while on cooldown\n";
                    return;
                }

                gun.last_shot = api.time.elapsed;

                auto total_rotation = gun_body.rotation() + gun.rotation;

                shots_to_spawn.emplace_back(ship_id, gun_body.position(), total_rotation);
            });

            m_lua.set_function("thruster_set", [this, &ship_id](usize thruster_id, f32 percentage) {
                auto components =
                    m_world.get<const ShipId &, const RigidBody &, const ShipThruster &>(
                        thruster_id);
                if (!components || std::get<const ShipId &>(*components).id != ship_id) {
                    std::cerr << "invalid thruster id\n";
                    return;
                }

                auto &rigid_body = std::get<const RigidBody &>(*components);
                auto &thruster = std::get<const ShipThruster &>(*components);

                auto thrust = std::clamp(percentage, 0.0f, 100.0f);
                auto dir = rigid_body.direction();
                cpBodyApplyForceAtLocalPoint(rigid_body.body,
                                             cpvmult(dir, thrust * thruster.max_thrust), cpvzero);
            });

            sol::protected_function_result update_result = ship.update();
            if (!update_result.valid()) {
                sol::error error = update_result;
                std::cerr << "[" << ship.name << "]: Error: " << error.what() << std::endl;
            }
        });

        auto gun_shot_texture = api.assets.textures.get(m_gun_shot_texture);
        f32 w, h;
        SDL_GetTextureSize(gun_shot_texture, &w, &h);

        for (auto &shot : shots_to_spawn) {
            auto ship_id = std::get<EntityId>(shot);
            auto origin = std::get<cpVect>(shot);
            auto angle = std::get<f32>(shot);

            auto mass = 100000.0f;
            auto moment = cpMomentForBox(mass, w, h);

            RigidBody block{.body = cpSpaceAddBody(m_space, cpBodyNew(mass, moment)),
                            .relative_position = cpvzero};
            auto shape = cpSpaceAddShape(m_space, cpBoxShapeNew(block.body, w, h, 0));
            cpShapeSetFilter(shape, cpShapeFilterNew(ship_id, 0xFFFFFFFF, 0xFFFFFFFF));

            cpBodySetPosition(block.body, origin);
            cpBodySetAngle(block.body, angle);

            const f32 BULLET_SPEED = 1000.0;
            cpBodySetVelocity(block.body, cpvforangle(angle) * BULLET_SPEED);

            m_world.spawn(block, Sprite{.handle = m_gun_shot_texture},
                          Lifetime{.until = api.time.elapsed + 1.0f});
        }

        m_world.query<ShipRadar &>([](EntityId &id, ShipRadar &radar) { radar.rotated = false; });
        m_world.query<ShipGun &>([](EntityId &id, ShipGun &gun) { gun.rotated = false; });

        std::vector<EntityId> to_remove{};

        m_world.query<const Lifetime &>([&api, &to_remove](EntityId id, const Lifetime &lifetime) {
            if (lifetime.until <= api.time.elapsed) {
                to_remove.emplace_back(id);
            }
        });

        for (auto id : to_remove) {
            auto rigid_body = m_world.get<RigidBody &>(id);
            if (rigid_body) {
                auto body = std::get<RigidBody &>(*rigid_body);
                cpSpaceRemoveBody(m_space, body.body);
            }

            m_world.remove<Lifetime>(id);
        }

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

                SDL_RenderTextureRotated(api.renderer, texture, NULL, &dest,
                                         body.rotation() * RAD2DEG, &center, SDL_FLIP_NONE);
            });

        m_world.query<const RigidBody &, const ShipRadar &>(
            [&api](EntityId id, const RigidBody &body, const ShipRadar &radar) {
                auto texture = api.assets.textures.get(radar.dish_handle);
                auto pos = body.position();

                f32 w, h;
                SDL_GetTextureSize(texture, &w, &h);

                auto total_rotation = body.rotation() + radar.rotation;

                SDL_FRect dest{
                    .x = static_cast<f32>(pos.x) - w / 2.0f,
                    .y = static_cast<f32>(pos.y) - h / 2.0f,
                    .w = w,
                    .h = h,
                };

                SDL_FPoint center{.x = w / 2, .y = h / 2};

                SDL_RenderTextureRotated(api.renderer, texture, NULL, &dest,
                                         total_rotation * RAD2DEG, &center, SDL_FLIP_NONE);
            });

        m_world.query<const RigidBody &, const ShipGun &>(
            [&api](EntityId id, const RigidBody &body, const ShipGun &gun) {
                auto texture = api.assets.textures.get(gun.gun_handle);
                auto pos = body.position();

                f32 w, h;
                SDL_GetTextureSize(texture, &w, &h);

                auto total_rotation = body.rotation() + gun.rotation;

                SDL_FRect dest{
                    .x = static_cast<f32>(pos.x) - w / 2.0f,
                    .y = static_cast<f32>(pos.y) - h / 2.0f,
                    .w = w,
                    .h = h,
                };

                SDL_FPoint center{.x = w / 2, .y = h / 2};

                SDL_RenderTextureRotated(api.renderer, texture, NULL, &dest,
                                         total_rotation * RAD2DEG, &center, SDL_FLIP_NONE);
            });
    }
};
