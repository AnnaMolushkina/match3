#include "Game.h"

#include "Config.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace m3 {

Game::Game(Board& board, const Level& level)
    : board_(&board), level_(&level), views_(static_cast<size_t>(board.cellCount())) {}

// Поле пересобрали заново — все фишки на местах и полностью видимы.
void Game::reset() {
    hasSelection_ = false;
    phase_        = Phase::Idle;
    timer_        = 0.0f;
    matches_.clear();
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
    beginRemoving();
    return true;
}

void Game::update(float dt) {
    if (dt <= 0.0f) return;

    if (phase_ == Phase::Removing) {
        updateRemoving(dt);
    } else if (phase_ == Phase::Falling) {
        updateFalling(dt);
    }
}

// Собран матч (ручной или каскадный) — запоминаем его клетки и начинаем
// растворение. Поле в Board пока не тронуто: фишки ещё нужно показать.
void Game::beginRemoving() {
    matches_ = board_->findMatches();
    if (matches_.empty()) {
        phase_ = Phase::Idle;  // поле успокоилось, ждём следующий ход
        return;
    }
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
    board_->clear(matches_);
    const std::vector<FallMove> moves = board_->applyGravity();
    matches_.clear();

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

}  // namespace m3
