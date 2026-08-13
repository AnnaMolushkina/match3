#include "TextureCache.h"

#include "Config.h"

#include <SDL2/SDL_image.h>

#include <cstring>

namespace m3 {

TextureCache::~TextureCache() { destroy(); }

void TextureCache::destroy() {
    for (SDL_Texture* texture : chips_) {
        if (texture) SDL_DestroyTexture(texture);
    }
    chips_.clear();

    // Несколько типов бустеров могут делить одну текстуру (см. load) —
    // уничтожаем каждый уникальный указатель ровно один раз.
    for (size_t i = 0; i < boosters_.size(); ++i) {
        SDL_Texture* texture = boosters_[i];
        if (!texture) continue;
        bool seenBefore = false;
        for (size_t j = 0; j < i; ++j) {
            if (boosters_[j] == texture) {
                seenBefore = true;
                break;
            }
        }
        if (!seenBefore) SDL_DestroyTexture(texture);
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
    // Горизонтальная и вертикальная ракета делят один и тот же файл (вертикальная
    // получается поворотом при отрисовке) — если путь уже встречался, текстуру
    // не грузим повторно, а переиспользуем уже загруженную.
    boosters_.assign(cfg::kBoosterSpriteCount, nullptr);
    for (int i = 1; i < cfg::kBoosterSpriteCount; ++i) {
        const char* path = cfg::kBoosterSprites[i];

        SDL_Texture* reused = nullptr;
        for (int j = 1; j < i; ++j) {
            if (std::strcmp(cfg::kBoosterSprites[j], path) == 0) {
                reused = boosters_[j];
                break;
            }
        }
        if (reused) {
            boosters_[i] = reused;
            continue;
        }

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
