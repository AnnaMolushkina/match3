#pragma once

#include <SDL2/SDL.h>

#include "Config.h"
#include "Level.h"

namespace m3 {

class Board;
class TextureCache;

// Отрисовка: Renderer ничего не знает про правила игры, ему передают готовое поле.
class Renderer {
public:
    Renderer(SDL_Renderer* sdl, TextureCache& textures, const Level& level);

    void clear();
    void present();
    void drawBoard(const Board& board);

private:
    void fill(int x, int y, int w, int h, cfg::Rgb color, Uint8 alpha = 255);
    void drawBoardBackground();

    SDL_Renderer* sdl_;
    TextureCache* textures_;
    const Level*  level_;
};

}  // namespace m3
