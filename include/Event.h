#pragma once
#include <string>

enum class EventType {
    Melee, Shot, Miss, OutOfRange, Death, Promote, BattleEnd,
    Clone, CloneFailed, Heal, Buff
};

struct BattleEvent {
    EventType   type;
    int         turn = 0;
    int         team = 0;      // 0 или 1
    std::string actor;           // КОПИЯ имени, не указатель
    std::string target;
    int         damage = 0;
    int         hpLeft = 0;
    int         restored = 0;    // восстановлено hp (Heal)
    std::string buff;            // название выданного бонуса (Buff)
};
