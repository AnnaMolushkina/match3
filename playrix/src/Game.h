#pragma once

#include <vector>

#include "Board.h"
#include "ChipView.h"
#include "Level.h"

namespace m3 {

// Один активированный бустер, ожидающий своей очереди в цепочке срабатываний
// (см. Game::beginBoosterChain/advanceBoosterChain). partner задан только для
// бустера(ов), активированных напрямую свапом, — рэйнбоу берёт из него цвет;
// у бустеров, найденных по цепочке, партнёра нет (как при активации кликом).
struct QueuedBooster {
    Cell             cell;
    cfg::BoosterType type       = cfg::BoosterType::None;
    bool             hasPartner = false;
    Cell             partner{};
};

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
    // Фазы поля. Falling наступает строго после Removing: по условию опадение
    // не должно начинаться, пока эффект удаления не отыграл до конца.
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

    void beginRemoving();
    void finishRemoving();
    void updateRemoving(float dt);

    // Общий шаг физики падения — двигает offsetY любой фишки, у которой он <
    // 0, к нулю. Вызывается из update() каждый кадр безусловно (не только в
    // Phase::Falling), поэтому фишки могут опадать и посреди Phase::Boosting.
    void stepGravity(float dt);
    // Проверяет, всё ли осело (физика уже сделана в stepGravity этим кадром);
    // если да — переходит к поиску каскада.
    void updateFalling();

    void beginShuffle();
    void updateShuffling(float dt);
    void restartLevel();

    // Немедленно снимает cells с поля (считает цель, если совпал цвет) и сразу
    // же запускает гравитацию — не дожидаясь ни чужого, ни своего полёта.
    // Так опадают соседи самолётика: они не часть отложенного chainBlast_,
    // видимого только в конце всей цепочки, а падают сию секунду.
    void resolveImmediate(const std::vector<Cell>& cells);

    // Клетка, в которой должен появиться бустер группы: клетка второго клика
    // (или первого — если группа не задела вторую), иначе — центр группы.
    static Cell pickBoosterCell(const MatchGroup& group, bool hasSwap, Cell swapA, Cell swapB);

    // Добавляет в out клетки, которые снесёт бустер типа type, лежащий в cell
    // (это его положение уже ПОСЛЕ свапа, если он был). immediate — подмножество
    // out, которое нужно спрятать сразу же, не дожидаясь ничьего полёта (у
    // самолётика — 4 соседа: они пропадают в момент активации, а не когда он
    // долетит до цели). flights — куда лететь для ракеты/самолётика. partner —
    // вторая клетка свапа, если бустер активирован свапом (nullptr при
    // активации двойным кликом либо цепочкой) — рэйнбоу берёт из неё цвет,
    // который нужно снести. Сам cell тоже добавляется в out: бустер снимается
    // вместе с тем, на что повлиял.
    void appendBoosterBlast(std::vector<Cell>& out, std::vector<Cell>& immediate,
                             std::vector<BoosterFlight>& flights, Cell cell, cfg::BoosterType type,
                             const Cell* partner) const;

    // Запускает цепочку срабатываний: initial — бустер(ы), активированные
    // напрямую кликом/свапом — это первая волна. Обрабатывается волнами (см.
    // advanceBoosterChain): следующая волна не начинает лететь, пока не
    // долетела (а если лёта не было ни у кого — не сработала) предыдущая.
    void beginBoosterChain(std::vector<QueuedBooster> initial, bool hasSwap, Cell swapA, Cell swapB);

    // Разбирает по одной волне бустеров. Волна — это то, что сейчас лежит в
    // boosterQueue_: либо initial из beginBoosterChain, либо всё, что нашлось
    // среди ОТЛОЖЕННЫХ клеток предыдущей волны (сиблинги — их могло найти
    // сразу несколько разных бустеров предыдущей волны, все они летят/
    // срабатывают одновременно, один не ждёт другого). Бустер, найденный
    // среди immediate-клеток текущего же бустера, ждать не должен вовсе —
    // присоединяется к ЭТОЙ волне прямо по ходу разбора. Внутри волны
    // бустеры без полёта (бомба, рэйнбоу) обрабатываются сразу же, синхронно;
    // у кого полёт есть (ракета, самолётик) — их полёты запускаются все разом
    // одним общим Phase::Boosting, и функция возвращает управление (остаток
    // разберёт updateBoosting, когда вся волна долетит). Если во всей волне
    // не нашлось ни одного полёта, но следующая волна не пуста — сразу
    // переходим к ней (никого не задерживаем, ждать нечего). Когда волны
    // кончились — весь накопленный снос уходит в startRemoving.
    void advanceBoosterChain();
    void updateBoosting(float dt);

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

    // Состояние цепочки срабатываний бустеров (см. beginBoosterChain/
    // advanceBoosterChain): кто ещё ждёт очереди, кто уже сработал (повторно
    // не активируется), что уже накоплено на снос, и контекст исходного свапа
    // для startRemoving в самом конце, когда цепочка полностью разберётся.
    std::vector<QueuedBooster> boosterQueue_;
    std::vector<Cell>          chainTriggered_;
    std::vector<Cell>          chainBlast_;
    bool                       chainHasSwap_ = false;
    Cell                       chainSwapA_{};
    Cell                       chainSwapB_{};

    // Полёт текущего шага цепочки (см. Phase::Boosting) — какой бустер сейчас
    // летит и насколько далеко.
    std::vector<BoosterFlight> flights_;
    float                      boostTimer_ = 0.0f;

    // Откуда прилетела фишка, лежащая теперь в клетке i, — заполняется на
    // время перетасовки, чтобы знать, какой путь ей осталось пролететь.
    std::vector<Cell> shuffleFrom_;

    int collected_ = 0;  // собрано фишек целевого цвета
};

}  // namespace m3
