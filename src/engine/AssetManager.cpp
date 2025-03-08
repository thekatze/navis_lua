#include "./AssetManager.h"

#include <SDL3_image/SDL_image.h>

#include "EngineApi.h"
#include "assert.h"

AssetManager::AssetManager(const EngineApi &api)
    : textures([&api](const char *path) {
          SDL_Texture *texture = IMG_LoadTexture(api.renderer, path);
          always_assert(texture != nullptr, "Texture load failed: " << path);
          return texture;
      }) {
}
