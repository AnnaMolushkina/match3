#pragma once

#include <SDL2/SDL.h>

#include <vector>

#include "ChipView.h"
#include "Config.h"
#include "Level.h"

namespace m3 {

// forward declaration
// Компилятору для ссылки/указателя достаточно знать, что тип существует 
// не нужно знать его размер или содержимое (можно не включать заголовочный файл)
class Board;
class TextureCache;
class Text;
struct Cell;

// Отрисовка: Renderer ничего не знает про правила игры, ему передают готовое поле.
class Renderer {
public:
    Renderer(SDL_Renderer* sdl, TextureCache& textures, Text& text, const Level& level);

    void clear();
    void present();

    // Панель слева от поля: фишка целевого цвета и сколько её ещё собрать.
    void drawGoal(int remaining);
    // views — смещение и прозрачность фишек, снимок анимации на этот кадр.
    // selected — клетка под подсветкой; nullptr, если игрок ничего не выбрал.
    void drawBoard(const Board& board, const std::vector<ChipView>& views,
                   const Cell* selected = nullptr);

    // Летящие спрайты бустеров (Game::flights()) поверх уже нарисованного
    // поля; progress (Game::flightProgress(), 0..1) — как далеко они долетели.
    void drawBoosterFlights(const std::vector<BoosterFlight>& flights, float progress);

private:
    void fill(int x, int y, int w, int h, cfg::Rgb color, Uint8 alpha = 255); // отрисовка прямоугольника заданного цвета и прозрачности
    void frame(int x, int y, int w, int h, int thickness, cfg::Rgb color); // отрисовка контура прямоугольника заданного цвета и толщины
    void drawBoardBackground();

    SDL_Renderer* sdl_;
    TextureCache* textures_;
    Text*         text_;
    const Level*  level_; // рендер не меняет настройки уровня, поэтому ссылка на const
};

}  // namespace m3
