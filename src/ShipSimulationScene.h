#pragma once

#include <iostream>

#include <sol/sol.hpp>
#include <unordered_map>

#include "engine/EngineApi.h"
#include "engine/IScene.h"

#include "ecs.h"

struct ShipMarker {};

struct ShipScript {
    std::string name;
    sol::protected_function update;
};

struct ShipSimulationScene : public IScene {
    World m_world;

    std::unordered_map<EntityId, ShipScript> m_ships;
    sol::state m_lua;

    void on_enter(EngineApi &api) override {
        m_lua.open_libraries(sol::lib::base);
        m_world = World{};

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

            auto ship_id = m_world.spawn(ShipMarker{});
            m_ships[ship_id] = ShipScript{
                .name = *name,
                .update = update,
            };
        };

        api.on_file_dropped("/Users/thekatze/Development/me-when-lua/assets/scripting/script.lua");
    }

    void update(EngineApi &api) override {
        for (auto &[_, ship] : m_ships) {
            std::cout << "Updating " << ship.name << '\n';
            ship.update();
        }
    }
    void render(EngineApi &api) override {}
};
