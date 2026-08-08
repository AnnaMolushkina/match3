#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <emscripten.h>

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* tileTexture;

void mainLoop() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        // пока пусто, обработаем ввод позже
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255); // фон
    SDL_RenderClear(renderer);

    SDL_Rect dst = { 100, 100, 64, 64 }; // x, y, ширина, высота
    SDL_RenderCopy(renderer, tileTexture, nullptr, &dst);

    SDL_RenderPresent(renderer);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    window = SDL_CreateWindow("Match3", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               600, 600, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    tileTexture = IMG_LoadTexture(renderer, "assets/booster_rainbow_ball_60.png");

    emscripten_set_main_loop(mainLoop, 0, 1);
    return 0;
}