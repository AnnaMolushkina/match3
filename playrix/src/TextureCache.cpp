#include "TextureCache.h"

#include "Level.h"

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

bool TextureCache::load(SDL_Renderer* renderer, const Level& level, std::string& error) {
    destroy();
    chips_.assign(level.colors, nullptr);

    for (int color = 0; color < level.colors; ++color) {
        const std::string& path    = level.chipSprites[color];
        SDL_Texture*       texture = IMG_LoadTexture(renderer, path.c_str());
        if (!texture) {
            error = "не удалось загрузить спрайт `" + path + "`: " + IMG_GetError();
            destroy();
            return false;
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        chips_[color] = texture;
    }

    if (!level.backgroundFile.empty()) {
        background_ = IMG_LoadTexture(renderer, level.backgroundFile.c_str());
        if (!background_) {
            error = "не удалось загрузить фон `" + level.backgroundFile + "`: " + IMG_GetError();
            destroy();
            return false;
        }
    }

    return true;
}

SDL_Texture* TextureCache::chip(int color) const {
    if (color < 0 || color >= static_cast<int>(chips_.size())) return nullptr;
    return chips_[color];
}

}  // namespace m3
