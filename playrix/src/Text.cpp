#include "Text.h"

namespace m3 {

Text::~Text() { destroy(); }

// загрузка шрифта и установка размера в пикселях
bool Text::load(SDL_Renderer* renderer, const char* path, int size, std::string& error) {
    destroy();
    sdl_  = renderer;
    font_ = TTF_OpenFont(path, size);
    if (!font_) {
        error = std::string("не удалось открыть шрифт `") + path + "`: " + TTF_GetError();
        return false;
    }
    return true;
}

void Text::destroy() {
    for (auto& entry : cache_) SDL_DestroyTexture(entry.second);
    cache_.clear();
    if (font_) {
        TTF_CloseFont(font_);
        font_ = nullptr;
    }
}

// измерение строки в пикселях: w и h — выходные параметры
void Text::measure(const std::string& text, int& w, int& h) const {
    w = 0;
    h = 0;
    if (font_) TTF_SizeUTF8(font_, text.c_str(), &w, &h);
}

// Текстура строки из кэша; при промахе строка рисуется шрифтом и запоминается.
SDL_Texture* Text::texture(const std::string& text, cfg::Rgb color) {
    if (!font_ || !sdl_) return nullptr;

    // Одна и та же строка может понадобиться в разных цветах (например, тень),
    // поэтому цвет входит в ключ.
    const std::string key = std::to_string(color.r) + ':' + std::to_string(color.g) + ':' +
                            std::to_string(color.b) + ':' + text;
    if (const auto it = cache_.find(key); it != cache_.end()) return it->second;

    const SDL_Color sdlColor{color.r, color.g, color.b, 255};
    SDL_Surface*    surface = TTF_RenderUTF8_Blended(font_, text.c_str(), sdlColor);
    if (!surface) return nullptr;

    SDL_Texture* result = SDL_CreateTextureFromSurface(sdl_, surface);
    SDL_FreeSurface(surface);
    if (!result) return nullptr;

    cache_.emplace(key, result);
    return result;
}

// отрисовка строки в пикселях: x, y — левый верхний угол
void Text::draw(const std::string& text, int x, int y, cfg::Rgb color) {
    SDL_Texture* tex = texture(text, color);
    if (!tex) return;

    int w = 0;
    int h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst{x, y, w, h};
    SDL_RenderCopy(sdl_, tex, nullptr, &dst);
}

}  // namespace m3
