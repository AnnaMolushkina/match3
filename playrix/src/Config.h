#pragma once

#include <cstdint>

// Константы движка: тайминги, физика, палитра UI.
// Всё, что относится к конкретному уровню (размер поля, число цветов,
// цель), лежит не здесь, а в файле уровня — см. Level.h и assets/level.cfg.
namespace cfg {

// ---- путь к файлу уровня ------------------------------------------------
constexpr const char* kLevelFile = "assets/level.cfg";

// ---- раскладка окна -----------------------------------------------------
constexpr int kMargin = 24;   // поля вокруг игрового поля
constexpr int kPanelW = 180;  // ширина левой панели с целью уровня

// ---- тайминги, секунды --------------------------------------------------
constexpr float kSwapTime    = 0.15f;
constexpr float kRemoveTime  = 0.24f;
constexpr float kShuffleTime = 0.40f;
constexpr float kSquashTime  = 0.14f;
constexpr float kWinDelay    = 1.60f;
constexpr float kShuffleHold = 0.35f;  // пауза с надписью перед перетасовкой

// ---- физика падения -----------------------------------------------------
constexpr float kGravity  = 3600.0f;  // px/s^2
constexpr float kMaxFallV = 2800.0f;  // px/s

// ---- палитра ------------------------------------------------------------
struct Rgb {
    uint8_t r, g, b;
};

constexpr Rgb kBgColor        = {30, 30, 40};
constexpr Rgb kBoardColor     = {44, 44, 60};
constexpr Rgb kCellColorEven  = {54, 54, 72};
constexpr Rgb kCellColorOdd   = {48, 48, 65};
constexpr Rgb kSelectColor    = {255, 255, 255};
constexpr Rgb kTextColor      = {235, 235, 245};
//constexpr Rgb kProgressBg     = {60, 60, 78};
constexpr Rgb kProgressFill   = {112, 210, 130};

}  // namespace cfg
