#pragma once

#include <optional>
#include <vector>

#include "Board.h"
#include "BoosterChain.h"
#include "BoosterCombo.h"
#include "ChipView.h"
#include "Level.h"

namespace m3 {

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

    // Летящие спрайты бустеров (самолётик к цели, ракеты к краям поля) и то,
    // насколько далеко они пролетели: 0 — только взлетели, 1 — долетели.
    // Пусто вне Phase::Boosting.
    const std::vector<BoosterFlight>& flights() const { return flights_; }
    float flightProgress() const {
        const float p = boostTimer_ / cfg::kBoosterFlightTime;
        return p < 1.0f ? p : 1.0f;
    }

    // Сколько фишек целевого цвета осталось собрать. 0 — цель выполнена.
    int remaining() const;

    // Снять выбор и оборвать анимации — нужно после перегенерации поля.
    void reset();

private:
    // Фазы поля
    enum class Phase {
        Idle,       // ждём хода игрока
        Boosting,   // самолётик/ракеты летят к цели — перед обычным Removing
        Removing,   // матч уходит в прозрачность
        Falling,    // фишки опадают, сверху прилетают новые
        Shuffling,  // ходов не осталось, фишки разлетаются по новым клеткам
    };

    bool        cellAt(int x, int y, Cell& out) const;
    static bool neighbour(Cell a, Cell b);
    bool        trySwap(Cell a, Cell b);

    // ---- основной конвейер хода: свап/каскад → матч → удаление → гравитация ----
    // (Game.cpp)

    void beginRemoving();
    void finishRemoving();
    void updateRemoving(float dt);

    // Общий шаг физики падения — двигает offsetY любой фишки, у которой он < 0, к нулю.
    // Вызывается из update() каждый кадр безусловно (не только в
    // Phase::Falling), поэтому фишки могут опадать и посреди Phase::Boosting.
    void stepGravity(float dt);
    // Проверяет, всё ли осело (физика уже сделана в stepGravity этим кадром);
    // если да — переходит к поиску каскада.
    void updateFalling();

    void beginShuffle();
    void updateShuffling(float dt);
    void restartLevel();

    // Клетка, в которой должен появиться НОВЫЙ бустер, родившийся из матча
    // (не путать с активацией уже стоящего на поле — см. ниже).
    static Cell pickBoosterCell(const MatchGroup& group, bool hasSwap, Cell swapA, Cell swapB);

    // Общее ядро для beginRemoving и активации бустеров: extraCells — то, что
    // нужно удалить помимо обычных цветовых матчей (пусто для обычного хода и
    // каскада; область поражения бустера(ов) — для клика/свапа по бустеру).
    void startRemoving(std::vector<Cell> extraCells, bool hasSwap, Cell swapA, Cell swapB);

    // ---- активация уже стоящих на поле бустеров: очередь и волны срабатывания ----
    // (BoosterChain.cpp; типы — в BoosterChain.h)

    // Для удаления соседних фишек самолётика посреди Phase::Boosting
    void resolveImmediate(const std::vector<Cell>& cells);

    // Добавляет в out клетки, которые удалит бустер вместе с собой
    void appendBoosterBlast(std::vector<Cell>& out, std::vector<Cell>& immediate,
                             std::vector<BoosterFlight>& flights, Cell cell, cfg::BoosterType type,
                             const Cell* partner) const;

    // Запускает цепочку срабатываний бустеров, найденных в ходе одного хода игрока
    void beginBoosterChain(std::vector<QueuedBooster> initial, bool hasSwap, Cell swapA, Cell swapB);

    // Разбирает по одной волне бустеров
    void advanceBoosterChain();
    void updateBoosting(float dt);

    // ---- комбинации двух бустеров, свапнутых друг с другом ----
    // (BoosterCombo.cpp; типы — в BoosterCombo.h)

    // at — клетка второго клика
    std::optional<BoosterCombo> tryBoosterCombo(cfg::BoosterType a, cfg::BoosterType b, Cell at) const;

    Board*       board_;
    const Level* level_;  // игра не меняет настройки уровня
    Cell         selected_{};
    bool         hasSelection_ = false;

    Phase                 phase_ = Phase::Idle;
    float                 timer_ = 0.0f;  // сколько секунд идёт текущая фаза
    std::vector<Cell>     matches_;       // клетки, которые сейчас растворяются
    std::vector<ChipView> views_;

    std::vector<MatchGroup> matchGroups_;
    std::vector<Cell>       boosterTargets_;

    // Состояние цепочки срабатываний бустеров
    std::vector<QueuedBooster> boosterQueue_;
    std::vector<Cell>          chainTriggered_;
    std::vector<Cell>          chainBlast_;
    bool                       chainHasSwap_ = false;
    Cell                       chainSwapA_{};
    Cell                       chainSwapB_{};

    std::vector<BoosterFlight> flights_; // Полет бустера
    float                      boostTimer_ = 0.0f;

    // Откуда прилетела фишка, лежащая теперь в клетке i, — заполняется на
    // время перетасовки, чтобы знать, какой путь ей осталось пролететь.
    std::vector<Cell> shuffleFrom_;

    int collected_ = 0;  // собрано фишек целевого цвета
};

}  // namespace m3