#include "SaveService.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include "ArmyGenerator.h"

namespace {

const char* const Header = "shieldwall-save";

struct PhaseToken {
    SavePhase phase;
    const char* token;
};
const PhaseToken PhaseTokens[] = {
    {SavePhase::Recruitment, "recruitment"},
    {SavePhase::Battle, "battle"},
    {SavePhase::Result, "result"},
};

const char* phaseToken(SavePhase phase) {
    for (const PhaseToken& entry : PhaseTokens) {
        if (entry.phase == phase) return entry.token;
    }
    throw DataError("неизвестная фаза партии");
}

SavePhase parsePhase(const std::string& token) {
    for (const PhaseToken& entry : PhaseTokens) {
        if (token == entry.token) return entry.phase;
    }
    throw DataError("неизвестная фаза «" + token + "», допустимы recruitment, battle, result");
}

// Одни и те же проверки на записи и на чтении: битое сохранение не должно
// ни создаваться, ни открываться
void checkArmyFits(const std::vector<UnitRecord>& army, int limit, const char* title) {
    int spent = 0;
    for (const UnitRecord& r : army) {
        if (!ArmyGenerator::canAfford(spent, r.spec.cost, limit)) {
            throw DataError(std::string(title) + " не помещается в лимит " + std::to_string(limit));
        }
        spent += r.spec.cost;
    }
}

void validate(const SaveData& data) {
    if (data.costLimit <= 0) {
        throw DataError("лимит цены должен быть больше нуля");
    }
    if (data.turn < 0) {
        throw DataError("номер хода не может быть отрицательным");
    }
    if (data.phase == SavePhase::Recruitment && data.turn != 0) {
        throw DataError("в фазе закупки номер хода должен быть 0");
    }
    checkArmyFits(data.armyA, data.costLimit, "первая армия");
    checkArmyFits(data.armyB, data.costLimit, "вторая армия");
}

void checkName(const std::string& name) {
    if (name.empty() || name.find_first_of(";\r\n") != std::string::npos ||
        name != dataformat::trim(name)) {
        throw DataError("имя юнита «" + name + "» нельзя записать в сохранение");
    }
}

void writeRecords(std::ostream& out, int index, const std::vector<UnitRecord>& records) {
    out << "army;" << index << ";" << records.size() << "\n";
    for (const UnitRecord& r : records) {
        checkName(r.name);
        dataformat::validateSpec(r.spec);
        out << dataformat::kindToken(r.spec.kind) << ";" << r.name << ";" << r.spec.maxHp << ";"
            << r.spec.melee << ";" << r.spec.ranged << ";" << r.spec.range << ";"
            << r.spec.defense << ";" << r.spec.cost << "\n";
    }
}

std::vector<std::string> nextFields(dataformat::LineReader& reader, const std::string& expected) {
    std::string line;
    if (!reader.next(line)) {
        throw DataError("файл закончился, а ожидалось: " + expected);
    }
    return dataformat::split(line, ';');
}

std::uint32_t parseSeed(const std::string& field) {
    std::size_t used = 0;
    unsigned long long value = 0;
    try {
        value = std::stoull(field, &used);
    } catch (const std::exception&) {
        throw DataError("seed: «" + field + "» не целое число");
    }
    if (used != field.size() || field[0] == '-' || value > 0xFFFFFFFFull) {
        throw DataError("seed: «" + field + "» не помещается в 32 бита");
    }
    return static_cast<std::uint32_t>(value);
}

void expectFields(const std::vector<std::string>& fields, const std::string& key,
                  std::size_t count) {
    if (fields[0] != key || fields.size() != count) {
        throw DataError("ожидалась строка «" + key + "» из " + std::to_string(count) + " полей");
    }
}

std::vector<UnitRecord> readArmy(dataformat::LineReader& reader, int index) {
    std::string title = "army;" + std::to_string(index) + ";N";
    std::vector<std::string> head = nextFields(reader, title);
    expectFields(head, "army", 3);
    if (dataformat::parseInt(head[1], "номер армии") != index) {
        throw DataError("армии должны идти по порядку, ожидалась «" + title + "»");
    }
    int count = dataformat::parseInt(head[2], "размер армии");
    if (count < 0) {
        throw DataError("размер армии не может быть отрицательным");
    }

    std::vector<UnitRecord> records;
    for (int i = 0; i < count; ++i) {
        std::vector<std::string> fields = nextFields(reader, "юнит " + std::to_string(i + 1) +
                                                            " из " + std::to_string(count));
        if (fields.size() != 8) {
            throw DataError("у юнита должно быть 8 полей, найдено " +
                            std::to_string(fields.size()));
        }
        UnitRecord record;
        record.name = fields[1];
        if (record.name.empty()) {
            throw DataError("у юнита пустое имя");
        }
        record.spec = dataformat::parseSpec(dataformat::parseKind(fields[0]), fields, 2);
        records.push_back(record);
    }
    return records;
}

}  // namespace

