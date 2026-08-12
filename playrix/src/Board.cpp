#include "Board.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <unordered_set>

namespace m3 {

Board::Board(int rows, int cols, int colors, uint32_t seed)
    : rows_(rows), cols_(cols), colors_(colors), cells_(rows * cols, kEmpty),
      boosters_(rows * cols, cfg::BoosterType::None), rng_(seed) {}

// Случайный выбор цвета из диапазона [0, colors_ - 1].
int Board::pickColor() {
    std::uniform_int_distribution<int> dist(0, colors_ - 1);
    return dist(rng_);
}

// Случайная раскладка без готовых матчей и гарантированно с ходом.
void Board::reset() {
    // Свежая раскладка — бустеров на поле ещё нет.
    boosters_.assign(cells_.size(), cfg::BoosterType::None);

    // Раскладываем по клеткам слева направо, сверху вниз, исключая цвет,
    // который дал бы тройку с двумя уже лежащими соседями слева или сверху,
    // а также исключая цвет, который бы создал 2x2 квадрат.
    std::vector<int> candidates;
    candidates.reserve(colors_);

    do {
        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_; ++c) {
                candidates.clear();
                for (int color = 0; color < colors_; ++color) {
                    // Проверка на линейные матчи (3+ в ряд)
                    if (c >= 2 && at(r, c - 1) == color && at(r, c - 2) == color) continue;
                    if (r >= 2 && at(r - 1, c) == color && at(r - 2, c) == color) continue;
                    
                    // Проверка на 2x2 квадрат (самолетик)
                    // (r, c) будет нижним-правым углом, остальные три уже заполнены
                    if (r >= 1 && c >= 1 && 
                        at(r - 1, c - 1) == color && 
                        at(r - 1, c) == color && 
                        at(r, c - 1) == color) {
                        continue;
                    }
                    
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
    std::swap(boosters_[index(a.r, a.c)], boosters_[index(b.r, b.c)]);
}

bool Board::matchesRunFrom(int r, int c, int dr, int dc) const {
    const int color = at(r, c);
    if (color < 0) return false;  // пусто или бустер — у бустера нет цвета для матча
    return inside(r + 2 * dr, c + 2 * dc) && at(r + dr, c + dc) == color &&
           at(r + 2 * dr, c + 2 * dc) == color;
}

bool Board::hasAnyMatch() const {
    // Проверяем линейные матчи (3+ в ряд)
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (matchesRunFrom(r, c, 0, 1) || matchesRunFrom(r, c, 1, 0)) return true;
        }
    }
    
    // Проверяем 2x2 квадраты (самолетики)
    for (int r = 0; r + 1 < rows_; ++r) {
        for (int c = 0; c + 1 < cols_; ++c) {
            const int color = at(r, c);
            if (color >= 0 &&
                at(r, c + 1) == color &&
                at(r + 1, c) == color &&
                at(r + 1, c + 1) == color) {
                return true;
            }
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
                // color < 0 — пустая клетка или бустер: у бустера нет цвета,
                // он не может ни начать, ни продолжить цветовую серию.
                if (color >= 0) {
                    while (inner + run < innerCount) {
                        const int nr = dc ? outer : inner + run;
                        const int nc = dc ? inner + run : outer;
                        if (at(nr, nc) != color) break;
                        ++run;
                    }
                }
                // Если серия длиной >= 3, то отмечаем все её клетки в маске.
                if (color >= 0 && run >= 3) {
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
    for (const Cell& cell : cells) {
        set(cell.r, cell.c, kEmpty);
        boosters_[index(cell.r, cell.c)] = cfg::BoosterType::None;
    }
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
                boosters_[index(write, c)] = boosters_[index(r, c)];
                boosters_[index(r, c)]     = cfg::BoosterType::None;
                moves.push_back({c, r, write, false});
            }
            --write;
        }

        // Строки 0..write остались пустыми — спавним новые фишки над полем.
        const int spawnCount = write + 1;
        for (int r = write; r >= 0; --r) {
            set(r, c, pickColor());
            boosters_[index(r, c)] = cfg::BoosterType::None;  // новая фишка без бустера
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
    const std::vector<cfg::BoosterType> originalBoosters = boosters_;  // бустеры едут вместе с фишками

    std::vector<int> perm(cells_.size()); // вектор перестановки индексов, который будем перемешивать
    std::iota(perm.begin(), perm.end(), 0); // заполняем perm числами 0, 1, ..cellCount()-1

    for (int attempt = 0; attempt < 500; ++attempt) {
        std::shuffle(perm.begin(), perm.end(), rng_); // случайным образом перемешиваем вектор индексов
        for (size_t i = 0; i < perm.size(); ++i) {
            cells_[i]    = original[perm[i]];
            boosters_[i] = originalBoosters[perm[i]];
        }
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

std::vector<Cell> Board::findPlanes() const {
    std::vector<char> marked(cells_.size(), 0);

    for (int r = 0; r + 1 < rows_; ++r) {
        for (int c = 0; c + 1 < cols_; ++c) {
            const int color = at(r, c);
            if (color < 0) continue;  // пусто или бустер — не может быть углом квадрата
            if (at(r, c + 1) != color || at(r + 1, c) != color || at(r + 1, c + 1) != color) continue;

            // Нашли 2x2 блок — помечаем его 4 клетки.
            marked[index(r, c)]         = 1;
            marked[index(r, c + 1)]     = 1;
            marked[index(r + 1, c)]     = 1;
            marked[index(r + 1, c + 1)] = 1;

            // Проверяем 8 клеток, касающихся блока по стороне (не по диагонали).
            // Идём в фиксированном порядке и берём первую подходящую —
            // фигуре нужна максимум одна лишняя клетка.
            const Cell ring[8] = {
                {r - 1, c},     {r - 1, c + 1},   // сверху
                {r + 2, c},     {r + 2, c + 1},   // снизу
                {r, c - 1},     {r + 1, c - 1},   // слева
                {r, c + 2},     {r + 1, c + 2},   // справа
            };
            for (const Cell& n : ring) {
                if (!inside(n.r, n.c)) continue;
                if (at(n.r, n.c) != color) continue;
                marked[index(n.r, n.c)] = 1;
                break;  // одной лишней клетки достаточно
            }
        }
    }

    std::vector<Cell> result;
    for (int r = 0; r < rows_; ++r)
        for (int c = 0; c < cols_; ++c)
            if (marked[index(r, c)]) result.push_back({r, c});
    return result;
}

// Есть ли внутри группы клеток хотя бы один цельный 2x2 квадрат.
bool Board::containsSquare(const std::vector<Cell>& cells) const {
    std::set<std::pair<int, int>> set;
    for (const Cell& cell : cells) set.insert({cell.r, cell.c});

    for (const Cell& cell : cells) {
        const int r = cell.r, c = cell.c;
        if (set.count({r, c + 1}) && set.count({r + 1, c}) && set.count({r + 1, c + 1})) {
            return true;
        }
    }
    return false;
}

// Классификация одной связной группы клеток по форме → тип бустера.
cfg::BoosterType Board::classifyMatch(const std::vector<Cell>& cells) const {
    if (cells.empty()) return cfg::BoosterType::None;

    const int cellCount = static_cast<int>(cells.size());

    // ---- Проверка 1: чистая горизонтальная или вертикальная линия ----
    bool allSameRow = true;
    bool allSameCol = true;
    for (size_t i = 1; i < cells.size(); ++i) {
        if (cells[i].r != cells[0].r) allSameRow = false;
        if (cells[i].c != cells[0].c) allSameCol = false;
    }

    if (allSameRow || allSameCol) {
        if (cellCount >= 5) return cfg::BoosterType::Rainbow;
        if (cellCount == 4) return cfg::BoosterType::Line;
        return cfg::BoosterType::None;
    }

    // ---- Проверка 2: максимальная линия (для T/L форм с 5+) ----
    // Если есть 5+ подряд в любом направлении → Rainbow (даже в T-форме)

    // Найти максимальную горизонтальную линию
    std::map<int, std::vector<int>> byRow;
    for (const auto& cell : cells) {
        byRow[cell.r].push_back(cell.c);
    }
    int maxHorizontal = 0;
    for (auto& [r, cols] : byRow) {
        std::sort(cols.begin(), cols.end());
        int count = 1;
        for (size_t i = 1; i < cols.size(); ++i) {
            if (cols[i] == cols[i-1] + 1) {
                count++;
            } else {
                maxHorizontal = std::max(maxHorizontal, count);
                count = 1;
            }
        }
        maxHorizontal = std::max(maxHorizontal, count);
    }

    // Найти максимальную вертикальную линию
    std::map<int, std::vector<int>> byCol;
    for (const auto& cell : cells) {
        byCol[cell.c].push_back(cell.r);
    }
    int maxVertical = 0;
    for (auto& [c, rows] : byCol) {
        std::sort(rows.begin(), rows.end());
        int count = 1;
        for (size_t i = 1; i < rows.size(); ++i) {
            if (rows[i] == rows[i-1] + 1) {
                count++;
            } else {
                maxVertical = std::max(maxVertical, count);
                count = 1;
            }
        }
        maxVertical = std::max(maxVertical, count);
    }

    int maxLine = std::max(maxHorizontal, maxVertical);
    if (maxLine >= 5) return cfg::BoosterType::Rainbow;

    // ---- Проверка 2: Самолетик может быть только из 4 или 5 фишек ----
    // Если 6+ фишек — это точно бомба, даже если есть 2x2
    if (cellCount <= 5 && containsSquare(cells)) {
        return cfg::BoosterType::Airplane;  // 4 клетки квадрата или 4 + 1 соседняя
    }

    // ---- Проверка 3: L/T форма (бомба) — занимает 2+ строк И 2+ столбцов ----
    int minR = cells[0].r, maxR = cells[0].r;
    int minC = cells[0].c, maxC = cells[0].c;
    for (const auto& cell : cells) {
        minR = std::min(minR, cell.r);
        maxR = std::max(maxR, cell.r);
        minC = std::min(minC, cell.c);
        maxC = std::max(maxC, cell.c);
    }

    if ((maxR - minR > 0) && (maxC - minC > 0)) {
        return cfg::BoosterType::Bomb;
    }

    return cfg::BoosterType::None;
}

// Разбить плоский список замэтченных клеток на связные компоненты по цвету.
// DFS: для каждой непосещённой клетки стартуем обход, собирая все клетки
// того же цвета, до которых можно добраться через соседей.
std::vector<std::vector<Cell>> Board::groupByColor(const std::vector<Cell>& cells) const {
    if (cells.empty()) return {};

    // Индексация для быстрого поиска
    std::unordered_set<int> cellSet;
    for (const Cell& cell : cells) cellSet.insert(index(cell.r, cell.c));

    std::unordered_set<int> visited;
    std::vector<std::vector<Cell>> groups;

    for (const Cell& cell : cells) {
        const int idx = index(cell.r, cell.c);
        if (visited.count(idx)) continue;

        // DFS для текущей компоненты
        std::vector<Cell> group;
        std::vector<Cell> stack;
        stack.push_back(cell);

        while (!stack.empty()) {
            Cell cur = stack.back();
            stack.pop_back();

            const int curIdx = index(cur.r, cur.c);
            if (visited.count(curIdx)) continue;
            visited.insert(curIdx);
            group.push_back(cur);

            // Проверяем 4 соседей (без диагоналей)
            const int dr[] = {-1, 1, 0, 0};
            const int dc[] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                const int nr = cur.r + dr[d];
                const int nc = cur.c + dc[d];
                const int nIdx = index(nr, nc);
                if (!inside(nr, nc)) continue;
                if (visited.count(nIdx)) continue;
                if (!cellSet.count(nIdx)) continue;
                // Дополнительная проверка: одного ли цвета?
                if (at(nr, nc) != at(cur.r, cur.c)) continue;
                stack.push_back({nr, nc});
            }
        }

        if (!group.empty()) {
            groups.push_back(std::move(group));
        }
    }

    return groups;
}

std::vector<MatchGroup> Board::collectMatchGroups() const {
    const std::vector<Cell> lineCells  = findMatches();
    const std::vector<Cell> planeCells = findPlanes();

    std::unordered_set<int> seen;
    std::vector<Cell>       merged;
    for (const Cell& cell : lineCells) {
        if (seen.insert(index(cell.r, cell.c)).second) merged.push_back(cell);
    }
    for (const Cell& cell : planeCells) {
        if (seen.insert(index(cell.r, cell.c)).second) merged.push_back(cell);
    }

    std::vector<MatchGroup> result;
    for (std::vector<Cell> group : groupByColor(merged)) {
        MatchGroup mg;
        mg.booster = classifyMatch(group);
        mg.cells   = std::move(group);
        result.push_back(std::move(mg));
    }
    return result;
}

}  // namespace m3
