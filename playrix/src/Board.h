#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "Config.h"

namespace m3 {

constexpr int kEmpty = -1;

// Цвет клетки, занятой бустером. Отличен от любого настоящего цвета
// (0..colors_-1) и от kEmpty, поэтому такая клетка никогда не совпадёт
// по цвету с соседями и не попадёт в обычный матч
constexpr int kBoosted = -2;

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

// Кусок замэтченных клеток одной формы + тип бустера, который они дают.
struct MatchGroup {
    std::vector<Cell> cells;
    cfg::BoosterType  booster;
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
    int  at(int r, int c) const { return cells_[index(r, c)]; } //возвращает цвет фишки в клетке или kEmpty
    void set(int r, int c, int value) { cells_[index(r, c)] = value; }

    // Тип бустера, лежащего в клетке (None — обычная фишка без бустера).
    cfg::BoosterType boosterAt(int r, int c) const { return boosters_[index(r, c)]; }

    // Помечает клетку бустером и стирает её цвет (kBoosted), чтобы дальше
    // клетка никогда не подхватилась обычным цветовым матчем.
    void setBooster(int r, int c, cfg::BoosterType type) {
        boosters_[index(r, c)] = type;
        if (type != cfg::BoosterType::None) cells_[index(r, c)] = kBoosted;
    }

    // Случайная раскладка без готовых матчей и гарантированно с ходом.
    void reset();

    void swapCells(Cell a, Cell b);

    std::vector<Cell> findMatches() const;
    bool              hasAnyMatch() const;

    void clear(const std::vector<Cell>& cells);

    // Уплотняет столбцы вниз и добивает пустоты новыми фишками сверху.
    std::vector<FallMove> applyGravity();

    bool hasValidMove() const;

    // Перетасовывает уже лежащие на поле фишки. Возвращает перестановку:
    // result[newIndex] == oldIndex, чтобы визуальный слой знал, что куда летит.
    std::vector<int> shuffle();

    // Найти 2x2 квадраты одного цвета, опционально с одной соседней фишкой того же цвета
    std::vector<Cell> findPlanes() const;

    // Собрать все матчи, разбить на связные группы и классифицировать каждую
    std::vector<MatchGroup> collectMatchGroups() const;

    // ---- случайность для эффектов бустеров (тот же генератор, что и раскладка) ----

    // Случайный цвет из палитры уровня — рэйнбоу без пары использует его,
    // чтобы выбрать, какой цвет снести с поля.
    int randomColor() { return pickColor(); }

    // Случайная клетка с фишкой данного цвета, кроме перечисленных в exclude;
    // false, если подходящей нет — самолётик так находит цель нужного для
    // победы цвета, не считая клеток, куда ему лететь нельзя (свои соседи).
    bool randomCellOfColor(int color, const std::vector<Cell>& exclude, Cell& out);

private:
    int  pickColor(); // случайный выбор цвета из диапазона [0, colors_ - 1]
    bool matchesRunFrom(int r, int c, int dr, int dc) const; // проверяет, есть ли серия из 3 или более одинаковых фишек, начинающаяся в (r, c) и продолжающаяся в направлении (dr, dc).

    // Есть ли цельный 2x2 квадрат одного цвета с левым верхним углом в (r, c).
    bool hasSquareAt(int r, int c) const;

    // Проверить, содержит ли группа клеток цельный 2x2 квадрат
    bool containsSquare(const std::vector<Cell>& cells) const;

    // Длина самой длинной подряд идущей серии клеток группы в одном
    // направлении (по строке при horizontal == true, иначе по столбцу).
    int longestRun(const std::vector<Cell>& cells, bool horizontal) const;

    // Классифицировать одну связную группу по форме → тип бустера
    cfg::BoosterType classifyMatch(const std::vector<Cell>& cells) const;

    // Разбить список клеток на связные компоненты по цвету
    std::vector<std::vector<Cell>> groupByColor(const std::vector<Cell>& cells) const;

    int              rows_;
    int              cols_;
    int              colors_;
    std::vector<int> cells_; // внутри лежит номер цвета фишки или kEmpty, если клетка пуста
  
    std::vector<cfg::BoosterType> boosters_; // содержит тип бустера в каждой клетке (None — обычная фишка без бустера)
    std::mt19937     rng_;

};


}  // namespace m3
