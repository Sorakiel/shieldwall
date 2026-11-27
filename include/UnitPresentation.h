#pragma once
#include <cstdint>
#include <string>
#include "Unit.h"
#include "gulyay_gorod.h"

namespace unitui {
// Fortification parameters are fixed by the supplied class, not by the flat
// defense column in the catalog. Keep their presentation tied to that class.
inline const fort::GulyayGorod& fortificationDefaults() {
    static const fort::GulyayGorod fortification;
    return fortification;
}
inline bool fortified(UnitKind kind) { return kind == UnitKind::GulyayGorod; }
inline int startingHp(UnitKind kind, int maxHp) {
    return fortified(kind) ? fortificationDefaults().durability() : maxHp;
}
inline const char* healthName(UnitKind kind) { return fortified(kind) ? "Прочность" : "Здоровье"; }
inline const char* defenseName(UnitKind kind) { return fortified(kind) ? "Броня" : "Защита"; }
inline std::string defenseValue(UnitKind kind, int defense) {
    return fortified(kind) ? std::to_string(fortificationDefaults().armorPercent()) + "%"
                          : std::to_string(defense);
}
inline std::string attackValue(UnitKind kind, int attack) {
    return fortified(kind) ? "—" : std::to_string(attack);
}
inline std::string academyHealth(UnitKind kind, int maxHp) {
    const auto maximum = std::to_string(maxHp);
    return fortified(kind) ? std::to_string(startingHp(kind, maxHp)) + "/" + maximum : maximum;
}
// IDs are shared by the menu and the SFML buttons; catalog order may vary.
inline std::uint32_t buyAction(UnitKind kind) {
    switch (kind) {
    case UnitKind::Light: return 1;
    case UnitKind::Heavy: return 2;
    case UnitKind::Archer: return 3;
    case UnitKind::Mage: return 4;
    case UnitKind::Healer: return 5;
    case UnitKind::GulyayGorod: return 6;
    }
    return 0;
}
} // namespace unitui
