#include "Game.h"

#include "Config.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <vector>

namespace {
// Красивое имя типа бустера для логов в браузере
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
    hasPendingSwap_ = false;
    views_.assign(views_.size(), ChipView{});
}

// Перевод пиксельных координат клика в клетку поля 
// false, если клик пришёлся мимо поля
bool Game::cellAt(int x, int y, Cell& out) const {
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
    // Повторный клик по той же фишке снимает выбор.
    if (cell == selected_) {
        hasSelection_ = false;
        return;
    }
    // Клик по не-соседней клетке: по условию ничего не происходит,
    // выбранная фишка так и остаётся выбранной.
    if (!neighbour(selected_, cell)) return;

    trySwap(selected_, cell);
    hasSelection_ = false;
}

// Обмен засчитывается, только если он собирает матч; иначе поле
// возвращается в исходное состояние и ход считается несостоявшимся.
bool Game::trySwap(Cell a, Cell b) {
    board_->swapCells(a, b);
    if (!board_->hasAnyMatch()) {
        board_->swapCells(a, b);
        return false;
    }
    // b — клетка второго клика: именно туда игрок передвинул фишку с a.
    // Если матч, рождённый этим свапом, даёт бустер, тот должен появиться там.
    pendingSwapA_   = a;
    pendingSwapB_   = b;
    hasPendingSwap_ = true;
    beginRemoving();
    return true;
}

void Game::update(float dt) {
    if (dt <= 0.0f) return;

    if (phase_ == Phase::Removing) {
        updateRemoving(dt);
    } else if (phase_ == Phase::Falling) {
        updateFalling(dt);
    } else if (phase_ == Phase::Shuffling) {
        updateShuffling(dt);
    }
}

// Клетка, где родится бустер группы. Приоритет — клетка второго клика (то
// место, куда игрок передвинул фишку с первого клика); если группа её не
// содержит, пробуем клетку первого клика; иначе (каскад без свапа) берём
// геометрический «центр» группы — просто чтобы бустер не сидел на самом краю.
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

// Собран матч (ручной или каскадный) — запоминаем его клетки и начинаем
// растворение. Поле в Board пока не тронуто: фишки ещё нужно показать.
void Game::beginRemoving() {
    // pendingSwap* живёт ровно один вызов: даже если матчей не найдётся или
    // это окажется не тот вызов, что сразу после свапа, флаг не должен
    // просочиться в следующий каскад.
    const bool hasSwap = hasPendingSwap_;
    const Cell swapA   = pendingSwapA_;
    const Cell swapB   = pendingSwapB_;
    hasPendingSwap_    = false;

    // Собираем все матчи, группируем их по связности, классифицируем каждую группу
    const std::vector<MatchGroup> groups = board_->collectMatchGroups();

    // Если после очистки и гравитации нет новых матчей, groups будет пуста
    if (groups.empty()) {
        // Поле успокоилось — только теперь можно смотреть на цель. Перезапускать
        // уровень посреди каскада нельзя: фишки ещё летят, и игрок не увидел бы,
        // чем закончился его ход.
        if (remaining() == 0) {
            restartLevel();
            return;
        }
        // Поле могло встать в позицию, где ни один обмен не собирает тройку.
        // Играть дальше нечем, поэтому фишки перетасовываются.
        if (!board_->hasValidMove()) {
            beginShuffle();
            return;
        }
        phase_ = Phase::Idle;  // ждём следующий ход
        return;
    }

    // Собираем все замэтченные клетки в один вектор, логируем каждый бустер
    // и заранее решаем, в какой клетке каждый из них появится.
    matchGroups_ = groups;
    boosterTargets_.assign(groups.size(), Cell{});

    std::vector<Cell> allCells;
    for (size_t i = 0; i < groups.size(); ++i) {
        const MatchGroup& group = groups[i];
        if (group.booster != cfg::BoosterType::None) {
            boosterTargets_[i] = pickBoosterCell(group, hasSwap, swapA, swapB);
            std::printf("[Матч] %s (%zu фишек)\n", boosterName(group.booster), group.cells.size());
        }
        for (const Cell& cell : group.cells) {
            allCells.push_back(cell);
        }
    }

    // Сохраняем в matches_ для визуализации отдельных фишек во время Removing фазы
    matches_ = allCells;
    phase_ = Phase::Removing;
    timer_ = 0.0f;
}

void Game::updateRemoving(float dt) {
    timer_ += dt;

    // Линейное затухание от 1 до 0 за kRemoveTime секунд.
    const float progress = timer_ / cfg::kRemoveTime;
    const float alpha    = (progress >= 1.0f) ? 0.0f : 1.0f - progress;
    for (const Cell& cell : matches_) views_[board_->index(cell.r, cell.c)].alpha = alpha;

    // Опадение трогается только здесь — когда прозрачность дошла до нуля.
    if (progress >= 1.0f) finishRemoving();
}

void Game::finishRemoving() {
    // Клетки, где вместо удаления рождается бустер: фишка там остаётся на
    // месте, просто «одевается» в бустер — её не чистим и не засчитываем
    // в цель, как обычную снятую фишку.
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

    // Считаем цель до clear: цвета фишек ещё на поле. В зачёт идут и каскадные
    // матчи — они разбираются этим же кодом.
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

// Свободное падение: все фишки стартуют одновременно с нулевой скоростью и
// разгоняются одной гравитацией. Отдельного тайминга нет — поэтому чем дальше
// фишке лететь, тем позже она приземлится, и опадение выглядит естественно.
void Game::updateFalling(float dt) {
    bool moving = false;

    for (ChipView& view : views_) {
        if (view.offsetY >= 0.0f) continue;  // эта фишка уже на месте

        view.speed = std::min(view.speed + cfg::kGravity * dt, cfg::kMaxFallV);
        view.offsetY += view.speed * dt;
        if (view.offsetY >= 0.0f) {
            view.offsetY = 0.0f;
            view.speed   = 0.0f;
        } else {
            moving = true;
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

    // Сглаживание: доля пути, которую фишке ещё осталось пролететь. Летят все
    // одновременно и одно и то же время — в отличие от падения, тут нет
    // физики, есть просто перестановка, и разнобой прилёта был бы лишним.
    const float left = 1.0f - progress * progress * (3.0f - 2.0f * progress);

    for (size_t i = 0; i < views_.size(); ++i) {
        const int index = static_cast<int>(i);
        const int dc    = shuffleFrom_[i].c - index % level_->cols;
        const int dr    = shuffleFrom_[i].r - index / level_->cols;
        views_[i].offsetX = static_cast<float>(dc * level_->tile) * left;
        views_[i].offsetY = static_cast<float>(dr * level_->tile) * left;
    }

    // После перетасовки на поле заведомо нет матчей и есть хотя бы один ход,
    // так что искать каскад не нужно — сразу ждём игрока.
    if (progress >= 1.0f) {
        views_.assign(views_.size(), ChipView{});
        shuffleFrom_.clear();
        phase_ = Phase::Idle;
    }
}

// Цель выполнена: собираем поле заново и обнуляем счётчик — уровень уходит
// на новый круг, перезагружать страницу не нужно.
void Game::restartLevel() {
    board_->reset();
    collected_ = 0;
    reset();
}

}  // namespace m3
