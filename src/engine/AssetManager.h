#pragma once

#include "defines.h"

#include <functional>
#include <unordered_map>

#include <SDL3/SDL_render.h>

struct EngineApi;

using AssetHandle = usize;

template <class T> class AssetCache {
  public:
    explicit AssetCache(std::function<T(const char *)> load_fn) : load_fn(load_fn) {}
    AssetHandle load(const char *path) {
        const AssetHandle handle = std::hash<const char *>{}(path);

        if (cached_assets.contains(handle)) {
            return handle;
        }

        T asset = load_fn(path);
        cached_assets[handle] = asset;
        return handle;
    }

    T get(AssetHandle handle) const { return cached_assets.at(handle); }

    T load_immediate(const char *path) { return get(load(path)); }

  private:
    const std::function<T(const char *)> load_fn;
    std::unordered_map<AssetHandle, T> cached_assets;
};

class AssetManager {
  public:
    AssetManager(const EngineApi &api);
    AssetCache<SDL_Texture *> textures;
};
