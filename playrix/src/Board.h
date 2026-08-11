#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace m3 {

constexpr int kEmpty = -1;

struct Cell {
    int r = 0;
    int c = 0;
};

inline bool operator==(Cell a, Cell b) { return a.r == b.r && a.c == b.c; }

// Одна фишка, съезжающая вниз по столбцу после схлопывания матча.
// fromRow отрицателен для фишек, рождённых выше верхнего края поля.
struct FallMove {
    int  col     = 0;
    int  fromRow = 0;
    int  toRow   = 0;
    bool spawned = false;
};

// Чистая логика поля: никакого SDL, никакой отрисовки, никаких анимаций.
// Все операции мгновенные — визуальный слой лишь проигрывает их результат.
class Board {
public:
    Board(int rows, int cols, int colors, uint32_t seed);

    int rows() const { return rows_; }
    int cols() const { return cols_; }
    int colors() const { return colors_; }
    int cellCount() const { return rows_ * cols_; }

    int  index(int r, int c) const { return r * cols_ + c; }
    bool inside(int r, int c) const { return r >= 0 && r < rows_ && c >= 0 && c < cols_; }
    int  at(int r, int c) const { return cells_[index(r, c)]; }
    void set(int r, int c, int value) { cells_[index(r, c)] = value; }

    // Случайная раскладка без готовых матчей и гарантированно с ходом.
    void reset();

    void swapCells(Cell a, Cell b);

    // Все клетки, входящие в серии длиной >= 3 по горизонтали или вертикали.
    // Формы L и T попадают в результат целиком: маска общая для обоих проходов.
    std::vector<Cell> findMatches() const;
    bool              hasAnyMatch() const;

    void clear(const std::vector<Cell>& cells);

    // Уплотняет столбцы вниз и добивает пустоты новыми фишками сверху.
    std::vector<FallMove> applyGravity();

    bool hasValidMove() const;

    // Перетасовывает уже лежащие на поле фишки. Возвращает перестановку:
    // result[newIndex] == oldIndex, чтобы визуальный слой знал, что куда летит.
    std::vector<int> shuffle();

private:
    int  pickColor();
    bool matchesRunFrom(int r, int c, int dr, int dc) const;

    int              rows_;
    int              cols_;
    int              colors_;
    std::vector<int> cells_; // внутри лежит номер цвета фишки или kEmpty, если клетка пуста
    std::mt19937     rng_;
};

}  // namespace m3
