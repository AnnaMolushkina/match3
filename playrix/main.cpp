#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <cstdint>
#include <string>

#include "src/Board.h"
#include "src/Config.h"
#include "src/Level.h"
#include "src/Renderer.h"
#include "src/TextureCache.h"

namespace {

struct App {
    SDL_Window*      window   = nullptr;
    SDL_Renderer*    sdl      = nullptr;
    m3::Level        level;
    m3::TextureCache textures;
    m3::Board*       board    = nullptr;
    m3::Renderer*    renderer = nullptr;
    bool             running  = true;
};

App g_app;

// Проверка требования «стартовая расстановка без автоматчей»: после
// генерации на поле не должно быть ни одной серии из трёх.
void reportMatches(const m3::Board& board) {
    const size_t count = board.findMatches().size();
    if (count == 0) {
        SDL_Log("поле сгенерировано: автоматчей нет");
    } else {
        SDL_Log("ОШИБКА: на стартовом поле %zu фишек в матчах", count);
    }
}

void frame() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_app.running = false;
        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
            // Перегенерация поля: удобно прокликать десяток раскладок подряд
            // и убедиться, что автоматчей не бывает.
            g_app.board->reset();
            reportMatches(*g_app.board);
        }
    }

    g_app.renderer->clear();
    g_app.renderer->drawBoard(*g_app.board);
    g_app.renderer->present();
}

bool init() {
    std::string error;
    if (!m3::loadLevel(cfg::kLevelFile, g_app.level, error)) {
        SDL_Log("%s: %s", cfg::kLevelFile, error.c_str());
        return false;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return false;
    }
    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        SDL_Log("IMG_Init: %s", IMG_GetError());
        return false;
    }

    // Размер окна задаётся уровнем: правка rows/cols/tile в level.cfg
    // сразу меняет и поле, и окно.
    g_app.window = SDL_CreateWindow("Match3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    g_app.level.windowW(), g_app.level.windowH(), SDL_WINDOW_SHOWN);
    if (!g_app.window) {
        SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
        return false;
    }

    g_app.sdl = SDL_CreateRenderer(g_app.window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_app.sdl) {
        SDL_Log("SDL_CreateRenderer: %s", SDL_GetError());
        return false;
    }

    if (!g_app.textures.load(g_app.sdl, g_app.level, error)) {
        SDL_Log("%s", error.c_str());
        return false;
    }

    const uint32_t seed = static_cast<uint32_t>(SDL_GetPerformanceCounter());
    g_app.board = new m3::Board(g_app.level.rows, g_app.level.cols, g_app.level.colors, seed);
    g_app.board->reset();
    reportMatches(*g_app.board);

    g_app.renderer = new m3::Renderer(g_app.sdl, g_app.textures, g_app.level);
    return true;
}

}  // namespace

int main() {
    if (!init()) return 1;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(frame, 0, 1);
#else
    while (g_app.running) {
        frame();
        SDL_Delay(16);
    }
#endif
    return 0;
}
