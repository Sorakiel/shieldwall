#include "EventFormatter.h"
#include <stdexcept>

const char* EventFormatter::teamName(int team) {
    switch (team) {
    case 0: return "Синие";
    case 1: return "Красные";
    default: return "Без команды";
    }
}

std::string EventFormatter::format(const BattleEvent& event, std::mt19937& rng, bool withPrefix) {
    // Остаток даёт одинаковые варианты фраз на разных стандартных библиотеках.
    const bool alternate = (rng() % 2) != 0;
    const std::string prefix = !withPrefix ? "" : "[Ход " + std::to_string(event.turn) + "] [" +
        teamName(event.team) + "] ";
    const std::string actor = "«" + event.actor + "»";
    const std::string target = "«" + event.target + "»";
    const std::string damage = " — урон: " + std::to_string(event.damage) +
        ", здоровье цели: " + std::to_string(event.hpLeft) + ".";

    switch (event.type) {
    case EventType::Melee:
        return prefix + actor + (alternate ? " наносит удар по " : " атакует в ближнем бою ") +
            target + damage;
    case EventType::Shot:
        return prefix + actor + (alternate ? " стреляет в " : " атакует выстрелом ") +
            target + damage;
    case EventType::Miss:
        return prefix + actor + (alternate ? " пропускает атаку." : " не может атаковать в этом действии.");
    case EventType::OutOfRange:
        return prefix + actor + (alternate ? " не стреляет: недостаточно дальности." :
            " пропускает выстрел: не хватает дальности.");
    case EventType::Death:
        return prefix + actor + (alternate ? " погибает. Боец будет удалён из строя в конце хода." :
            " погибает и будет удалён из строя в конце хода.");
    case EventType::Promote:
        return prefix + actor + (alternate ? " становится первым бойцом в строю." :
            " занимает первую позицию в строю.");
    case EventType::BattleEnd:
        if (event.team == -2) return prefix + "Бой завершён. Ничья.";
        return prefix + (alternate ? "Бой завершён. Победили " : "Победила команда ") +
            teamName(event.team) + ".";
    case EventType::Clone:
        return prefix + actor + (alternate ? " создаёт копию бойца " : " клонирует бойца ") +
            target + ".";
    case EventType::CloneFailed:
        return prefix + actor + (alternate ? " не смог клонировать бойца " : " не смог создать копию бойца ") +
            target + ".";
    case EventType::Heal:
        return prefix + actor + (alternate ? " лечит " : " восстанавливает здоровье ") +
            target + " — восстановлено здоровья: " + std::to_string(event.restored) + ".";
    case EventType::Buff:
        return prefix + actor + (alternate ? " выдаёт бойцу " : " передаёт бойцу ") + target +
            " бонус: " + event.buff + ".";
    }
    throw std::invalid_argument("Неизвестный тип события боя");
}