std::vector<UnitRecord> SaveService::snapshot(Army& army) {
    std::vector<UnitRecord> records;
    for (std::size_t i = 0; i < army.size(); ++i) {
        const Unit* unit = army.at(i);
        UnitRecord record;
        record.name = unit->name();
        record.spec.kind = unit->kind();
        record.spec.maxHp = unit->maxHp();
        record.spec.melee = unit->meleeAttack();
        record.spec.defense = unit->defense();
        record.spec.cost = unit->cost();
        if (const Archer* archer = dynamic_cast<const Archer*>(unit)) {
            record.spec.ranged = archer->rangedAttack();
            record.spec.range = archer->range();
        }
        records.push_back(record);
    }
    return records;
}

void SaveService::write(const SaveData& data, std::ostream& out) {
    validate(data);
    out << Header << ";" << SupportedVersion << "\n";
    out << "phase;" << phaseToken(data.phase) << "\n";
    out << "limit;" << data.costLimit << "\n";
    out << "seed;" << data.seed << "\n";
    out << "turn;" << data.turn << "\n";
    writeRecords(out, 0, data.armyA);
    writeRecords(out, 1, data.armyB);
}

void SaveService::saveToFile(const SaveData& data, const std::string& path) {
    // Пишем сначала в строку: при битых данных не остаётся наполовину записанного файла
    std::ostringstream text;
    write(data, text);

    std::filesystem::path target(path);
    if (target.has_parent_path()) {
        std::filesystem::create_directories(target.parent_path());
    }
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        throw DataError("не удалось открыть файл сохранения «" + path + "» для записи");
    }
    out << text.str();
    if (!out) {
        throw DataError("не удалось записать файл сохранения «" + path + "»");
    }
}

SaveData SaveService::read(std::istream& in, const std::string& source) {
    dataformat::LineReader reader(in);
    SaveData data;
    try {
        std::vector<std::string> head = nextFields(reader, std::string(Header) + ";N");
        if (head[0] != Header || head.size() != 2) {
            throw DataError(std::string("это не сохранение, первой должна идти строка «") +
                            Header + ";N»");
        }
        int version = dataformat::parseInt(head[1], "версия");
        if (version != SupportedVersion) {
            throw DataError("версия сохранения " + std::to_string(version) +
                            " не поддерживается, нужна " + std::to_string(SupportedVersion));
        }

        std::vector<std::string> phase = nextFields(reader, "phase;...");
        expectFields(phase, "phase", 2);
        data.phase = parsePhase(phase[1]);

        std::vector<std::string> limit = nextFields(reader, "limit;N");
        expectFields(limit, "limit", 2);
        data.costLimit = dataformat::parseInt(limit[1], "лимит цены");

        std::vector<std::string> seed = nextFields(reader, "seed;N");
        expectFields(seed, "seed", 2);
        data.seed = parseSeed(seed[1]);

        std::vector<std::string> turn = nextFields(reader, "turn;N");
        expectFields(turn, "turn", 2);
        data.turn = dataformat::parseInt(turn[1], "номер хода");

        data.armyA = readArmy(reader, 0);
        data.armyB = readArmy(reader, 1);
        std::string extra;
        if (reader.next(extra)) {
            throw DataError("после второй армии есть лишние строки");
        }
        validate(data);
    } catch (const DataError& e) {
        throw DataError(source + ", строка " + std::to_string(reader.lineNo()) + ": " + e.what());
    }
    return data;
}

SaveData SaveService::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw DataError("не удалось открыть файл сохранения «" + path + "»");
    }
    return read(in, path);
}
