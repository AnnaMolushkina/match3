#pragma once

#include <string>
#include <vector>

namespace m3 {

// Параметры уровня, читаются из внешнего файла (assets/level.cfg).
// Менять размер поля, число цветов и цель уровня можно без пересборки.
struct Level {
    int rows   = 8;
    int cols   = 8;
    int colors = 5;
    int tile   = 64;  // размер клетки в пикселях

    // Спрайты фишек: ровно `colors` штук, индекс = цвет фишки.
    std::vector<std::string> chipSprites;

    // Цель уровня: собрать goalAmount фишек цвета goalColor.
    int goalColor  = 0;
    int goalAmount = 20;

    std::string fontFile;
    std::string backgroundFile;  // необязательный фон, пустая строка = нет

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
