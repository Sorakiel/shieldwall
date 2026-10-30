#pragma once
#include <memory>

class Unit;

// Три интерфейса из задания: Unit, Healable, Clonable.
// Юнит, которого можно лечить, реализует IHealable; кого можно клонировать - IClonable.
// Кто не реализует интерфейс, того способность не затрагивает: маг не лечится
// и не клонируется.

class IHealable {
public:
    virtual ~IHealable() = default;
    // Возвращает, сколько hp реально восстановлено (не больше, чем не хватает до максимума)
    virtual int heal(int amount) = 0;
};

class IClonable {
public:
    virtual ~IClonable() = default;
    // Полная копия юнита в его текущем состоянии
    virtual std::unique_ptr<Unit> makeCopy() const = 0;
};
