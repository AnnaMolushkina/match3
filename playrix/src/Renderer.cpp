#include "Renderer.h"

#include "Board.h"
#include "TextureCache.h"

#include <cmath>

namespace m3 {

Renderer::Renderer(SDL_Renderer* sdl, TextureCache& textures, const Level& level)
    : sdl_(sdl), textures_(&textures), level_(&level) {} //берем адрес ссылок, чтобы сохранить их как указатели в полях класса

// Рисуем прямоугольник заданного цвета и прозрачности.
void Renderer::fill(int x, int y, int w, int h, cfg::Rgb color, Uint8 alpha) {
    SDL_SetRenderDrawColor(sdl_, color.r, color.g, color.b, alpha);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(sdl_, &rect);
}

// Рисуем только контур прямоугольника — четыре полосы по краям.
void Renderer::frame(int x, int y, int w, int h, int t, cfg::Rgb color) {
    fill(x, y, w, t, color);                      // сверху
    fill(x, y + h - t, w, t, color);              // снизу
    fill(x, y + t, t, h - 2 * t, color);          // слева
    fill(x + w - t, y + t, t, h - 2 * t, color);  // справа
}

// Очистить экран перед отрисовкой нового кадра.
void Renderer::clear() {
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdl_, cfg::kBgColor.r, cfg::kBgColor.g, cfg::kBgColor.b, 255);
    SDL_RenderClear(sdl_);
}

// Показать на экране то, что нарисовали в буфере SDL_Renderer
void Renderer::present() { SDL_RenderPresent(sdl_); }
// Фон под полем
void Renderer::drawBoardBackground() {
    const int x = level_->boardX();
    const int y = level_->boardY();

    if (SDL_Texture* bg = textures_->background()) {
        SDL_Rect dst{x, y, level_->boardW(), level_->boardH()};
        SDL_RenderCopy(sdl_, bg, nullptr, &dst);
        return;
    }

    fill(x - 8, y - 8, level_->boardW() + 16, level_->boardH() + 16, cfg::kBoardColor);
    for (int r = 0; r < level_->rows; ++r) {
        for (int c = 0; c < level_->cols; ++c) {
            const cfg::Rgb color = ((r + c) % 2 == 0) ? cfg::kCellColorEven : cfg::kCellColorOdd;
            fill(x + c * level_->tile, y + r * level_->tile, level_->tile, level_->tile, color);
        }
    }
}

// Рисует всё поле с фишками
void Renderer::drawBoard(const Board& board, const std::vector<ChipView>& views,
                        const Cell* selected) {
    drawBoardBackground();

    // Подсветка выбранной клетки рисуется до фишек: полупрозрачная заливка
    // и рамка остаются под спрайтом и не перечёркивают его.
    if (selected) {
        const int x = level_->boardX() + selected->c * level_->tile;
        const int y = level_->boardY() + selected->r * level_->tile;
        fill(x, y, level_->tile, level_->tile, cfg::kSelectColor, 55);
        frame(x, y, level_->tile, level_->tile, cfg::kSelectFrame, cfg::kSelectColor);
    }

    // Новые фишки начинают падение выше поля, поэтому обрезаем всё, что
    // выходит за его границы: иначе они рисовались бы поверх отступа и панели.
    const SDL_Rect clip{level_->boardX(), level_->boardY(), level_->boardW(), level_->boardH()};
    SDL_RenderSetClipRect(sdl_, &clip);

    for (int r = 0; r < board.rows(); ++r) {
        for (int c = 0; c < board.cols(); ++c) {
            const int color = board.at(r, c);
            if (color == kEmpty) continue;

            // Board нумерует цвета подряд от нуля, а текстуры лежат по
            // номерам спрайтов из палитры — перевод делает уровень.
            SDL_Texture* texture = textures_->chip(level_->sprite(color));
            if (!texture) continue;

            const ChipView& view  = views[board.index(r, c)];
            const long      alpha = std::lround(view.alpha * 255.0f);
            if (alpha <= 0) continue;  // фишка уже растворилась
            SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha));

            // dst — прямоугольник на экране, куда SDL_RenderCopy нарисует текстуру
            SDL_Rect dst{level_->boardX() + c * level_->tile,
                         level_->boardY() + r * level_->tile +
                             static_cast<int>(std::lround(view.offsetY)),
                         level_->tile, level_->tile};
            SDL_RenderCopy(sdl_, texture, nullptr, &dst); // SDL_RenderCopy рисует текстуру в dst, растягивая её на dst.w x dst.h пикселей
        }
    }

    SDL_RenderSetClipRect(sdl_, nullptr);
}

}  // namespace m3
