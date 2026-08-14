// Активация уже стоящих на поле бустеров: очередь и волны срабатывания.
// Реализует ту часть класса Game, что отвечает на вопрос "бустер X сработал —
// что именно удалить, когда это показать и кого ещё это заденет по цепочке".

#include "Game.h"

#include "BoosterChain.h"
#include "Config.h"

#include <cstdio>

namespace m3 {

// Клетки, которые удалит бустер.
// immediate — подмножество out, которое нужно убрать сразу (соседи самолётика).
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

        case cfg::BoosterType::Bomb:  // квадрат 5x5 вокруг бустера — без полёта,
                                      // поэтому весь снос сразу же immediate
            for (int r = cell.r - 2; r <= cell.r + 2; ++r) {
                for (int c = cell.c - 2; c <= cell.c + 2; ++c) {
                    if (board_->inside(r, c)) add({r, c});
                }
            }
            immediate = out;
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
            immediate = out;  // тоже без полёта — сносится сразу
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

// Разбирает волны, пока не найдет такую, где у кого-то есть полёт —
// тогда запускает общий Phase::Boosting на всю волну разом.
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

// Разворачивает уже готовый результат комбо в состояние цепочки — так же,
// как advanceBoosterChain разворачивает один QueuedBooster, только blast/
// immediate/flights уже посчитаны в tryBoosterCombo, звать appendBoosterBlast
// не нужно.
void Game::beginBoosterComboChain(BoosterCombo combo, Cell a, Cell b, bool hasSwap, Cell swapA,
                                   Cell swapB) {
    auto contains = [](const std::vector<Cell>& v, Cell c) {
        for (const Cell& e : v) {
            if (e == c) return true;
        }
        return false;
    };

    boosterQueue_.clear();
    chainTriggered_ = {a, b};  // оба бустера уже объединились — не искать их как "найденные"
    chainBlast_.clear();
    chainHasSwap_ = hasSwap;
    chainSwapA_   = swapA;
    chainSwapB_   = swapB;

    // Бустеров среди immediate ищем ДО resolveImmediate — она их сотрёт с
    // доски. Найденные идут в boosterQueue_: раз у самого комбо нет полёта,
    // advanceBoosterChain() ниже разберёт их без всякой паузы, "сразу".
    for (const Cell& c : combo.immediate) {
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

    for (const Cell& c : combo.immediate) {
        views_[static_cast<size_t>(board_->index(c.r, c.c))].alpha = 0.0f;
    }
    resolveImmediate(combo.immediate);

    for (const Cell& c : combo.blast) {
        bool wasImmediate = false;
        for (const Cell& ic : combo.immediate) {
            if (ic == c) {
                wasImmediate = true;
                break;
            }
        }
        if (wasImmediate) continue;

        if (!contains(chainBlast_, c)) chainBlast_.push_back(c);

        if (contains(chainTriggered_, c)) continue;
        const cfg::BoosterType caught = board_->boosterAt(c.r, c.c);
        if (caught != cfg::BoosterType::None) {
            boosterQueue_.push_back(QueuedBooster{c, caught, false, Cell{}});
        }
    }

    if (!combo.flights.empty()) {
        views_[static_cast<size_t>(board_->index(a.r, a.c))].alpha = 0.0f;
        views_[static_cast<size_t>(board_->index(b.r, b.c))].alpha = 0.0f;
        flights_    = std::move(combo.flights);
        boostTimer_ = 0.0f;
        phase_      = Phase::Boosting;
        return;
    }

    advanceBoosterChain();  // нет полёта — сразу проверяем, не нашлось ли чего по цепочке
}

void Game::updateBoosting(float dt) {
    boostTimer_ += dt;
    if (boostTimer_ < cfg::kBoosterFlightTime) return;
    advanceBoosterChain();
}

}  // namespace m3
