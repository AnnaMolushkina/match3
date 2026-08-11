#pragma once

#include <SDL2/SDL.h>

#include <string>
#include <vector>

namespace m3 {

// Владеет текстурами фишек и фоном.
// Грузит всю палитру cfg::kChipSprites один раз за запуск
class TextureCache {
public:
    ~TextureCache();

    bool load(SDL_Renderer* renderer, std::string& error);
    void destroy();

    // Аргумент — номер спрайта в палитре
    // перевод одного в другое делает Level::sprite().
    SDL_Texture* chip(int sprite) const;
    SDL_Texture* background() const { return background_; }

private:
    std::vector<SDL_Texture*> chips_;
    SDL_Texture*              background_ = nullptr;
};

}  // namespace m3
