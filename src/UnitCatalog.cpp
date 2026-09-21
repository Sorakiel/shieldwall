#include "UnitCatalog.h"
#include <algorithm>
#include <fstream>

namespace dataformat {

std::string trim(const std::string& s) {
    const char* spaces = " \t\r\n";
    std::size_t begin = s.find_first_not_of(spaces);
    if (begin == std::string::npos) {
        return "";
    }
    std::size_t end = s.find_last_not_of(spaces);
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> split(const std::string& line, char separator) {
    // Пустые поля сохраняем: "a;;b" - это три поля, иначе сдвинутся номера колонок
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (true) {
        std::size_t pos = line.find(separator, start);
        if (pos == std::string::npos) {
            fields.push_back(trim(line.substr(start)));
            return fields;
        }
        fields.push_back(trim(line.substr(start, pos - start)));
        start = pos + 1;
    }
}

int parseInt(const std::string& field, const std::string& what) {
    std::size_t used = 0;
    int value = 0;
    try {
        value = std::stoi(field, &used);
    } catch (const std::exception&) {
        throw DataError(what + ": «" + field + "» не целое число");
    }
    if (used != field.size()) {
        throw DataError(what + ": «" + field + "» не целое число");
    }
    return value;
}

UnitKind parseKind(const std::string& token) {
    if (token == "light") return UnitKind::Light;
    if (token == "heavy") return UnitKind::Heavy;
    if (token == "archer") return UnitKind::Archer;
    throw DataError("неизвестный тип юнита «" + token + "», допустимы light, heavy, archer");
}

const char* kindToken(UnitKind kind) {
    switch (kind) {
        case UnitKind::Light: return "light";
        case UnitKind::Heavy: return "heavy";
        case UnitKind::Archer: return "archer";
    }
    throw DataError("неизвестный тип юнита");
}

void validateSpec(const UnitSpec& spec) {
    if (spec.maxHp <= 0) throw DataError("hp должно быть больше нуля");
    if (spec.cost <= 0) throw DataError("цена должна быть больше нуля");
    if (spec.melee < 0 || spec.ranged < 0 || spec.range < 0 || spec.defense < 0) {
        throw DataError("атака, дальность и защита не могут быть отрицательными");
    }
    // Без этой проверки лучник с нулевой дальностью молча никогда не стреляет
    if (spec.kind == UnitKind::Archer) {
        if (spec.ranged == 0 || spec.range == 0) {
            throw DataError("у лучника выстрел и дальность должны быть больше нуля");
        }
    } else if (spec.ranged != 0 || spec.range != 0) {
        throw DataError("у юнита без стрельбы выстрел и дальность должны быть равны 0");
    }
}

UnitSpec parseSpec(UnitKind kind, const std::vector<std::string>& fields, std::size_t first) {
    const std::size_t count = 6;
    if (fields.size() < first + count) {
        throw DataError("ожидалось полей: " + std::to_string(first + count) +
                        ", найдено: " + std::to_string(fields.size()));
    }
    if (fields.size() > first + count) {
        throw DataError("лишние поля: ожидалось " + std::to_string(first + count) +
                        ", найдено " + std::to_string(fields.size()));
    }
    UnitSpec spec;
    spec.kind = kind;
    spec.maxHp = parseInt(fields[first], "hp");
    spec.melee = parseInt(fields[first + 1], "атака с руки");
    spec.ranged = parseInt(fields[first + 2], "выстрел");
    spec.range = parseInt(fields[first + 3], "дальность");
    spec.defense = parseInt(fields[first + 4], "защита");
    spec.cost = parseInt(fields[first + 5], "цена");
    validateSpec(spec);
    return spec;
}

}  // namespace dataformat

UnitCatalog UnitCatalog::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw DataError("не удалось открыть файл данных «" + path + "»");
    }
    return loadFromStream(in, path);
}

UnitCatalog UnitCatalog::loadFromStream(std::istream& in, const std::string& source) {
    UnitCatalog catalog;
    bool versionSeen = false;
    std::string line;
    int lineNo = 0;

    while (std::getline(in, line)) {
        ++lineNo;
        if (lineNo == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
            line.erase(0, 3);   // BOM, который любит добавлять блокнот Windows
        }
        std::string text = dataformat::trim(line);
        if (text.empty() || text[0] == '#') {
            continue;
        }

        auto fail = [&](const std::string& message) {
            return DataError(source + ", строка " + std::to_string(lineNo) + ": " + message);
        };

        try {
            std::vector<std::string> fields = dataformat::split(text, ';');
            if (!versionSeen) {
                if (fields[0] != "version" || fields.size() != 2) {
                    throw DataError("первой должна идти строка «version;N»");
                }
                int version = dataformat::parseInt(fields[1], "версия");
                if (version != SupportedVersion) {
                    throw DataError("версия формата " + std::to_string(version) +
                                    " не поддерживается, нужна " + std::to_string(SupportedVersion));
                }
                versionSeen = true;
                continue;
            }

            UnitKind kind = dataformat::parseKind(fields[0]);
            UnitSpec spec = dataformat::parseSpec(kind, fields, 1);
            bool duplicate = std::any_of(catalog.specs_.begin(), catalog.specs_.end(),
                                         [&](const UnitSpec& s) { return s.kind == kind; });
            if (duplicate) {
                throw DataError("тип «" + fields[0] + "» уже описан выше");
            }
            catalog.specs_.push_back(spec);
        } catch (const DataError& e) {
            throw fail(e.what());
        }
    }

    if (!versionSeen) {
        throw DataError(source + ": файл пуст, нет строки «version;N»");
    }
    if (catalog.specs_.empty()) {
        throw DataError(source + ": не описан ни один тип юнита");
    }
    return catalog;
}

const UnitSpec& UnitCatalog::spec(UnitKind kind) const {
    for (const UnitSpec& s : specs_) {
        if (s.kind == kind) {
            return s;
        }
    }
    throw DataError(std::string("в каталоге нет типа «") + dataformat::kindToken(kind) + "»");
}

int UnitCatalog::minCost() const {
    int best = 0;
    for (const UnitSpec& s : specs_) {
        if (best == 0 || s.cost < best) {
            best = s.cost;
        }
    }
    return best;
}
