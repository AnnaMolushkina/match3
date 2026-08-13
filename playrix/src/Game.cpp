#include "Game.h"

#include "Config.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <vector>

namespace {
// Для логов в браузере
const char* boosterName(cfg::BoosterType type) {
    switch (type) {
        case cfg::BoosterType::RocketHorizontal: return "Ракета горизонтальная (4 в столбец)";
        case cfg::BoosterType::RocketVertical:   return "Ракета вертикальная (4 в строку)";
        case cfg::BoosterType::Rainbow:          return "Шар (5+ в ряд)";
        case cfg::BoosterType::Bomb:              return "Бомба (L/T)";
        case cfg::BoosterType::Airplane:          return "Самолётик (2x2)";
        default:                                  return "Обычный матч";
    }
}
}  // namespace

namespace m3 {

Game::Game(Board& board, const Level& level)
    : board_(&board), level_(&level), views_(static_cast<size_t>(board.cellCount())) {}

int Game::remaining() const {
    const int left = level_->goalAmount - collected_;
    return left > 0 ? left : 0;
}

// Поле пересобрали заново — все фишки на местах и полностью видимы.
void Game::reset() {
    hasSelection_ = false;
    phase_        = Phase::Idle;
    timer_        = 0.0f;
    matches_.clear();
    shuffleFrom_.clear();
    matchGroups_.clear();
    boosterTargets_.clear();
    flights_.clear();
    boostTimer_ = 0.0f;
    boosterQueue_.clear();
    chainTriggered_.clear();
    chainBlast_.clear();
    views_.assign(views_.size(), ChipView{});
}

// Перевод пиксельных координат клика в клетку поля 
// false, если клик пришёлся мимо поля
bool Game::cellAt(int x, int y, Cell& out) const {
    // переводим координаты клика в локальные координаты относительно поля
    const int localX = x - level_->boardX();
    const int localY = y - level_->boardY();
    if (localX < 0 || localY < 0) return false;

    const int c = localX / level_->tile;
    const int r = localY / level_->tile;
    if (r >= level_->rows || c >= level_->cols) return false;

    out = {r, c};
    return true;
}

// Соседние по стороне: выше, ниже, левее или правее. Диагональ соседом не считается.
bool Game::neighbour(Cell a, Cell b) {
    return std::abs(a.r - b.r) + std::abs(a.c - b.c) == 1;
}

void Game::onClick(int x, int y) {
    // Пока фишки растворяются или падают, ход принимать нельзя: поле в этот
    // момент уже переехало, и клик пришёлся бы не по той фишке, что видит игрок.
    if (phase_ != Phase::Idle) return;

    Cell cell;
    if (!cellAt(x, y, cell)) return;  // мимо поля — выбор не трогаем

    // Первый клик — просто запоминаем фишку.
    if (!hasSelection_) {
        selected_     = cell;
        hasSelection_ = true;
        return;
    }
    // Повторный клик по той же фишке снимает выбор — или, если это бустер,
    // активирует его на месте (без свапа, без клетки-партнёра).
    if (cell == selected_) {
        hasSelection_ = false;
        const cfg::BoosterType booster = board_->boosterAt(cell.r, cell.c);
        if (booster != cfg::BoosterType::None) {
            beginBoosterChain({QueuedBooster{cell, booster, false, Cell{}}}, /*hasSwap=*/false, Cell{},
                               Cell{});
        }
        return;
    }
    // Клик по не-соседней клетке: по условию ничего не происходит,
    // выбранная фишка так и остаётся выбранной.
    if (!neighbour(selected_, cell)) {
        selected_     = cell;
        hasSelection_ = true;
        return;
    }

    trySwap(selected_, cell);
    hasSelection_ = false;
}

// Обмен засчитывается, только если он собирает матч или затрагивает бустер;
// иначе поле возвращается в исходное состояние и ход считается несостоявшимся.
bool Game::trySwap(Cell a, Cell b) {
    // Запоминаем до свапа
    const cfg::BoosterType boosterA = board_->boosterAt(a.r, a.c);
    const cfg::BoosterType boosterB = board_->boosterAt(b.r, b.c);

    if (boosterA != cfg::BoosterType::None || boosterB != cfg::BoosterType::None) {
        // Свап с бустером — это способ его активировать
        board_->swapCells(a, b);

        std::vector<QueuedBooster> initial;
        if (boosterA != cfg::BoosterType::None) initial.push_back(QueuedBooster{b, boosterA, true, a});
        if (boosterB != cfg::BoosterType::None) initial.push_back(QueuedBooster{a, boosterB, true, b});

        beginBoosterChain(std::move(initial), /*hasSwap=*/true, a, b);
        return true;
    }

    board_->swapCells(a, b);
    if (!board_->hasAnyMatch()) {
        board_->swapCells(a, b);
        return false;
    }

    startRemoving({}, /*hasSwap=*/true, a, b);
    return true;
}

void Game::update(float dt) {
    if (dt <= 0.0f) return;

    // Гравитация общая и безусловная
    stepGravity(dt);

    if (phase_ == Phase::Boosting) {
        updateBoosting(dt);
    } else if (phase_ == Phase::Removing) {
        updateRemoving(dt);
    } else if (phase_ == Phase::Falling) {
        updateFalling();
    } else if (phase_ == Phase::Shuffling) {
        updateShuffling(dt);
    }
}

// Клетка, где появится бустер группы. 
//Приоритет — клетка второго клика;
//иначе берём геометрический «центр» группы.
Cell Game::pickBoosterCell(const MatchGroup& group, bool hasSwap, Cell swapA, Cell swapB) {
    if (hasSwap) {
        for (const Cell& cell : group.cells) {
            if (cell == swapB) return cell;
        }
        for (const Cell& cell : group.cells) {
            if (cell == swapA) return cell;
        }
    }

    std::vector<Cell> sorted = group.cells;
    std::sort(sorted.begin(), sorted.end(), [](const Cell& a, const Cell& b) {
        return a.r != b.r ? a.r < b.r : a.c < b.c;
    });
    return sorted[sorted.size() / 2];
}

// Клетки, которые удалит бустер.
// immediate — подмножество out, которое нужно убрать сразу, (соседи самолётика).
// Для ракеты/самолётика ещё заполняется flights — описание полёта.
// partner — вторая клетка свапа (nullptr при активации двойным кликом)
void Game::appendBoosterBlast(std::vector<Cell>& out, std::vector<Cell>& immediate,
                               std::vector<BoosterFlight>& flights, Cell cell, cfg::BoosterType type,
                               const Cell* partner) const {
    auto add = [&](Cell c) {
        for (const Cell& existing : out) {
            if (existing == c) return;
        }
        out.push_back(c);
    };

    add(cell);  // бустер удаляется вместе с тем, на что он повлиял

    const auto row = static_cast<float>(cell.r);
    const auto col = static_cast<float>(cell.c);

    switch (type) {
        case cfg::BoosterType::RocketHorizontal:  // удаляет всю строку
            for (int c = 0; c < board_->cols(); ++c) add({cell.r, c});
            // "Дублируется" — две ракеты летят от места активации к обоим
            // краям строки.
            flights.push_back({type, row, col, row, 0.0f});
            flights.push_back({type, row, col, row, static_cast<float>(board_->cols() - 1)});
            break;

        case cfg::BoosterType::RocketVertical:  // удаляет весь столбец
            for (int r = 0; r < board_->rows(); ++r) add({r, cell.c});
            flights.push_back({type, row, col, 0.0f, col});
            flights.push_back({type, row, col, static_cast<float>(board_->rows() - 1), col});
            break;

        case cfg::BoosterType::Bomb:  // квадрат 5x5 вокруг бустера
            for (int r = cell.r - 2; r <= cell.r + 2; ++r) {
                for (int c = cell.c - 2; c <= cell.c + 2; ++c) {
                    if (board_->inside(r, c)) add({r, c});
                }
            }
            break;

        case cfg::BoosterType::Rainbow: {
            // Со свапом — цвет фишки-партнера.
            // Без свапа (клик), либо если партнер сам оказался бустером — случайный цвет
            int color = partner ? board_->at(partner->r, partner->c) : -1;
            if (color < 0) color = board_->randomColor();
            for (int r = 0; r < board_->rows(); ++r) {
                for (int c = 0; c < board_->cols(); ++c) {
                    if (board_->at(r, c) == color) add({r, c});
                }
            }
            break;
        }

        case cfg::BoosterType::Airplane: {
            // 4 соседа по сторонам удаляются сразу, не дожидаясь долёта до цели
            // Сам самолётик — тоже immediate, а не только его соседи.
            immediate.push_back(cell);

            const Cell neighbours[4] = {
                {cell.r - 1, cell.c},
                {cell.r + 1, cell.c},
                {cell.r, cell.c - 1},
                {cell.r, cell.c + 1},
            };
            for (const Cell& n : neighbours) {
                if (board_->inside(n.r, n.c)) {
                    add(n);
                    immediate.push_back(n);
                }
            }

            // "Умно" здесь — случайная фишка того цвета, что нужен для цели,
            // но не сам самолётик и не его соседи
            std::vector<Cell> excluded(std::begin(neighbours), std::end(neighbours));
            excluded.push_back(cell);
            Cell target;
            if (board_->randomCellOfColor(level_->goalColor, excluded, target)) {
                add(target);
                flights.push_back(
                    {type, row, col, static_cast<float>(target.r), static_cast<float>(target.c)});
            }
            break;
        }

        case cfg::BoosterType::None:
            break;
    }
}

// Общее ядро для обычного хода/каскада и активации бустера. extraCells —
// то, что нужно снести сверх обычных цветовых матчей: пусто для обычного
// случая, область поражения бустера(ов) — при клике/свапе по бустеру.
void Game::startRemoving(std::vector<Cell> extraCells, bool hasSwap, Cell swapA, Cell swapB) {
    const std::vector<MatchGroup> groups = board_->collectMatchGroups();

    if (groups.empty() && extraCells.empty()) {
        // Поле успокоилось — только теперь можно смотреть на цель. Перезапускать
        // уровень посреди каскада нельзя: фишки ещё летят, и игрок не увидел бы,
        // чем закончился его ход.
        if (remaining() == 0) {
            restartLevel();
            return;
        }
        // Поле могло встать в позицию, где ни один обмен не собирает тройку.
        // фишки перетасовываются.
        if (!board_->hasValidMove()) {
            beginShuffle();
            return;
        }
        phase_ = Phase::Idle;  // ждём следующий ход
        return;
    }

    // Собираем все замэтченные клетки в один вектор поверх extraCells,
    // логируем каждый новый бустер и заранее решаем, в какой клетке он появится.
    matchGroups_ = groups;
    boosterTargets_.assign(groups.size(), Cell{});

    std::vector<Cell> allCells = std::move(extraCells);
    auto              addCell  = [&](Cell c) {
        for (const Cell& existing : allCells) {
            if (existing == c) return;
        }
        allCells.push_back(c);
    };

    for (size_t i = 0; i < groups.size(); ++i) {
        const MatchGroup& group = groups[i];
        if (group.booster != cfg::BoosterType::None) {
            boosterTargets_[i] = pickBoosterCell(group, hasSwap, swapA, swapB);
            std::printf("[Матч] %s (%zu фишек)\n", boosterName(group.booster), group.cells.size());
        }
        for (const Cell& cell : group.cells) addCell(cell);
    }

    // Сохраняем в matches_ для визуализации отдельных фишек во время Removing фазы
    matches_ = std::move(allCells);
    phase_   = Phase::Removing;
    timer_   = 0.0f;
}

// Немедленно удаляет клетки с поля и запускает гравитацию
void Game::resolveImmediate(const std::vector<Cell>& cells) {
    if (cells.empty()) return;

    for (const Cell& cell : cells) {
        if (board_->at(cell.r, cell.c) == level_->goalColor) ++collected_;
    }
    board_->clear(cells);

    const std::vector<FallMove> moves = board_->applyGravity();
    for (const FallMove& move : moves) {
        ChipView& view = views_[static_cast<size_t>(board_->index(move.toRow, move.col))];
        view           = ChipView{};  // локальный сброс — только эта клетка, не всё поле
        // Фишка начинает путь из своей прежней клетки; у заспавненных fromRow
        // отрицателен, поэтому они стартуют за верхним краем поля.
        view.offsetY = static_cast<float>((move.fromRow - move.toRow) * level_->tile);
    }
}

void Game::beginBoosterChain(std::vector<QueuedBooster> initial, bool hasSwap, Cell swapA, Cell swapB) {
    boosterQueue_ = std::move(initial);
    chainTriggered_.clear();
    chainBlast_.clear();
    chainHasSwap_ = hasSwap;
    chainSwapA_   = swapA;
    chainSwapB_   = swapB;
    advanceBoosterChain();
}

// Разбирает волны, пока не найдет такую, где у кого-то есть полёт
// тогда запускает общий Phase::Boosting на всю волну разом
void Game::advanceBoosterChain() {
    auto contains = [](const std::vector<Cell>& v, Cell c) {
        for (const Cell& e : v) {
            if (e == c) return true;
        }
        return false;
    };

    for (;;) {
        // Текущая волна — всё, что сейчас в очереди (initial из
        // beginBoosterChain либо всё, что нашла предыдущая волна).
        std::vector<QueuedBooster> wave = std::move(boosterQueue_);
        boosterQueue_.clear();

        std::vector<BoosterFlight> waveFlights;

        for (size_t i = 0; i < wave.size(); ++i) {
            const QueuedBooster next = wave[i];  // копия: wave ниже может вырасти
            if (contains(chainTriggered_, next.cell)) continue;  // на случай повторной находки
            chainTriggered_.push_back(next.cell);

            std::vector<Cell>          blast;
            std::vector<Cell>          immediate;
            std::vector<BoosterFlight> flights;
            appendBoosterBlast(blast, immediate, flights, next.cell, next.type,
                                next.hasPartner ? &next.partner : nullptr);
            std::printf("[Бустер] %s активирован (%zu клеток)\n", boosterName(next.type), blast.size());

            auto inWave = [&](Cell c) {
                for (const QueuedBooster& q : wave) {
                    if (q.cell == c) return true;
                }
                return false;
            };

            for (const Cell& c : immediate) {
                if (contains(chainTriggered_, c) || inWave(c)) continue;
                const cfg::BoosterType caught = board_->boosterAt(c.r, c.c);
                if (caught != cfg::BoosterType::None) {
                    wave.push_back(QueuedBooster{c, caught, false, Cell{}});
                }
            }

            // immediate снимаются и опадают прямо сейчас
            resolveImmediate(immediate);

            // Остальная (отложенная) часть области поражения копится на снос
            // в самом конце цепочки — бустер внутри неё активируется только
            // следующей волной, после долёта этой.
            for (const Cell& c : blast) {
                bool wasImmediate = false;
                for (const Cell& ic : immediate) {
                    if (ic == c) {
                        wasImmediate = true;
                        break;
                    }
                }
                if (wasImmediate) continue;

                if (!contains(chainBlast_, c)) chainBlast_.push_back(c);

                if (contains(chainTriggered_, c)) continue;
                bool alreadyQueued = false;
                for (const QueuedBooster& q : boosterQueue_) {
                    if (q.cell == c) {
                        alreadyQueued = true;
                        break;
                    }
                }
                if (alreadyQueued) continue;
                const cfg::BoosterType caught = board_->boosterAt(c.r, c.c);
                if (caught != cfg::BoosterType::None) {
                    boosterQueue_.push_back(QueuedBooster{c, caught, false, Cell{}});
                }
            }

            if (!flights.empty()) {
                bool ownCellWasImmediate = false;
                for (const Cell& ic : immediate) {
                    if (ic == next.cell) {
                        ownCellWasImmediate = true;
                        break;
                    }
                }
                if (!ownCellWasImmediate) {
                    views_[static_cast<size_t>(board_->index(next.cell.r, next.cell.c))].alpha = 0.0f;
                }
                for (const BoosterFlight& f : flights) waveFlights.push_back(f);
            }
        }

        if (!waveFlights.empty()) {
            flights_    = std::move(waveFlights);
            boostTimer_ = 0.0f;
            phase_      = Phase::Boosting;
            return;  // ждём долёта всей волны — остаток продолжит updateBoosting
        }

        if (boosterQueue_.empty()) break; 
    }

    // Волны кончились — цепочка полностью разобрана.
    flights_.clear();
    startRemoving(std::move(chainBlast_), chainHasSwap_, chainSwapA_, chainSwapB_);
}

void Game::updateBoosting(float dt) {
    boostTimer_ += dt;
    if (boostTimer_ < cfg::kBoosterFlightTime) return;
    advanceBoosterChain();
}

// Начинает разбор каскада
void Game::beginRemoving() {
    startRemoving({}, /*hasSwap=*/false, Cell{}, Cell{});
}

void Game::updateRemoving(float dt) {
    timer_ += dt;

    // Линейное затухание от 1 до 0 за kRemoveTime секунд. 
    const float progress = timer_ / cfg::kRemoveTime;
    const float alpha    = (progress >= 1.0f) ? 0.0f : 1.0f - progress;
    for (const Cell& cell : matches_) {
        ChipView& view = views_[board_->index(cell.r, cell.c)];
        view.alpha     = std::min(view.alpha, alpha);
    }

    // Опадение трогается только здесь — когда прозрачность дошла до нуля.
    if (progress >= 1.0f) finishRemoving();
}

void Game::finishRemoving() {
    std::vector<Cell> boosterCells;
    for (size_t i = 0; i < matchGroups_.size(); ++i) {
        if (matchGroups_[i].booster != cfg::BoosterType::None) {
            boosterCells.push_back(boosterTargets_[i]);
        }
    }
    auto isBoosterCell = [&](const Cell& cell) {
        for (const Cell& b : boosterCells) {
            if (b == cell) return true;
        }
        return false;
    };

    // Считаем цель до clear: цвета фишек ещё на поле. В зачёт идут и каскадные матчи
    std::vector<Cell> toClear;
    toClear.reserve(matches_.size());
    for (const Cell& cell : matches_) {
        if (isBoosterCell(cell)) continue;  // эта фишка не снимается — она стала бустером
        if (board_->at(cell.r, cell.c) == level_->goalColor) ++collected_;
        toClear.push_back(cell);
    }

    board_->clear(toClear);

    // Бустер отображается сразу после матча — до того, как поле подчинится
    // гравитации; сама фишка-носитель дальше падает как обычная.
    for (size_t i = 0; i < matchGroups_.size(); ++i) {
        if (matchGroups_[i].booster == cfg::BoosterType::None) continue;
        const Cell& cell = boosterTargets_[i];
        board_->setBooster(cell.r, cell.c, matchGroups_[i].booster);
    }

    const std::vector<FallMove> moves = board_->applyGravity();
    matches_.clear();
    matchGroups_.clear();
    boosterTargets_.clear();

    // Гравитация заняла все опустевшие клетки, так что прозрачных фишек
    // больше нет: сбрасываем визуальное состояние поля целиком.
    views_.assign(views_.size(), ChipView{});
    for (const FallMove& move : moves) {
        ChipView& view = views_[static_cast<size_t>(board_->index(move.toRow, move.col))];
        // Фишка начинает путь из своей прежней клетки. У заспавненных fromRow
        // отрицателен, поэтому они стартуют за верхним краем поля.
        view.offsetY = static_cast<float>((move.fromRow - move.toRow) * level_->tile);
    }

    phase_ = Phase::Falling;
}

// Физика падения — общая для всех фишек с offsetY < 0
void Game::stepGravity(float dt) {
    for (ChipView& view : views_) {
        if (view.offsetY >= 0.0f) continue;  // эта фишка уже на месте

        view.speed = std::min(view.speed + cfg::kGravity * dt, cfg::kMaxFallV);
        view.offsetY += view.speed * dt;
        if (view.offsetY >= 0.0f) {
            view.offsetY = 0.0f;
            view.speed   = 0.0f;
        }
    }
}

// Свободное падение: все фишки стартуют одновременно с нулевой скоростью и 
//разгоняются одной гравитацией
void Game::updateFalling() {
    bool moving = false;
    for (const ChipView& view : views_) {
        if (view.offsetY < 0.0f) {
            moving = true;
            break;
        }
    }

    // Поле остановилось — ищем каскад. Автоматч разбирается тем же кодом,
    // что и матч, собранный руками.
    if (!moving) beginRemoving();
}

// Ходов не осталось. Board тасует уже лежащие фишки и возвращает перестановку:
// по ней видно, из какой клетки каждая приехала, поэтому фишки не появляются на
// новых местах мгновенно, а перелетают туда.
void Game::beginShuffle() {
    const std::vector<int> perm = board_->shuffle();

    views_.assign(views_.size(), ChipView{});
    shuffleFrom_.resize(perm.size());
    for (size_t i = 0; i < perm.size(); ++i) {
        // perm[i] — индекс клетки, откуда взялась фишка, лежащая теперь в i.
        shuffleFrom_[i] = {perm[i] / level_->cols, perm[i] % level_->cols};
    }

    hasSelection_ = false;  // выбранной фишки на этом месте уже нет
    phase_        = Phase::Shuffling;
    timer_        = 0.0f;
}

void Game::updateShuffling(float dt) {
    timer_ += dt;
    float progress = timer_ / cfg::kShuffleTime;
    if (progress > 1.0f) progress = 1.0f;

    // Сглаживание: доля пути, которую фишке ещё осталось пролететь
    const float left = 1.0f - progress * progress * (3.0f - 2.0f * progress);

    for (size_t i = 0; i < views_.size(); ++i) {
        const int index = static_cast<int>(i);
        const int dc    = shuffleFrom_[i].c - index % level_->cols;
        const int dr    = shuffleFrom_[i].r - index / level_->cols;
        views_[i].offsetX = static_cast<float>(dc * level_->tile) * left;
        views_[i].offsetY = static_cast<float>(dr * level_->tile) * left;
    }

    // После перетасовки на поле заведомо нет матчей и есть хотя бы один ход
    if (progress >= 1.0f) {
        views_.assign(views_.size(), ChipView{});
        shuffleFrom_.clear();
        phase_ = Phase::Idle;
    }
}

// Цель выполнена: собираем поле заново и обнуляем счётчик — уровень уходит
// на новый круг
void Game::restartLevel() {
    board_->reset();
    collected_ = 0;
    reset();
}

}  // namespace m3
