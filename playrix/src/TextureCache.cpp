#include "TextureCache.h"

#include "Config.h"

#include <SDL2/SDL_image.h>

namespace m3 {

TextureCache::~TextureCache() { destroy(); }

void TextureCache::destroy() {
    for (SDL_Texture* texture : chips_) {
        if (texture) SDL_DestroyTexture(texture);
    }
    chips_.clear();
    for (SDL_Texture* texture : boosters_) {
        if (texture) SDL_DestroyTexture(texture);
    }
    boosters_.clear();
    if (background_) {
        SDL_DestroyTexture(background_);
        background_ = nullptr;
    }
}

bool TextureCache::load(SDL_Renderer* renderer, std::string& error) {
    destroy();
    chips_.assign(cfg::kChipSpriteCount, nullptr);

    for (int sprite = 0; sprite < cfg::kChipSpriteCount; ++sprite) {
        const char*  path    = cfg::kChipSprites[sprite];
        SDL_Texture* texture = IMG_LoadTexture(renderer, path);
        if (!texture) {
            error = std::string("не удалось загрузить спрайт `") + path + "`: " + IMG_GetError();
            destroy();
            return false;
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        chips_[sprite] = texture;
    }

    // Индекс 0 (None) не грузим — бустерам без типа спрайт не нужен.
    boosters_.assign(cfg::kBoosterSpriteCount, nullptr);
    for (int i = 1; i < cfg::kBoosterSpriteCount; ++i) {
        const char*  path    = cfg::kBoosterSprites[i];
        SDL_Texture* texture = IMG_LoadTexture(renderer, path);
        if (!texture) {
            error = std::string("не удалось загрузить спрайт бустера `") + path +
                    "`: " + IMG_GetError();
            destroy();
            return false;
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        boosters_[i] = texture;
    }

    if (cfg::kBackgroundSprite[0] != '\0') {
        background_ = IMG_LoadTexture(renderer, cfg::kBackgroundSprite);
        if (!background_) {
            error = std::string("не удалось загрузить фон `") + cfg::kBackgroundSprite +
                    "`: " + IMG_GetError();
            destroy();
            return false;
        }
    }

    return true;
}

SDL_Texture* TextureCache::chip(int sprite) const {
    if (sprite < 0 || sprite >= static_cast<int>(chips_.size())) return nullptr;
    return chips_[sprite];
}

SDL_Texture* TextureCache::booster(cfg::BoosterType type) const {
    const int index = static_cast<int>(type);
    if (index <= 0 || index >= static_cast<int>(boosters_.size())) return nullptr;
    return boosters_[index];
}

}  // namespace m3
