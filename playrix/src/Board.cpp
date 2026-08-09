#include "Board.h"

#include <algorithm>
#include <numeric>

namespace m3 {

Board::Board(int rows, int cols, int colors, uint32_t seed)
    : rows_(rows), cols_(cols), colors_(colors), cells_(rows * cols, kEmpty), rng_(seed) {}

int Board::pickColor() {
    std::uniform_int_distribution<int> dist(0, colors_ - 1);
    return dist(rng_);
}

void Board::reset() {
    // Раскладываем по клеткам слева направо, сверху вниз, исключая цвет,
    // который дал бы тройку с двумя уже лежащими соседями слева или сверху.
    // Так автоматчей не бывает по построению — без цикла «сгенерировали и
    // проверили». Кандидатов всегда хотя бы colors_ - 2, то есть минимум один.
    std::vector<int> candidates;
    candidates.reserve(colors_);

    do {
        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                candidates.clear();
                for (int color = 0; color < colors_; ++color) {
                    if (c >= 2 && at(r, c - 1) == color && at(r, c - 2) == color) continue;
                    if (r >= 2 && at(r - 1, c) == color && at(r - 2, c) == color) continue;
                    candidates.push_back(color);
                }
                std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
                set(r, c, candidates[dist(rng_)]);
            }
        }
        // Поле без матчей может оказаться и без ходов — тогда пересобираем.
    } while (!hasValidMove());
}

void Board::swapCells(Cell a, Cell b) {
    std::swap(cells_[index(a.r, a.c)], cells_[index(b.r, b.c)]);
}

bool Board::matchesRunFrom(int r, int c, int dr, int dc) const {
    const int color = at(r, c);
    if (color == kEmpty) return false;
    return inside(r + 2 * dr, c + 2 * dc) && at(r + dr, c + dc) == color &&
           at(r + 2 * dr, c + 2 * dc) == color;
}

bool Board::hasAnyMatch() const {
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (matchesRunFrom(r, c, 0, 1) || matchesRunFrom(r, c, 1, 0)) return true;
        }
    }
    return false;
}

std::vector<Cell> Board::findMatches() const {
    // Общая маска на оба прохода: пересекающиеся серии (формы L и T)
    // попадают в результат целиком и ровно по одному разу.
    std::vector<char> marked(cells_.size(), 0);

    auto scan = [&](int dr, int dc, int outerCount, int innerCount) {
        for (int outer = 0; outer < outerCount; ++outer) {
            int inner = 0;
            while (inner < innerCount) {
                const int r     = dc ? outer : inner;
                const int c     = dc ? inner : outer;
                const int color = at(r, c);
                int       run   = 1;
                if (color != kEmpty) {
                    while (inner + run < innerCount) {
                        const int nr = dc ? outer : inner + run;
                        const int nc = dc ? inner + run : outer;
                        if (at(nr, nc) != color) break;
                        ++run;
                    }
                }
                if (color != kEmpty && run >= 3) {
                    for (int k = 0; k < run; ++k) {
                        const int nr = dc ? outer : inner + k;
                        const int nc = dc ? inner + k : outer;
                        marked[index(nr, nc)] = 1;
                    }
                }
                inner += run;
            }
        }
    };

    scan(0, 1, rows_, cols_);  // по строкам
    scan(1, 0, cols_, rows_);  // по столбцам

    std::vector<Cell> result;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (marked[index(r, c)]) result.push_back({r, c});
        }
    }
    return result;
}

void Board::clear(const std::vector<Cell>& cells) {
    for (const Cell& cell : cells) set(cell.r, cell.c, kEmpty);
}

std::vector<FallMove> Board::applyGravity() {
    std::vector<FallMove> moves;

    for (int c = 0; c < cols_; ++c) {
        // Уплотняем столбец вниз: write ползёт снизу вверх по свободным местам.
        int write = rows_ - 1;
        for (int r = rows_ - 1; r >= 0; --r) {
            if (at(r, c) == kEmpty) continue;
            if (write != r) {
                set(write, c, at(r, c));
                set(r, c, kEmpty);
                moves.push_back({c, r, write, false});
            }
            --write;
        }

        // Строки 0..write остались пустыми — рождаем новые фишки над полем.
        // Стартовая строка t - spawnCount держит их на дистанции в одну клетку
        // друг от друга, поэтому колонна влетает сверху ровным строем.
        const int spawnCount = write + 1;
        for (int r = write; r >= 0; --r) {
            set(r, c, pickColor());
            moves.push_back({c, r - spawnCount, r, true});
        }
    }

    return moves;
}

bool Board::hasValidMove() const {
    Board probe(*this);
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (c + 1 < cols_) {
                probe.swapCells({r, c}, {r, c + 1});
                if (probe.hasAnyMatch()) return true;
                probe.swapCells({r, c}, {r, c + 1});
            }
            if (r + 1 < rows_) {
                probe.swapCells({r, c}, {r + 1, c});
                if (probe.hasAnyMatch()) return true;
                probe.swapCells({r, c}, {r + 1, c});
            }
        }
    }
    return false;
}

std::vector<int> Board::shuffle() {
    const std::vector<int> original = cells_;

    std::vector<int> perm(cells_.size());
    std::iota(perm.begin(), perm.end(), 0);

    for (int attempt = 0; attempt < 500; ++attempt) {
        std::shuffle(perm.begin(), perm.end(), rng_);
        for (size_t i = 0; i < perm.size(); ++i) cells_[i] = original[perm[i]];
        if (!hasAnyMatch() && hasValidMove()) return perm;
    }

    // Набор цветов оказался безнадёжным (бывает на крошечных полях) —
    // собираем поле заново; визуальный слой просто подставит новые цвета.
    reset();
    std::iota(perm.begin(), perm.end(), 0);
    return perm;
}

}  // namespace m3
