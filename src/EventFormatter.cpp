#include "EventFormatter.h"
#include <stdexcept>

const char* EventFormatter::teamName(int team) {
    switch (team) {
    case 0: return "Синие";
    case 1: return "Красные";
    default: return "Без команды";
    }
}

std::string EventFormatter::format(const BattleEvent& event, std::mt19937& rng) {
    // Остаток даёт одинаковые варианты фраз на разных стандартных библиотеках.
    const bool alternate = (rng() % 2) != 0;
    const std::string prefix = "[Ход " + std::to_string(event.turn) + "] [" +
                               teamName(event.team) + "] ";
    const std::string actor = "«" + event.actor + "»";
    const std::string target = "«" + event.target + "»";
    const std::string damage = " — " + std::to_string(event.damage) +
                               " урона, осталось " + std::to_string(event.hpLeft) + " hp.";

    switch (event.type) {
    case EventType::Melee:
        return prefix + actor + (alternate ? " наносит удар по " : " атакует в ближнем бою ") +
               target + damage;
    case EventType::Shot:
        return prefix + actor + (alternate ? " пускает стрелу в " : " поражает стрелой ") +
               target + damage;
    case EventType::Miss:
        return prefix + actor + (alternate ? " пропускает атаку." : " не наносит удара.");
    case EventType::OutOfRange:
        return prefix + actor + (alternate ? " не видит цели за спинами своих: не хватает дальности." :
                                             " не стреляет: цель вне дальности.");
    case EventType::Death:
        return prefix + actor + (alternate ? " падает замертво; место освободится в конце хода." :
                                             " погибает, но остаётся в строю до уборки.");
    case EventType::Promote:
        return prefix + (alternate ? "Поле расчищено: " : "После уборки ") + actor +
               (alternate ? " выходит в первый ряд." : " занимает первое место в строю.");
    case EventType::BattleEnd:
        return prefix + (alternate ? "Бой окончен. Победили " : "Победа за командой ") +
               teamName(event.team) + ".";
    }
    throw std::invalid_argument("Неизвестный тип события боя");
}
