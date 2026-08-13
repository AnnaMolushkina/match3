#pragma once

#include <vector>

#include "Board.h"
#include "ChipView.h"
#include "Level.h"

namespace m3 {

// Правила игры: выбор фишки кликом, обмен с соседней клеткой, разбор матчей.
// Как и Board, обходится без SDL — на вход приходят пиксельные координаты
// клика, наружу отдаётся только выбранная клетка и состояние фишек.
//
// Поле в Board меняется мгновенно, а игра держит поверх него фазы анимации:
// сначала матч растворяется, и только потом фишки опадают. Пока анимация идёт,
// клики игнорируются — иначе игрок мог бы двинуть фишку, которой уже нет.
class Game {
public:
    Game(Board& board, const Level& level);

    // Левый клик мышью; x и y — координаты внутри окна.
    void onClick(int x, int y);

    // Продвинуть анимации на dt секунд.
    void update(float dt);

    // Клетка под подсветкой либо nullptr, если ничего не выбрано.
    const Cell* selected() const { return hasSelection_ ? &selected_ : nullptr; }

    // Смещение и прозрачность каждой фишки; индексы те же, что у Board.
    const std::vector<ChipView>& views() const { return views_; }

    // Сколько фишек целевого цвета осталось собрать. 0 — цель выполнена.
    int remaining() const;

    // Снять выбор и оборвать анимации — нужно после перегенерации поля.
    void reset();

private:
    // Фазы поля. Falling наступает строго после Removing: по условию опадение
    // не должно начинаться, пока эффект удаления не отыграл до конца.
    enum class Phase {
        Idle,       // ждём хода игрока
        Removing,   // матч уходит в прозрачность
        Falling,    // фишки опадают, сверху прилетают новые
        Shuffling,  // ходов не осталось, фишки разлетаются по новым клеткам
    };

    bool        cellAt(int x, int y, Cell& out) const;
    static bool neighbour(Cell a, Cell b);
    bool        trySwap(Cell a, Cell b);

    void beginRemoving();
    void finishRemoving();
    void updateRemoving(float dt);
    void updateFalling(float dt);
    void beginShuffle();
    void updateShuffling(float dt);
    void restartLevel();

    // Клетка, в которой должен появиться бустер группы: клетка второго клика
    // (или первого — если группа не задела вторую), иначе — центр группы.
    static Cell pickBoosterCell(const MatchGroup& group, bool hasSwap, Cell swapA, Cell swapB);

    // Добавляет в out клетки, которые снесёт бустер типа type, лежащий в cell
    // (это его положение уже ПОСЛЕ свапа, если он был). partner — вторая
    // клетка свапа, если бустер активирован свапом (nullptr при активации
    // двойным кликом либо цепочкой) — рэйнбоу берёт из неё цвет, который нужно
    // снести. Сам cell тоже добавляется: бустер снимается вместе с тем, на что
    // повлиял.
    void appendBoosterBlast(std::vector<Cell>& out, Cell cell, cfg::BoosterType type,
                             const Cell* partner) const;

    // Разворачивает цепочку срабатываний: cells — уже посчитанная область
    // поражения бустера(ов), сработавших от клика/свапа; roots — их клетки
    // (эти уже сработали, повторно не активируются). Каждый ДРУГОЙ бустер,
    // попавший в область поражения, активируется так же, как двойным кликом
    // (без партнёра свапа), его область добавляется в снос и тоже проверяется
    // на новые бустеры — пока цепочка не затухнет. Возвращает полный снос.
    std::vector<Cell> resolveBoosterChain(std::vector<Cell> cells, const std::vector<Cell>& roots) const;

    // Общее ядро для beginRemoving и активации бустеров: extraCells — то, что
    // нужно снести помимо обычных цветовых матчей (пусто для обычного хода и
    // каскада; область поражения бустера(ов) — для клика/свапа по бустеру).
    // Обычные матчи ищутся всегда — свап, двигающий бустер, мог заодно
    // подвинуть соседнюю фишку в новую комбинацию, и её нужно тоже разобрать:
    // обычным сносом или рождением нового бустера, как при простом ходе.
    void startRemoving(std::vector<Cell> extraCells, bool hasSwap, Cell swapA, Cell swapB);

    Board*       board_;
    const Level* level_;  // игра не меняет настройки уровня
    Cell         selected_{};
    bool         hasSelection_ = false;

    Phase                 phase_ = Phase::Idle;
    float                 timer_ = 0.0f;  // сколько секунд идёт текущая фаза
    std::vector<Cell>     matches_;       // клетки, которые сейчас растворяются
    std::vector<ChipView> views_;

    // Группы текущего матча вместе с клетками, куда сядут их бустеры —
    // заполняется в startRemoving и применяется к полю в finishRemoving,
    // когда эффект растворения уже доигран.
    std::vector<MatchGroup> matchGroups_;
    std::vector<Cell>       boosterTargets_;  // parallel to matchGroups_

    // Откуда прилетела фишка, лежащая теперь в клетке i, — заполняется на
    // время перетасовки, чтобы знать, какой путь ей осталось пролететь.
    std::vector<Cell> shuffleFrom_;

    int collected_ = 0;  // собрано фишек целевого цвета
};

}  // namespace m3
