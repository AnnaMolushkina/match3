#include "Renderer.h"

#include "Board.h"
#include "TextureCache.h"

namespace m3 {

Renderer::Renderer(SDL_Renderer* sdl, TextureCache& textures, const Level& level)
    : sdl_(sdl), textures_(&textures), level_(&level) {}

void Renderer::fill(int x, int y, int w, int h, cfg::Rgb color, Uint8 alpha) {
    SDL_SetRenderDrawColor(sdl_, color.r, color.g, color.b, alpha);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(sdl_, &rect);
}

void Renderer::clear() {
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdl_, cfg::kBgColor.r, cfg::kBgColor.g, cfg::kBgColor.b, 255);
    SDL_RenderClear(sdl_);
}

void Renderer::present() { SDL_RenderPresent(sdl_); }

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

void Renderer::drawBoard(const Board& board) {
    drawBoardBackground();

    for (int r = 0; r < board.rows(); ++r) {
        for (int c = 0; c < board.cols(); ++c) {
            const int color = board.at(r, c);
            if (color == kEmpty) continue;

            SDL_Texture* texture = textures_->chip(color);
            if (!texture) continue;

            SDL_Rect dst{level_->boardX() + c * level_->tile, level_->boardY() + r * level_->tile,
                         level_->tile, level_->tile};
            SDL_RenderCopy(sdl_, texture, nullptr, &dst);
        }
    }
}

}  // namespace m3
