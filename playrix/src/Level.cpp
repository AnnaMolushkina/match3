#include "Level.h"

#include "Config.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <fstream>

namespace m3 {
namespace {

// Убирает пробелы, табуляции и переносы строк в начале и конце строки.
std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t      b  = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

// Без исключений: Emscripten по умолчанию линкует libc++ без их раскрутки,
// и std::stoi на мусорном значении просто оборвал бы работу.

// Возвращает true и записывает в out, если текст — корректное целое число,
bool parseInt(const std::string& text, int& out) {
    if (text.empty()) return false;

    char*           end   = nullptr;
    const long      value = std::strtol(text.c_str(), &end, 10);
    if (end != text.c_str() + text.size()) return false;
    if (value < INT_MIN || value > INT_MAX) return false;

    out = static_cast<int>(value);
    return true;
}

}  // namespace

// Панель с целью уровня занимает полосу слева от поля, поэтому смещение
// по X складывается из её ширины и отступа, а сверху остаётся только отступ.
int Level::windowW() const { return boardX() + boardW() + cfg::kMargin; }
int Level::windowH() const { return boardH() + 2 * cfg::kMargin; }
int Level::boardX() const { return cfg::kPanelW + cfg::kMargin; }
int Level::boardY() const { return cfg::kMargin; }

bool loadLevel(const char* path, Level& out, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = std::string("не удалось открыть файл уровня: ") + path;
        return false;
    }

    Level level;
    level.chips.clear();

    // Номер спрайта целевого цвета так, как он записан в файле; ниже
    // переводится в индекс внутри level.chips. -1 — ключ не встретился.
    int goalSprite = -1;

    std::string line;
    int         lineNo = 0;
    while (std::getline(file, line)) {
        ++lineNo;
        if (size_t hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        line = trim(line);
        if (line.empty()) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            error = "строка " + std::to_string(lineNo) + ": ожидался формат `ключ = значение`";
            return false;
        }
        const std::string key   = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        bool ok = true;
        if (key == "rows") {
            ok = parseInt(value, level.rows);
        } else if (key == "cols") {
            ok = parseInt(value, level.cols);
        } else if (key == "tile") {
            ok = parseInt(value, level.tile);
        } else if (key == "colors") {
            ok = parseInt(value, level.colors);
        } else if (key == "chip") {
            int sprite = 0;
            ok = parseInt(value, sprite);
            if (ok) level.chips.push_back(sprite);
        } else if (key == "goal_color") {
            ok = parseInt(value, goalSprite);
        } else if (key == "goal_amount") {
            ok = parseInt(value, level.goalAmount);
        } else {
            error = "строка " + std::to_string(lineNo) + ": неизвестный ключ `" + key + "`";
            return false;
        }

        if (!ok) {
            error = "строка " + std::to_string(lineNo) + ": `" + value + "` не число";
            return false;
        }
    }

    // Проверки
    if (level.rows < 3 || level.cols < 3) {
        error = "rows и cols должны быть не меньше 3";
        return false;
    }
    if (level.tile < 16) {
        error = "tile должен быть не меньше 16";
        return false;
    }
    // Меньше трёх цветов — начальную раскладку без автоматчей не собрать.
    if (level.colors < 3) {
        error = "colors должно быть не меньше 3";
        return false;
    }
    if (static_cast<int>(level.chips.size()) != level.colors) {
        error = "задано " + std::to_string(level.chips.size()) + " строк `chip`, а colors = " +
                std::to_string(level.colors);
        return false;
    }
    for (size_t i = 0; i < level.chips.size(); ++i) {
        const int sprite = level.chips[i];
        if (sprite < 0 || sprite >= cfg::kChipSpriteCount) {
            error = "chip = " + std::to_string(sprite) + ": в палитре только " +
                    std::to_string(cfg::kChipSpriteCount) +
                    " спрайтов (см. kChipSprites в src/Config.h)";
            return false;
        }
        // Дубль — это два одинаковых на вид цвета: матчи собирались бы там,
        // где игрок их не видит.
        const auto upTo = level.chips.begin() + static_cast<long>(i);
        if (std::find(level.chips.begin(), upTo, sprite) != upTo) {
            error = "chip = " + std::to_string(sprite) + " указан дважды";
            return false;
        }
    }
    if (goalSprite < 0) {
        error = "не задан ключ `goal_color`";
        return false;
    }
    // Цель задаётся номером спрайта, а внутри игры цвета плотно пронумерованы
    // от нуля — переводим одно в другое сразу при загрузке.
    const auto goalIt = std::find(level.chips.begin(), level.chips.end(), goalSprite);
    if (goalIt == level.chips.end()) {
        error = "goal_color = " + std::to_string(goalSprite) + ": такого цвета нет среди строк `chip`";
        return false;
    }
    level.goalColor = static_cast<int>(goalIt - level.chips.begin());

    if (level.goalAmount <= 0) {
        error = "goal_amount должен быть больше нуля";
        return false;
    }

    out = std::move(level);
    return true;
}

}  // namespace m3
