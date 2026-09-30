#pragma once
#include <cstddef>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>
#include "Unit.h"

// Ошибка чтения файла данных или сохранения. Текст рассчитан на игрока:
// в нём есть имя файла и номер строки, а не только "parse error".
class DataError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Характеристики типа юнита. Для типов без стрельбы ranged и range равны 0.
struct UnitSpec {
    UnitKind kind = UnitKind::Light;
    int maxHp = 0;
    int melee = 0;
    int ranged = 0;
    int range = 0;
    int defense = 0;
    int cost = 0;
};

// Разбор строк общего вида "поле;поле;..." используют и каталог, и сохранения,
// чтобы правила чтения чисел и типов не жили в двух копиях.
namespace dataformat {

std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& line, char separator);
int parseInt(const std::string& field, const std::string& what);
UnitKind parseKind(const std::string& token);
const char* kindToken(UnitKind kind);

// Проверяет значения, которые не ловит разбор чисел: отрицательные, нулевое hp и т.п.
void validateSpec(const UnitSpec& spec);

// Читает поля "hp;с руки;выстрел;дальность;защита;цена" начиная с fields[first].
UnitSpec parseSpec(UnitKind kind, const std::vector<std::string>& fields, std::size_t first);

}  // namespace dataformat

class UnitCatalog {
public:
    static constexpr int SupportedVersion = 1;

    static UnitCatalog loadFromFile(const std::string& path);
    // source нужен только для текста ошибок
    static UnitCatalog loadFromStream(std::istream& in, const std::string& source);

    const std::vector<UnitSpec>& specs() const { return specs_; }

private:
    std::vector<UnitSpec> specs_;
};
