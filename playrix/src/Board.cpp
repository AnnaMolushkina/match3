#include "Board.h"

#include <algorithm>
#include <numeric>

namespace m3 {

Board::Board(int rows, int cols, int colors, uint32_t seed)
    : rows_(rows), cols_(cols), colors_(colors), cells_(rows * cols, kEmpty), rng_(seed) {}

// Случайный выбор цвета из диапазона [0, colors_ - 1].
int Board::pickColor() {
    std::uniform_int_distribution<int> dist(0, colors_ - 1);
    return dist(rng_);
}

// Случайная раскладка без готовых матчей и гарантированно с ходом.
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
    // Локальная лямбда функция для прохода по строкам или столбцам.
    auto scan = [&](int dr, int dc, int outerCount, int innerCount) {
        for (int outer = 0; outer < outerCount; ++outer) {
            int inner = 0;
            while (inner < innerCount) {
                // dc ? outer : inner — тернарный оператор 
                // ("если dc равен 1 — взять outer, иначе inner"). 
                // Так scan умеет двигаться либо построчно 
                // (перебирая столбцы во внутреннем цикле),
                // либо по столбцам (перебирая строки), без дублирования кода.
                const int r     = dc ? outer : inner;
                const int c     = dc ? inner : outer;
                const int color = at(r, c);
                int       run   = 1;
                // Считаем run — длину серии одинаковых фишек в направлении (dr, dc).
                if (color != kEmpty) {
                    while (inner + run < innerCount) {
                        const int nr = dc ? outer : inner + run;
                        const int nc = dc ? inner + run : outer;
                        if (at(nr, nc) != color) break;
                        ++run;
                    }
                }
                // Если серия длиной >= 3, то отмечаем все её клетки в маске.
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

// Стереть клетки после матча
void Board::clear(const std::vector<Cell>& cells) {
    for (const Cell& cell : cells) set(cell.r, cell.c, kEmpty);
}

// гравитация: падение фишек и спавн новых
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

        // Строки 0..write остались пустыми — спавним новые фишки над полем.
        const int spawnCount = write + 1;
        for (int r = write; r >= 0; --r) {
            set(r, c, pickColor());
            moves.push_back({c, r - spawnCount, r, true}); 
            // fromRow отрицателен для фишек, рождённых выше верхнего края поля
        }
    }

    return moves; // возвращаем список всех падений, чтобы визуальный слой знал, что куда летит
}

bool Board::hasValidMove() const {
    Board probe(*this); // создаём копию поля, чтобы не менять его при проверке
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
    // сохраняем текущую раскладку, чтобы потом проверить, 
    // что она не содержит матчей и имеет хотя бы один ход

    std::vector<int> perm(cells_.size()); // вектор перестановки индексов, который будем перемешивать
    std::iota(perm.begin(), perm.end(), 0); // заполняем perm числами 0, 1, ..cellCount()-1

    for (int attempt = 0; attempt < 500; ++attempt) {
        std::shuffle(perm.begin(), perm.end(), rng_); // случайным образом перемешиваем вектор индексов
        for (size_t i = 0; i < perm.size(); ++i) cells_[i] = original[perm[i]];
        // физически переставляем фишки на поле в соответствии с perm
        if (!hasAnyMatch() && hasValidMove()) return perm;
        // визуальный слой узнает, что куда летит по этой перестановке: result[newIndex] == oldIndex
    }

    // Набор цветов оказался безнадёжным —
    // собираем поле заново; визуальный слой просто подставит новые цвета.
    reset();
    std::iota(perm.begin(), perm.end(), 0);
    return perm;
}

}  // namespace m3
