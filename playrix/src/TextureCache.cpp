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

}  // namespace m3
