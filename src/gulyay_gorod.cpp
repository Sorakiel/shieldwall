#include <algorithm>

#include "gulyay_gorod.h"

namespace fort {
namespace {

// Сборка из готовых щитов на колёсах: дешевле бронированных возов.
constexpr int kInitialDurability = 850;  // 850 очков из 1000
constexpr int kArmor = 40;               // 40 % снижения урона
constexpr int kWood = 12;
constexpr int kLabour = 30;
constexpr int kSections = 8;  // щитовых секций в стене
constexpr int kShelter = 20;  // конных воинов за щитами

// Порог «серьёзного повреждения» — 10 % от предела прочности.
constexpr int kDamageThreshold = GulyayGorod::kMaxDurability / 10;

}  // namespace

GulyayGorod::GulyayGorod()
    : durability_(kInitialDurability),
      armor_(kArmor),
      wood_(kWood),
      labour_(kLabour) {}

GulyayGorod::~GulyayGorod() = default;
GulyayGorod::GulyayGorod(GulyayGorod&&) noexcept = default;
GulyayGorod& GulyayGorod::operator=(GulyayGorod&&) noexcept = default;

int GulyayGorod::durability() const { return durability_; }
int GulyayGorod::armorPercent() const { return armor_; }
int GulyayGorod::shieldSections() const { return kSections; }
int GulyayGorod::shelterCapacity() const { return kShelter; }
int GulyayGorod::woodCost() const { return wood_; }
int GulyayGorod::labourCost() const { return labour_; }
bool GulyayGorod::isAlive() const { return durability_ > 0; }

HitResult GulyayGorod::takeHit(int rawPower) {
    if (!isAlive()) return HitResult::Destroyed;
    if (rawPower <= 0) return HitResult::Blocked;

    // Броня щитов учитывается здесь, внутри укрепления.
    // Дробная часть отбрасывается: слабый удар не пробивает щит.
    const int passed = rawPower * (100 - armor_) / 100;
    if (passed <= 0) return HitResult::Blocked;

    const int before = durability_;
    durability_ = std::max(0, before - passed);

    if (durability_ == 0) return HitResult::Destroyed;
    if (before - durability_ < kDamageThreshold) return HitResult::Scratched;
    return HitResult::Damaged;
}

void GulyayGorod::repair(int points) {
    if (points <= 0) return;
    durability_ = std::min(kMaxDurability, durability_ + points);
}

}  // namespace fort
