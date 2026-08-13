
// Конкретный случай удобно собирать так:
//   const auto [x, y] = canonicalBoosterPair(a, b);
//   if (x == RocketHorizontal && y == RocketHorizontal) { ... }        // линия+линия
//   if (x == RocketHorizontal && y == Bomb)              { ... }        // линия+бомба
//   ...

#include "Game.h"

#include "BoosterCombo.h"

#include <optional>

namespace m3 {

std::pair<cfg::BoosterType, cfg::BoosterType> canonicalBoosterPair(cfg::BoosterType a, cfg::BoosterType b) {
    return a <= b ? std::make_pair(a, b) : std::make_pair(b, a);
}

std::optional<BoosterCombo> Game::tryBoosterCombo(cfg::BoosterType /*a*/, cfg::BoosterType /*b*/,
                                                   Cell /*at*/) const {
    // TODO: 10 неупорядоченных пар (RocketH, RocketV, Bomb, Rainbow, Airplane друг с другом и с собой)
    return std::nullopt;
}

}  // namespace m3
