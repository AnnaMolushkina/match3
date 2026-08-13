#pragma once

#include <SDL2/SDL.h>

#include <string>
#include <vector>

#include "Config.h"

namespace m3 {

// Владеет текстурами фишек, бустеров и фоном.
// Грузит всю палитру cfg::kChipSprites и cfg::kBoosterSprites один раз за запуск.
class TextureCache {
public:
    ~TextureCache();

    bool load(SDL_Renderer* renderer, std::string& error);
    void destroy();

    // Аргумент — номер спрайта в палитре
    // перевод одного в другое делает Level::sprite().
    SDL_Texture* chip(int sprite) const; 
    // Спрайт бустера по его типу; nullptr для BoosterType::None.
    SDL_Texture* booster(cfg::BoosterType type) const;

private:
    std::vector<SDL_Texture*> chips_; // текстуры фишек, индекс — номер спрайта в палитре cfg::kChipSprites
    std::vector<SDL_Texture*> boosters_;  // бустеры, индекс — номер спрайта в палитре cfg::kBoosterSprites
};

}  // namespace m3
