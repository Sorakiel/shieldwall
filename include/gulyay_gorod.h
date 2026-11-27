#pragma once

namespace fort {

/// Чем закончился удар по укреплению.
enum class HitResult {
    Blocked,  ///< щиты выдержали, прочность не изменилась
    Scratched,  ///< потеряно меньше 10% прочности
    Damaged,    ///< серьёзное повреждение
    Destroyed  ///< укрепление разобрано, прочность = 0
};

/// GulyayGorod — подвижное укрепление: щиты размером со стену,
/// с прорезями для пищалей, поставленные на колёса или сани.
///
/// Сооружение самодостаточно: само хранит состояние, само учитывает броню
/// щитов, само сообщает, чем закончился удар. Наступательных характеристик
/// у него нет ни одной — крепость не нападает.
class GulyayGorod {
   public:
    /// Предел прочности в очках: 1000 — только что собранное укрепление.
    /// 1 очко = 1/1000 предела прочности.
    static constexpr int kMaxDurability = 1000;

    GulyayGorod();
    ~GulyayGorod();

    GulyayGorod(const GulyayGorod&) = delete;
    GulyayGorod& operator=(const GulyayGorod&) = delete;
    GulyayGorod(GulyayGorod&&) noexcept;
    GulyayGorod& operator=(GulyayGorod&&) noexcept;

    /// Текущая прочность в очках, 0..kMaxDurability.
    int durability() const;

    /// Броня щитов в процентах, 0..80. Дробная часть прошедшего урона
    /// отбрасывается: слабый удар может не пробить щит вовсе.
    int armorPercent() const;

    /// Число щитовых секций в стене.
    int shieldSections() const;

    /// Сколько конных воинов укрывается за щитами.
    /// Укрепление служит укрытием, а не оружием.
    int shelterCapacity() const;

    /// Стоимость сборки: брёвна.
    int woodCost() const;

    /// Стоимость сборки: человеко-дни.
    int labourCost() const;

    /// Укрепление ещё держится.
    bool isAlive() const;

    /// Нанести удар по укреплению.
    /// @param rawPower сила удара ДО учёта брони — броня применяется внутри.
    HitResult takeHit(int rawPower);

    /// Починить на @p points очков прочности, не выше kMaxDurability.
    void repair(int points);

   private:
    int durability_;  // очки прочности, 0..1000
    int armor_;       // проценты, 0..80
    int wood_;        // брёвна
    int labour_;      // человеко-дни
};

}  // namespace fort
