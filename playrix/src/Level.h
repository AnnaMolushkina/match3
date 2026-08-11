#pragma once

#include <string>
#include <vector>

namespace m3 {

// Параметры уровня, читаются из внешнего файла (assets/level.cfg).
// Менять размер поля, набор цветов и цель уровня можно без пересборки.
struct Level {
    int rows = 8;
    int cols = 8;
    int tile = 64;  // размер клетки в пикселях

    // Число цветов на уровне. Должно совпадать с длиной chips — проверяется
    // при загрузке, чтобы опечатка в списке не осталась незамеченной.
    int colors = 3;

    // Цвета уровня: номера спрайтов из палитры cfg::kChipSprites.
    // Board работает с плотными индексами внутри этого списка (0..colors-1),
    // а отрисовка переводит их обратно в номер спрайта через sprite().
    std::vector<int> chips;

    // Цель уровня: собрать goalAmount фишек цвета goalColor.
    // goalColor — индекс внутри chips, а не номер спрайта.
    int goalColor  = 0;
    int goalAmount = 20;

    int sprite(int color) const { return chips[color]; }

    // Пиксельная раскладка, вычисляется из rows/cols/tile.
    int windowW() const;
    int windowH() const;
    int boardX() const;
    int boardY() const;
    int boardW() const { return cols * tile; }
    int boardH() const { return rows * tile; }
};

// Читает файл формата `ключ = значение` (# — комментарий).
// В случае ошибки возвращает false и заполняет error понятным описанием.
bool loadLevel(const char* path, Level& out, std::string& error);

}  // namespace m3
