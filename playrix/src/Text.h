#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <string>
#include <unordered_map>

#include "Config.h"

namespace m3 {

// Обёртка над SDL2_ttf. Шрифт открывается один раз, а готовые текстуры строк
// складываются в кэш: число на панели рисуется каждый кадр, и пересобирать его
// текстуру шестьдесят раз в секунду незачем — меняется оно куда реже.
class Text {
public:
    ~Text();

    bool load(SDL_Renderer* renderer, const char* path, int size, std::string& error);
    void destroy();

    // Размер строки в пикселях — нужен, чтобы прижать её к краю или центру.
    void measure(const std::string& text, int& w, int& h) const;

    // x, y — левый верхний угол строки.
    void draw(const std::string& text, int x, int y, cfg::Rgb color);

private:
    SDL_Texture* texture(const std::string& text, cfg::Rgb color);

    SDL_Renderer*                                 sdl_  = nullptr;
    TTF_Font*                                     font_ = nullptr;
    std::unordered_map<std::string, SDL_Texture*> cache_;
};

}  // namespace m3
