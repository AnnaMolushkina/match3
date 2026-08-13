#include "Renderer.h"

#include "Board.h"
#include "Text.h"
#include "TextureCache.h"

#include <cmath>
#include <string>

namespace m3 {

namespace {
// Горизонтальная и вертикальная ракета делят один спрайт (см. Config.h) —
// вертикальная получается поворотом того же изображения на 90° влево
// (против часовой стрелки; в SDL положительный угол — по часовой).
double boosterAngle(cfg::BoosterType type) {
    return (type == cfg::BoosterType::RocketVertical) ? -90.0 : 0.0;
}
}  // namespace

Renderer::Renderer(SDL_Renderer* sdl, TextureCache& textures, Text& text, const Level& level)
    : sdl_(sdl), textures_(&textures), text_(&text), level_(&level) {} //берем адрес ссылок, чтобы сохранить их как указатели в полях класса

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

// Очистить экран перед отрисовкой нового кадра. При kBgAlpha == 0 канвас
// остаётся прозрачным, и за игровым полем виден фон HTML-страницы.
void Renderer::clear() {
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdl_, cfg::kBgColor.r, cfg::kBgColor.g, cfg::kBgColor.b, cfg::kBgAlpha);
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

            // Клетка с бустером рисуется его спрайтом вместо обычной фишки —
            // сама фишка при этом просто «одета» в бустер, цвет под ним не теряется.
            const cfg::BoosterType booster = board.boosterAt(r, c);
            SDL_Texture*           texture;
            if (booster != cfg::BoosterType::None) {
                texture = textures_->booster(booster);
            } else {
                // Board нумерует цвета подряд от нуля, а текстуры лежат по
                // номерам спрайтов из палитры — перевод делает уровень.
                texture = textures_->chip(level_->sprite(color));
            }
            if (!texture) continue;

            const ChipView& view  = views[board.index(r, c)];
            const long      alpha = std::lround(view.alpha * 255.0f);
            if (alpha <= 0) continue;  // фишка уже растворилась
            SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha));

            // dst — прямоугольник на экране, куда рисуем текстуру, растягивая
            // её на dst.w x dst.h пикселей
            SDL_Rect dst{level_->boardX() + c * level_->tile +
                             static_cast<int>(std::lround(view.offsetX)),
                         level_->boardY() + r * level_->tile +
                             static_cast<int>(std::lround(view.offsetY)),
                         level_->tile, level_->tile};
            // RenderCopyEx с углом 0 равносилен обычному RenderCopy — используем
            // его всегда, чтобы вертикальная ракета получала поворот без
            // отдельного спрайта.
            SDL_RenderCopyEx(sdl_, texture, nullptr, &dst, boosterAngle(booster), nullptr,
                              SDL_FLIP_NONE);
        }
    }

    SDL_RenderSetClipRect(sdl_, nullptr);
}

// Цель уровня: фишка нужного цвета по центру полосы слева от поля, а поверх
// неё в правом нижнем углу — сколько таких фишек ещё осталось собрать.
void Renderer::drawGoal(int remaining) {
    const int size = cfg::kGoalIconSize;
    const int x    = (level_->boardX() - size) / 2;   // центр полосы слева от поля
    const int y    = (level_->windowH() - size) / 2;  // и по высоте окна

    if (SDL_Texture* icon = textures_->chip(level_->sprite(level_->goalColor))) {
        // Прозрачность на текстуре общая, а фишка этого же цвета могла
        // растворяться на поле — возвращаем её в полную видимость.
        SDL_SetTextureAlphaMod(icon, 255);
        SDL_Rect dst{x, y, size, size};
        SDL_RenderCopy(sdl_, icon, nullptr, &dst);
    }

    const std::string label = std::to_string(remaining);
    int               w     = 0;
    int               h     = 0;
    text_->measure(label, w, h);

    // Правый нижний угол картинки: правый край строки совпадает с правым
    // краем фишки, нижний — с нижним.
    const int tx = x + size - w;
    const int ty = y + size - h;
    text_->draw(label, tx + 2, ty + 2, cfg::kTextShadow);  // тень — чтобы число читалось на любом цвете
    text_->draw(label, tx, ty, cfg::kTextColor);
}

}  // namespace m3
