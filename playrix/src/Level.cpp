#include "Level.h"

#include "Config.h"

#include <climits>
#include <cstdlib>
#include <fstream>

namespace m3 {
namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t      b  = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

// Без исключений: Emscripten по умолчанию линкует libc++ без их раскрутки,
// и std::stoi на мусорном значении просто оборвал бы работу.
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
    level.chipSprites.clear();

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
        } else if (key == "colors") {
            ok = parseInt(value, level.colors);
        } else if (key == "tile") {
            ok = parseInt(value, level.tile);
        } else if (key == "goal_color") {
            ok = parseInt(value, level.goalColor);
        } else if (key == "goal_amount") {
            ok = parseInt(value, level.goalAmount);
        } else if (key == "chip") {
            level.chipSprites.push_back(value);
        } else if (key == "font") {
            level.fontFile = value;
        } else if (key == "background") {
            level.backgroundFile = value;
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
    // Меньше трёх цветов — начальную раскладку без автоматчей не собрать.
    if (level.colors < 3) {
        error = "colors должно быть не меньше 3";
        return false;
    }
    if (level.tile < 16) {
        error = "tile должен быть не меньше 16";
        return false;
    }
    if (static_cast<int>(level.chipSprites.size()) != level.colors) {
        error = "задано " + std::to_string(level.chipSprites.size()) + " строк `chip`, а colors = " +
                std::to_string(level.colors);
        return false;
    }
    if (level.goalColor < 0 || level.goalColor >= level.colors) {
        error = "goal_color должен быть в диапазоне 0.." + std::to_string(level.colors - 1);
        return false;
    }
    if (level.goalAmount <= 0) {
        error = "goal_amount должен быть больше нуля";
        return false;
    }
    if (level.fontFile.empty()) {
        error = "не задан ключ `font`";
        return false;
    }

    out = std::move(level);
    return true;
}

}  // namespace m3
