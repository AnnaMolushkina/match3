// Комбинации двух бустеров, свапнутых друг с другом

#include "Game.h"

#include "BoosterCombo.h"

#include <algorithm>
#include <optional>

namespace m3 {

std::pair<cfg::BoosterType, cfg::BoosterType> canonicalBoosterPair(cfg::BoosterType a, cfg::BoosterType b) {
    return a <= b ? std::make_pair(a, b) : std::make_pair(b, a);
}

std::optional<BoosterCombo> Game::tryBoosterCombo(cfg::BoosterType typeA, cfg::BoosterType typeB, Cell a,
                                                   Cell b) const {
    using T                = cfg::BoosterType;
    const auto [x, y]      = canonicalBoosterPair(typeA, typeB);
    const bool xIsRocket   = x == T::RocketHorizontal || x == T::RocketVertical;
    const bool yIsRocket   = y == T::RocketHorizontal || y == T::RocketVertical;

    BoosterCombo combo;
    auto add = [&](Cell c) {
        for (const Cell& e : combo.blast) {
            if (e == c) return;
        }
        combo.blast.push_back(c);
    };
    add(a);
    add(b);

    // р + р — строка и столбец клетки объединения; неважно, горизонтальные
    // ракеты были или вертикальные, срабатывают одинаково.
    if (xIsRocket && yIsRocket) {
        for (int c = 0; c < board_->cols(); ++c) add({b.r, c});
        for (int r = 0; r < board_->rows(); ++r) add({r, b.c});

        const auto row = static_cast<float>(b.r);
        const auto col = static_cast<float>(b.c);
        combo.flights.push_back({T::RocketHorizontal, row, col, row, 0.0f});
        combo.flights.push_back({T::RocketHorizontal, row, col, row, static_cast<float>(board_->cols() - 1)});
        combo.flights.push_back({T::RocketVertical, row, col, 0.0f, col});
        combo.flights.push_back({T::RocketVertical, row, col, static_cast<float>(board_->rows() - 1), col});
        return combo;
    }

    // р + б — 3 строки и 3 столбца вокруг клетки объединения.
    if (xIsRocket && y == T::Bomb) {
        return std::nullopt;
    }

    // р + с — самолётик летит в фишку нужного цвета и сносит там всю строку
    // или столбец (зависит от того, горизонтальная ракета была или вертикальная).
    if (xIsRocket && y == T::Airplane) {
        return std::nullopt;
    }

    // ш + ш — снести все фишки на поле. Без полёта — весь снос сразу же immediate.
    if (x == T::Rainbow && y == T::Rainbow) {
        for (int r = 0; r < board_->rows(); ++r) {
            for (int c = 0; c < board_->cols(); ++c) add({r, c});
        }
        combo.immediate = combo.blast;
        return combo;
    }

    // б + б — квадрат 7x7 вокруг клетки объединения. Без полёта — сразу immediate.
    if (x == T::Bomb && y == T::Bomb) {
        for (int r = b.r - 3; r <= b.r + 3; ++r) {
            for (int c = b.c - 3; c <= b.c + 3; ++c) {
                if (board_->inside(r, c)) add({r, c});
            }
        }
        combo.immediate = combo.blast;
        return combo;
    }

    // б + с — самолётик летит в фишку нужного цвета, но сносит там не одну
    // клетку, а квадрат 3x3.
    if (x == T::Bomb && y == T::Airplane) {
        return std::nullopt;
    }

    // с + с — сносятся все 8 соседей (включая диагонали), рождаются 3
    // самолётика, каждый летит в свою клетку нужного цвета.
    if (x == T::Airplane && y == T::Airplane) {
        return std::nullopt;
    }

    return std::nullopt;
}

bool isRainbowSpawnPair(cfg::BoosterType a, cfg::BoosterType b, cfg::BoosterType& spawnType) {
    using T = cfg::BoosterType;
    if (a == T::Rainbow && b != T::Rainbow) {
        spawnType = b;
        return true;
    }
    if (b == T::Rainbow && a != T::Rainbow) {
        spawnType = a;
        return true;
    }
    return false;
}


void Game::beginRainbowSpawnCombo(cfg::BoosterType spawnType, Cell a, Cell b) {
    // a/b сами по себе, удаляем их мгновенно и
    // сразу спавним новые бустеры, не дожидаясь ни гравитации после a/b, ни
    // матча, который она могла случайно собрать (тот разрешится сам на
    // следующей проверке startRemoving).
    views_[static_cast<size_t>(board_->index(a.r, a.c))].alpha = 0.0f;
    views_[static_cast<size_t>(board_->index(b.r, b.c))].alpha = 0.0f;
    resolveImmediate({a, b});
    beginRainbowSpawn(spawnType);
}

// ~10% клеток мгновенно (без затухания старой фишки) становятся бустерами
// type — для ракеты каждая клетка сама решает, горизонтальная она или
// вертикальная. Дальше — пауза (Phase::SpawnHold), чтобы игрок успел их
// увидеть, прежде чем они все одновременно активируются.
void Game::beginRainbowSpawn(cfg::BoosterType type) {
    std::vector<Cell> candidates;
    for (int r = 0; r < board_->rows(); ++r) {
        for (int c = 0; c < board_->cols(); ++c) {
            if (board_->boosterAt(r, c) == cfg::BoosterType::None) candidates.push_back({r, c});
        }
    }

    const int count = std::max(1, board_->cellCount() * 10 / 100);
    pendingActivateCells_ = board_->pickRandomCells(std::move(candidates), count);

    for (const Cell& cell : pendingActivateCells_) {
        cfg::BoosterType actual = type;
        if (type == cfg::BoosterType::RocketHorizontal || type == cfg::BoosterType::RocketVertical) {
            actual = board_->randomBool() ? cfg::BoosterType::RocketHorizontal : cfg::BoosterType::RocketVertical;
        }
        board_->setBooster(cell.r, cell.c, actual);
        views_[static_cast<size_t>(board_->index(cell.r, cell.c))] = ChipView{};  // виден сразу, без затухания
    }

    phase_ = Phase::SpawnHold;
    timer_ = 0.0f;
}

void Game::updateSpawnHold(float dt) {
    timer_ += dt;
    if (timer_ < cfg::kBoosterSpawnHoldTime) return;

    std::vector<QueuedBooster> initial;
    for (const Cell& cell : pendingActivateCells_) {
        initial.push_back(QueuedBooster{cell, board_->boosterAt(cell.r, cell.c), false, Cell{}});
    }
    pendingActivateCells_.clear();
    beginBoosterChain(std::move(initial), /*hasSwap=*/false, Cell{}, Cell{});
}

}  // namespace m3
