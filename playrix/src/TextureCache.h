#pragma once

#include <SDL2/SDL.h>

#include <string>
#include <vector>

namespace m3 {

struct Level;

// Владеет текстурами фишек и фоном.
class TextureCache {
public:
    ~TextureCache();

    bool load(SDL_Renderer* renderer, const Level& level, std::string& error);
    void destroy();

    SDL_Texture* chip(int color) const;
    SDL_Texture* background() const { return background_; }

private:
    std::vector<SDL_Texture*> chips_;
    SDL_Texture*              background_ = nullptr;
};

}  // namespace m3
