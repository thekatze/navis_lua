#pragma once

#include "assert.h"
#include "defines.h"

#include <bitset>
#include <cstring>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

using EntityId = usize;

struct IArchetype {
    static const usize INITIAL_CAPACITY = 8;
    static const usize GROW_FACTOR = 2;

    virtual ~IArchetype() = default;

    virtual usize entity_count() = 0;
    virtual bool remove_entity(const EntityId &) = 0;
    virtual void *storage_of(const std::type_index &type) = 0;
    virtual void clear() = 0;
};

template <typename... Components> struct Archetype : public IArchetype {
    static constexpr usize ARCHETYPE_SIZE = sizeof(EntityId) + (sizeof(Components) + ...);

    usize m_entity_count;
    usize m_capacity;
    u8 *m_storage;
    std::unordered_map<std::type_index, usize> m_offsets;

    Archetype(usize capacity = IArchetype::INITIAL_CAPACITY)
        : m_entity_count(0),
          m_capacity(capacity),
          m_storage(reinterpret_cast<u8 *>(operator new(capacity * ARCHETYPE_SIZE))),
          m_offsets({{typeid(EntityId), 0}, {typeid(Components), offset_of<Components>()}...}) {}

    ~Archetype() { operator delete(m_storage); }

    usize entity_count() override { return m_entity_count; }

    template <typename Component> Component *storage_of() {
        return reinterpret_cast<Component *>(m_storage + offset_of<Component>() * m_capacity);
    }

    // specialization for EntityId, they are always first
    template <> EntityId *storage_of<EntityId>() { return reinterpret_cast<EntityId *>(m_storage); }

    void *storage_of(const std::type_index &type) override {
        return m_storage + m_offsets.at(type) * m_capacity;
    }

    void add_entity(EntityId id, Components... components) {
        ensure_capacity(m_entity_count + 1);

        storage_of<EntityId>()[m_entity_count] = id;
        ((storage_of<Components>()[m_entity_count] = components), ...);

        ++m_entity_count;
    }

    bool remove_entity(const EntityId &id) override {
        auto ids = storage_of<EntityId>();
        for (usize i = 0; i < m_entity_count; ++i) {
            if (id == ids[i]) {
                EntityId *to_remove = storage_of<EntityId>() + i;
                EntityId *last_item = storage_of<EntityId>() + m_entity_count - 1;

                *to_remove = std::move(*last_item);

                (
                    [&]() {
                        Components *to_remove = storage_of<Components>() + i;
                        Components *last_item = storage_of<Components>() + m_entity_count - 1;

                        *to_remove = std::move(*last_item);
                    }(),
                    ...);

                m_entity_count--;

                return true;
            }
        }

        return false;
    }

    void clear() override { m_entity_count = 0; }

  private:
    void ensure_capacity(usize required_capacity) {
        if (required_capacity <= m_capacity) {
            return;
        }

        usize new_capacity = m_capacity;
        do {
            new_capacity = m_capacity * IArchetype::GROW_FACTOR;
        } while (new_capacity < required_capacity);

        u8 *new_storage = reinterpret_cast<u8 *>(operator new(new_capacity * ARCHETYPE_SIZE));

        void *source = storage_of<EntityId>();
        void *destination = new_storage;

        std::memcpy(destination, source, m_entity_count * sizeof(EntityId));
        (
            [&]() {
                void *source = storage_of<Components>();
                void *destination = new_storage + offset_of<Components>() * new_capacity;

                std::memcpy(destination, source, m_entity_count * sizeof(Components));
            }(),
            ...);

        operator delete(m_storage);
        m_storage = new_storage;
        m_capacity = new_capacity;
    }

    template <typename Component> constexpr usize offset_of() const {
        static_assert(((std::is_same_v<Component, Components>) || ...),
                      "Component is not part of the Archetype components!");

        usize offset = sizeof(EntityId);
        bool component_found = false;

        ((std::is_same_v<Component, Components>
              ? component_found = true
              : !component_found && (offset += sizeof(Components))),
         ...);

        return offset;
    }
};

const usize COMPONENT_COUNT = 64;

struct World {
    using Signature = std::bitset<COMPONENT_COUNT>;

    usize m_component_count;
    EntityId m_next_entity_id;

    std::unordered_map<std::type_index, usize> m_component_indices;
    std::unordered_map<Signature, std::unique_ptr<IArchetype>> m_archetypes;
    u8 m_queries_in_progress;

    World()
        : m_component_count(0),
          m_next_entity_id(1),
          m_component_indices(),
          m_queries_in_progress(0) {}

    template <class Component> usize component_index() {
        auto it = m_component_indices.find(typeid(Component));

        if (it == m_component_indices.end()) {
            it = m_component_indices
                     .insert(
                         std::pair<std::type_index, usize>(typeid(Component), m_component_count++))
                     .first;
        }

        return it->second;
    }

    template <class... Components> inline Signature signature_of() {
        std::bitset<COMPONENT_COUNT> signature;

        (signature.set(component_index<Components>(), true), ...);

        return signature;
    }

