#pragma once

#include <SDL2/SDL.h>

#include "Config.h"
#include "Level.h"

namespace m3 {

// forward declaration
// Компилятору для ссылки/указателя достаточно знать, что тип существует 
// не нужно знать его размер или содержимое (можно не включать заголовочный файл)
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
    const Level*  level_; // рендер не меняет настройки уровня, поэтому ссылка на const
};

}  // namespace m3
