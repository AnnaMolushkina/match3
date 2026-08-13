#pragma once

#include <utility>
#include <vector>

#include "Board.h"
#include "ChipView.h"

// Комбинации двух бустеров, свапнутых игроком друг с другом 

// Game::tryBoosterCombo сейчас всегда возвращает std::nullopt, и
// Game::trySwap в этом случае активирует оба бустера независимо, как и раньше.
namespace m3 {

struct BoosterCombo {
    std::vector<Cell>          blast;      // все клетки, которые удалит комбо (включая обе клетки-бустера)
    std::vector<Cell>          immediate;  // подмножество blast, которое сносится сразу, без ожидания полёта
    std::vector<BoosterFlight> flights;    // визуальные полёты комбо, если есть
};

// Приводит пару типов бустеров к каноническому (неупорядоченному) виду
// искать/switch'ить пары удобнее в одном фиксированном порядке.
// .first <= .second по значению enum.
std::pair<cfg::BoosterType, cfg::BoosterType> canonicalBoosterPair(cfg::BoosterType a, cfg::BoosterType b);

}  // namespace m3
