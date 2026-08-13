#pragma once

#include "Board.h"
#include "ChipView.h"

// Вспомогательные типы и утилиты для активации уже стоящих на поле бустеров: очередь/волны срабатывания
namespace m3 {

// Один активированный бустер, ожидающий своей очереди в цепочке срабатываний
struct QueuedBooster {
    Cell             cell;
    cfg::BoosterType type       = cfg::BoosterType::None;
    bool             hasPartner = false;
    Cell             partner{};
};

// Логи в консоли браузера
inline const char* boosterName(cfg::BoosterType type) {
    switch (type) {
        case cfg::BoosterType::RocketHorizontal: return "Ракета горизонтальная (4 в столбец)";
        case cfg::BoosterType::RocketVertical:   return "Ракета вертикальная (4 в строку)";
        case cfg::BoosterType::Rainbow:          return "Шар (5+ в ряд)";
        case cfg::BoosterType::Bomb:              return "Бомба (L/T)";
        case cfg::BoosterType::Airplane:          return "Самолётик (2x2)";
        default:                                  return "Обычный матч";
    }
}

}  // namespace m3