    template <class... Components> EntityId spawn(Components... components) {
        always_assert(m_queries_in_progress == 0, "cant spawn during active query");
        always_assert(m_next_entity_id != 0, "uh oh, we ran out of entity ids");

        EntityId entity_id = m_next_entity_id++;
        const auto signature = signature_of<Components...>();

        auto it = m_archetypes.find(signature);

        if (it == m_archetypes.end()) {
            it =
                m_archetypes
                    .insert(std::make_pair(signature, std::make_unique<Archetype<Components...>>()))
                    .first;
        }

        auto &archetype = dynamic_cast<Archetype<Components...> &>(*it->second);
        archetype.add_entity(entity_id, components...);

        return entity_id;
    }

    template <class... Components> bool remove(EntityId id) {
        always_assert(m_queries_in_progress == 0, "cant remove during active query");
        const auto signature = signature_of<Components...>();
        for (auto &[archetype_signature, archetype] : m_archetypes) {
            if (signature != (archetype_signature & signature))
                continue; // signature does not fully overlap with this archetype

            if (archetype->remove_entity(id)) {
                return true;
            }
        }

        return false;
    }

    template <class... Components> void delete_matching() {
        always_assert(m_queries_in_progress == 0, "cant clear entities during active query");
        const auto signature = signature_of<Components...>();
        for (auto &[archetype_signature, archetype] : m_archetypes) {
            if (signature != (archetype_signature & signature))
                continue; // signature does not fully overlap with this archetype

            archetype->clear();
        }
    }

    template <class... Components> bool delete_exact() {
        always_assert(m_queries_in_progress == 0, "cant clear entities during active query");
        const auto signature = signature_of<Components...>();
        for (auto &[archetype_signature, archetype] : m_archetypes) {
            if (archetype_signature != signature)
                continue; // signature is not this archetype

            archetype->clear();
            return true;
        }

        return false;
    }

    template <class... Components> std::optional<std::tuple<Components...>> get(EntityId id) {
        const auto signature = signature_of<Components...>();

        // TODO: optimize this with some kind of cached index to memory address lookup
        // good enough for an initial implementation
        for (auto &[archetype_signature, archetype] : m_archetypes) {
            if (signature != (archetype_signature & signature))
                continue; // signature does not fully overlap with this archetype

            auto entity_ids = reinterpret_cast<EntityId *>(archetype->storage_of(typeid(EntityId)));

            auto storages = std::tuple(reinterpret_cast<std::remove_reference_t<Components> *>(
                archetype->storage_of(typeid(Components)))...);

            auto count = archetype->entity_count();

            for (usize i = 0; i < count; ++i) {
                if (entity_ids[i] == id) {
                    return std::optional(std::tuple<Components...>(
                        std::get<std::remove_reference_t<Components> *>(storages)[i]...));
                }
            }
        }

        return std::nullopt;
    }

    template <class... Components, class Fn> void query(Fn fn) {
        const auto signature = signature_of<Components...>();

        m_queries_in_progress++;
        for (auto &[archetype_signature, archetype] : m_archetypes) {
            if (signature != (archetype_signature & signature))
                continue; // signature does not fully overlap with this archetype

            auto count = archetype->entity_count();
            auto entity_ids = reinterpret_cast<EntityId *>(archetype->storage_of(typeid(EntityId)));

            auto storages = std::tuple(reinterpret_cast<std::remove_reference_t<Components> *>(
                archetype->storage_of(typeid(Components)))...);

            for (usize i = 0; i < count; ++i) {
                fn(entity_ids[i], std::get<std::remove_reference_t<Components> *>(storages)[i]...);
            }
        }
        m_queries_in_progress--;
    }

    template <class... Components> usize query_count() {
        const auto signature = signature_of<Components...>();

        usize count = 0;

        m_queries_in_progress++;
        for (auto &[archetype_signature, archetype] : m_archetypes) {
            if (signature != (archetype_signature & signature))
                continue; // signature does not fully overlap with this archetype

            count += archetype->entity_count();
        }

        m_queries_in_progress--;

        return count;
    }

    template <class... Components>
    std::vector<std::tuple<EntityId &, Components...>> query_into_vec() {
        const auto signature = signature_of<Components...>();

        std::vector<std::tuple<EntityId &, Components...>> vec;

        m_queries_in_progress++;
        for (auto &[archetype_signature, archetype] : m_archetypes) {
            if (signature != (archetype_signature & signature))
                continue; // signature does not fully overlap with this archetype

            auto entity_ids = reinterpret_cast<EntityId *>(archetype->storage_of(typeid(EntityId)));

            auto storages = std::tuple(reinterpret_cast<std::remove_reference_t<Components> *>(
                archetype->storage_of(typeid(Components)))...);

            auto count = archetype->entity_count();
            vec.reserve(vec.size() + count);

            for (usize i = 0; i < count; ++i) {
                vec.emplace_back(entity_ids[i],
                                 std::get<std::remove_reference_t<Components> *>(storages)[i]...);
            }
        }
        m_queries_in_progress--;

        return vec;
    }
};
