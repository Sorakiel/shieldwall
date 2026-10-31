#include "SfmlScene.h"
#include "EquipmentGlyphs.h"
#include <cmath>
#include <stdexcept>
namespace sfmlui {
namespace {
const sf::Color ink(15, 22, 27), panel(24, 33, 38), raised(33, 44, 49), border(62, 69, 66);
const sf::Color paper(233, 227, 207), muted(155, 166, 164), gold(206, 173, 108);
const sf::Color blue(107, 181, 225), red(233, 139, 128), green(132, 204, 160), violet(174, 153, 231);
constexpr float Pi = 3.14159265f;
sf::String u8(const std::string& s) {
    return sf::String::fromUtf8(s.begin(), s.end());
}
sf::Color alpha(sf::Color c, float a) {
    c.a = static_cast<std::uint8_t>(std::clamp(a, 0.f, 1.f) * 255);
    return c;
}
sf::Color teamColor(int team) {
    return team == 0 ? blue : red;
}
sf::Color typeColor(UnitKind k) {
    switch (k) {
    case UnitKind::Light:
        return sf::Color(137, 182, 168);
    case UnitKind::Heavy:
        return gold;
    case UnitKind::Archer:
        return sf::Color(201, 166, 113);
    case UnitKind::Mage:
        return violet;
    case UnitKind::Healer:
        return green;
    }
    return paper;
}
bool contains(const std::string& s, const std::string& p) {
    return s.find(p) != std::string::npos;
}
bool methodScreen(const State& s) {
    return s.recruitment.screen == RecruitmentScreen::Methods;
}
bool confirmation(const State& s) {
    return s.choices.size() == 2 &&
           (contains(s.choices[0].label, "Заменить") || contains(s.choices[0].label, "Перезаписать") ||
            s.choices[0].label == "Удалить сохранение");
}
std::string actionName(EventType t) {
    switch (t) {
    case EventType::Melee:
        return "Ближний удар";
    case EventType::Shot:
        return "Выстрел";
    case EventType::Heal:
        return "Исцеление";
    case EventType::Clone:
        return "Сотворение копии";
    case EventType::CloneFailed:
        return "Заклинание рассеялось";
    case EventType::Buff:
        return "Боевое снаряжение";
    case EventType::Death:
        return "Воин пал";
    case EventType::Promote:
        return "Строй смыкается";
    case EventType::BattleEnd:
        return "Сражение окончено";
    case EventType::Miss:
        return "Пропуск действия";
    case EventType::OutOfRange:
        return "Цель вне досягаемости";
    }
    return "Действие";
}
} // namespace
Scene::Scene(const std::filesystem::path& dir) {
    if (!body_.openFromFile(dir / "fonts/GolosText.ttf") ||
        !display_.openFromFile(dir / "fonts/CormorantGaramond.ttf"))
        throw std::runtime_error("Не удалось открыть шрифты интерфейса из " + dir.u8string());
}
void Scene::box(float x, float y, float w, float h, sf::Color fill, float radius, sf::Color stroke) {
    radius = std::min({radius, w / 2, h / 2});
    sf::ConvexShape shape(32);
    const sf::Vector2f centers[] = {{x + w - radius, y + radius},
                                    {x + w - radius, y + h - radius},
                                    {x + radius, y + h - radius},
                                    {x + radius, y + radius}};
    for (int corner = 0; corner < 4; ++corner)
        for (int i = 0; i < 8; ++i) {
            const float a = (-90.f + corner * 90.f + i * 90.f / 7.f) * Pi / 180;
            shape.setPoint(static_cast<std::size_t>(corner * 8 + i),
                           centers[corner] + sf::Vector2f(std::cos(a), std::sin(a)) * radius);
        }
    shape.setFillColor(fill);
    shape.setOutlineColor(stroke);
    shape.setOutlineThickness(-1);
    target_->draw(shape);
}
void Scene::line(sf::Vector2f a, sf::Vector2f b, sf::Color color, float thickness) {
    const auto d = b - a;
    sf::RectangleShape shape({std::sqrt(d.x * d.x + d.y * d.y), thickness});
    shape.setPosition(a);
    shape.setRotation(sf::radians(std::atan2(d.y, d.x)));
    shape.setFillColor(color);
    target_->draw(shape);
}
void Scene::disc(float x, float y, float radius, sf::Color color) {
    sf::CircleShape shape(radius, 40);
    shape.setPosition({x - radius, y - radius});
    shape.setFillColor(color);
    target_->draw(shape);
}
void Scene::polygon(std::initializer_list<sf::Vector2f> points, sf::Color color) {
    sf::ConvexShape shape(points.size());
    std::size_t i = 0;
    for (const auto p : points)
        shape.setPoint(i++, p);
    shape.setFillColor(color);
    target_->draw(shape);
}
float Scene::textWidth(const sf::String& value, unsigned size, bool display) const {
    float w = 0;
    sf::String run;
    auto measure = [&] {
        const sf::Text t(display ? display_ : body_, run, size);
        const auto b = t.getLocalBounds();
        return std::max(t.findCharacterPos(run.getSize()).x, b.position.x + b.size.x);
    };
    for (const auto p : value) {
        if (equipmentGlyph(p)) {
            w += measure() + size + 3.f;
            run.clear();
        } else
            run += p;
    }
    return w + measure();
}
void Scene::text(const sf::String& value, float x, float y, unsigned size, sf::Color color, bool display) {
    sf::String run;
    auto flush = [&] {
        sf::Text t(display ? display_ : body_, run, size);
        t.setPosition({x, y});
        t.setFillColor(color);
        target_->draw(t);
        if (!run.isEmpty()) {
            auto bounds = t.getGlobalBounds();
            if (bounds.size.x > 0 && bounds.size.y > 0)
                textBounds_.push_back(bounds);
        }
        x += textWidth(run, size, display);
        run.clear();
    };
    for (const auto p : value) {
        if (equipmentGlyph(p)) {
            flush();
            drawEquipmentGlyph(*target_, p, x, y + 3, static_cast<float>(size), color);
            x += size + 3.f;
        } else
            run += p;
    }
    flush();
}
void Scene::label(const std::string& v, float x, float y, unsigned size, sf::Color c, bool display) {
    text(u8(v), x, y, size, c, display);
}
sf::String Scene::fit(const std::string& value, float width, unsigned size, bool display) const {
    auto singleLine = value;
    for (auto& ch : singleLine)
        if (ch == '\n' || ch == '\r' || ch == '\t')
            ch = ' ';
    auto s = u8(singleLine);
    if (textWidth(s, size, display) <= width)
        return s;
    while (!s.isEmpty() && textWidth(s + u8("…"), size, display) > width)
        s.erase(s.getSize() - 1);
    return s + u8("…");
}
void Scene::paragraph(const std::string& value, float x, float y, float width, unsigned size, sf::Color color,
                      unsigned maxLines) {
    std::vector<sf::String> rows;
    sf::String row;
    for (auto ch : u8(value)) {
        if (ch == U'\n') {
            rows.push_back(row);
            row.clear();
            continue;
        }
        row += ch;
        if (textWidth(row, size) > width && row.getSize() > 1) {
            auto split = row.getSize() - 1;
            while (split > 0 && row[split] != U' ')
                --split;
            if (split == 0)
                split = row.getSize() - 1;
            rows.push_back(row.substring(0, split));
            row = row.substring(split + (row[split] == U' ' ? 1 : 0));
        }
    }
    if (!row.isEmpty())
        rows.push_back(row);
    const auto count =
        maxLines == 0 ? rows.size() : std::min(rows.size(), static_cast<std::size_t>(maxLines));
    for (std::size_t i = 0; i < count; ++i) {
        auto line = rows[i];
        if (i + 1 == count && count < rows.size()) {
            while (!line.isEmpty() && textWidth(line + u8("…"), size) > width)
                line.erase(line.getSize() - 1);
            line += u8("…");
        }
        text(line, x, y + static_cast<float>(i) * size * 1.55f, size, color);
    }
}
void Scene::button(const std::string& v, float x, float y, float w, int action, bool enabled, bool primary,
                   float h) {
    const sf::FloatRect bounds({x, y}, {w, h});
    const bool hover = enabled && bounds.contains(mouse_);
    box(x, y + 3, w, h, alpha(sf::Color::Black, .22f), 9);
    (void)primary;
    const auto fill = !enabled ? sf::Color(123, 110, 82) : hover ? sf::Color(229, 200, 142) : gold;
    box(x, y, w, h, fill, 9, gold);
    if (v == "×") {
        const float cx = x + w / 2, cy = y + h / 2;
        line({cx - 4, cy - 4}, {cx + 4, cy + 4}, ink, 1.5f);
        line({cx + 4, cy - 4}, {cx - 4, cy + 4}, ink, 1.5f);
    } else if (v == "<" || v == ">") {
        const float d = v == "<" ? -1.f : 1.f, cx = x + w / 2, cy = y + h / 2;
        const auto c = !enabled ? sf::Color(77, 70, 54) : ink;
        line({cx - d * 3, cy - 5}, {cx + d * 3, cy}, c, 1.5f);
        line({cx + d * 3, cy}, {cx - d * 3, cy + 5}, c, 1.5f);
    } else {
        const auto value = fit(v, w - 28, 15);
        text(value, x + (w - textWidth(value, 15)) / 2, y + (h - 23) / 2, 15, ink);
    }
    if (hover)
        line({x + 14, y + h - 2}, {x + w - 14, y + h - 2}, ink);
    buttons_.push_back({bounds, action, enabled});
}
void Scene::crest(float x, float y, float z, sf::Color c) {
    polygon({{x, y},
             {x + z * .48f, y - z * .13f},
             {x + z * .96f, y},
             {x + z * .86f, y + z * .62f},
             {x + z * .48f, y + z * .96f},
             {x + z * .10f, y + z * .62f}},
            c);
    polygon({{x + z * .04f, y + z * .04f},
             {x + z * .48f, y - z * .08f},
             {x + z * .92f, y + z * .04f},
             {x + z * .82f, y + z * .59f},
             {x + z * .48f, y + z * .89f},
             {x + z * .14f, y + z * .59f}},
            ink);
    line({x + z * .48f, y + z * .02f}, {x + z * .48f, y + z * .73f}, c, z * .025f);
    line({x + z * .25f, y + z * .24f}, {x + z * .71f, y + z * .24f}, c, z * .025f);
    polygon({{x + z * .35f, y + z * .42f},
             {x + z * .48f, y + z * .35f},
             {x + z * .61f, y + z * .42f},
             {x + z * .59f, y + z * .58f},
             {x + z * .48f, y + z * .70f},
             {x + z * .37f, y + z * .58f}},
            c);
}
void Scene::warrior(UnitKind kind, float x, float y, float z, sf::Color color, bool left, float opacity) {
    const auto c = alpha(color, opacity), dark = alpha(sf::Color(33, 43, 48), opacity),
               metal = alpha(sf::Color(204, 204, 188), opacity);
    const float d = left ? -1.f : 1.f;
    auto p = [&](float a, float b) { return sf::Vector2f(x + a * z * d, y + b * z); };
    polygon({p(-.28f, .36f), p(.22f, .34f), p(.38f, .91f), p(-.4f, .91f)}, alpha(color, .32f * opacity));
    line(p(-.12f, .65f), p(-.17f, .98f), dark, z * .11f);
    line(p(.12f, .65f), p(.2f, .98f), dark, z * .11f);
    polygon({p(-.23f, .31f), p(.2f, .31f), p(.25f, .7f), p(-.25f, .7f)}, c);
    line(p(-.19f, .36f), p(-.33f, .65f), metal, z * .07f);
    line(p(.2f, .36f), p(.34f, .55f), metal, z * .07f);
    disc(x, y + z * .19f, z * .125f, metal);
    box(x - z * .10f, y + z * .12f, z * .2f, z * .13f, dark, z * .03f);
    if (kind == UnitKind::Mage) {
        polygon({p(-.22f, .11f), p(.05f, -.19f), p(.20f, .11f)}, c);
        line(p(.37f, .18f), p(.37f, .96f), gold, z * .035f);
        disc(p(.37f, .14f).x, p(.37f, .14f).y, z * .08f, alpha(violet, opacity));
    } else if (kind == UnitKind::Healer) {
        disc(x, y + z * .17f, z * .17f, c);
        disc(x, y + z * .2f, z * .10f, dark);
        line(p(.4f, .21f), p(.4f, .96f), metal, z * .035f);
        line(p(.4f, 0), p(.4f, .21f), c, z * .06f);
        line(p(.29f, .08f), p(.51f, .08f), c, z * .06f);
        line(p(-.07f, .45f), p(.07f, .45f), paper, z * .035f);
        line(p(0, .38f), p(0, .52f), paper, z * .035f);
    } else if (kind == UnitKind::Archer) {
        line(p(.32f, .17f), p(.54f, .35f), gold, z * .035f);
        line(p(.54f, .35f), p(.54f, .62f), gold, z * .035f);
        line(p(.54f, .62f), p(.32f, .82f), gold, z * .035f);
        line(p(.32f, .17f), p(.32f, .82f), metal, z * .012f);
        line(p(.2f, .5f), p(.66f, .5f), metal, z * .02f);
    } else {
        line(p(.35f, .05f), p(.35f, .62f), metal, z * .04f);
        line(p(.24f, .5f), p(.47f, .5f), gold, z * .035f);
        if (kind == UnitKind::Heavy) {
            drawEquipmentGlyph(*target_, U'🛡', x - z * .49f, y + z * .42f, z * .44f, c);
            box(x - z * .17f, y + z * .06f, z * .34f, z * .22f, metal, z * .07f);
            line(p(-.12f, .16f), p(.12f, .16f), dark, z * .035f);
        } else
            drawEquipmentGlyph(*target_, U'🛡', x - z * .40f, y + z * .45f, z * .31f, c);
    }
    line(p(-.25f, .65f), p(.25f, .65f), gold, z * .035f);
}
void Scene::background(const State& s) {
    sf::VertexArray g(sf::PrimitiveType::TriangleStrip, 4);
    g[0] = {{0, 0}, sf::Color(29, 40, 44)};
    g[1] = {{Width, 0}, sf::Color(16, 26, 32)};
    g[2] = {{0, Height}, sf::Color(12, 18, 23)};
    g[3] = {{Width, Height}, sf::Color(21, 29, 32)};
    target_->draw(g);
    polygon({{480, 720}, {720, 315}, {930, 720}}, sf::Color(29, 43, 47));
    polygon({{740, 720}, {1040, 240}, {1330, 720}}, sf::Color(25, 38, 43));
    polygon({{1030, 740}, {1300, 380}, {1530, 740}}, sf::Color(24, 35, 40));
    for (int i = 0; i < 6; ++i) {
        const float x = 880.f + i * 65.f, h = i == 2 ? 180.f : i == 3 ? 150.f : 96.f;
        box(x, 650 - h, 45, h, sf::Color(18, 29, 34), 0);
        for (int j = 0; j < 3; ++j)
            box(x + j * 17.f, 640 - h, 11, 18, sf::Color(18, 29, 34), 0);
    }
    for (int i = 0; i < 34; ++i)
        disc(std::fmod(i * 127.3f + 25, Width), std::fmod(i * 91.1f - s.ambience * 5 + 9000, Height),
             i % 3 == 0 ? 1.4f : .7f, alpha(gold, .1f + (i % 4) * .035f));
    line({32, 96}, {1408, 96}, alpha(gold, .22f));
}
void Scene::header(const std::string& section, const std::string& subtitle) {
    crest(34, 28, 31, gold);
    label("SHIELDWALL", 80, 23, 27, paper, true);
    label("ТАКТИЧЕСКАЯ ДУЭЛЬ", 81, 58, 9, muted);
    label(section, 1090, 28, 23, gold, true);
    text(fit(subtitle, 315, 11), 1090, 59, 11, muted);
}
void Scene::notice(const State& s, float x, float y, float width) {
    if (s.notices.empty())
        return;
    const auto& v = s.notices.back();
    if (contains(v, "Shieldwall —") || contains(v, "Набор армии:") ||
        contains(v, "Выберите управление ходами") || contains(v, "Как игроки набирают"))
        return;
    box(x, y, width, 50, alpha(raised, .85f), 9, border);
    disc(x + 20, y + 24, 3, gold);
    text(fit(v, width - 55, 13), x + 35, y + 14, 13, muted);
}
void Scene::menu(const State& s) {
    header("Главное меню", "Соберите строй. Оставьте след.");
    label("СТРАТЕГИЯ В ОДНОМ СТРОЮ", 112, 162, 12, gold);
    label("SHIELDWALL", 106, 204, 88, paper, true);
    line({114, 322}, {400, 322}, gold, 2);
    label("Каждый воин.\nКаждый ход.", 112, 345, 44, paper, true);
    label("Две армии. Один исход. Ваше решение.", 115, 463, 16, muted);
    for (std::size_t i = 0; i < s.choices.size(); ++i) {
        const auto& c = s.choices[i];
        button(c.label, 114, 526.f + i * 70.f, 365, static_cast<int>(c.value), c.enabled, i == 0, 54);
    }
    disc(1008, 389, 207, alpha(gold, .025f));
    disc(1008, 389, 163, alpha(gold, .035f));
    crest(903, 257, 222, gold);
    line({820, 534}, {1195, 534}, alpha(gold, .35f));
    label("ДЕРЖИ СТРОЙ", 891, 563, 37, paper, true);
    label("Сила армии — в тех, кто стоит рядом.", 845, 617, 14, muted);
    notice(s, 114, 764, 1212);
    label("ЛОКАЛЬНАЯ ИГРА  /  ДВА ПОЛКОВОДЦА", 115, 845, 10, muted);
    label("Терпение. Тактика. Победа.", 1110, 839, 17, gold, true);
}

void Scene::settings(const State& s) {
    header("Новая партия", "Правила будущего сражения");
    const bool shared = std::any_of(s.choices.begin(), s.choices.end(),
                                    [](const MenuChoice& c) { return c.label == "Общий экран"; });
    const bool seed = contains(s.prompt, "Seed");
    label("ПОДГОТОВКА К ПОХОДУ", 104, 156, 12, gold);
    label(s.enteringNumber ? (seed ? "Жребий судьбы" : "Цена победы")
          : shared         ? "Два полководца"
                           : "Ритм сражения",
          100, 190, 58, paper, true);
    paragraph(
        s.enteringNumber
            ? (seed ? "Одинаковый seed — одинаковый исход. Сохраните число, чтобы повторить сражение."
                    : "Каждая армия получает одинаковый бюджет. Сделайте ставку на число воинов или на их "
                      "силу.")
        : shared
            ? "Набирайте армии открыто или передавайте управление друг другу, скрывая состав до начала боя."
            : "Изучайте каждое действие или наблюдайте за сражением в автоматическом режиме.",
        104, 279, 510, 17, muted);
    crest(1120, 196, 130, alpha(gold, .18f));
    if (s.enteringNumber && s.numberCancel != NumberCancel::Dialog) {
        box(752, 296, 584, 330, panel, 16, border);
        label(seed ? "Seed партии" : "Лимит каждой армии", 780, 320, 23, paper, true);
        box(780, 371, 528, 78, ink, 10, s.error.empty() ? gold : red);
        const auto value = fit(s.input, 476, 36);
        if (s.selectAll)
            box(795, 383, std::max(14.f, textWidth(value, 36) + 8), 49, alpha(gold, .25f), 4);
        text(value + (s.selectAll ? sf::String{} : sf::String("|")), 798, 383, 36, paper);
        paragraph(s.error.empty() ? (seed ? "От 0 до 4 294 967 295. Ноль — допустимый seed."
                                          : "Одинаковый бюджет для двух армий.")
                                  : s.error,
                  782, 472, 512, 14, s.error.empty() ? muted : red, 2);
        const bool back = s.numberCancel == NumberCancel::Previous;
        button("Продолжить", 780, 548, back ? 252 : 528, Submit, true, true, 50);
        if (back)
            button("Назад", 1044, 548, 264, static_cast<int>(uiAction::Back), true, false, 50);
        button("В главное меню", 104, 741, 260, 0);
    }
    if (!s.enteringNumber && !s.enteringText) {
        std::size_t i = 0;
        for (const auto& c : s.choices) {
            if (c.value == 0 || c.value == uiAction::Back) {
                button(c.label, c.value == 0 ? 104 : 390, 741, 260, static_cast<int>(c.value), c.enabled);
                continue;
            }
            const float x = 104.f + i * 622.f;
            box(x, 400, 590, 260, panel, 16, border);
            label(shared ? (i == 0 ? "I  ОТКРЫТЫЙ НАБОР" : "II  СКРЫТЫЙ НАБОР")
                         : (i == 0 ? "I  НАБЛЮДАЙТЕ" : "II  КОМАНДУЙТЕ"),
                  x + 30, 428, 12, gold);
            label(c.label, x + 30, 465, 37, paper, true);
            paragraph(shared ? (i == 0 ? "Оба состава доступны на одном экране."
                                       : "Отдельный ввод для каждого игрока.")
                             : (i == 0 ? "Ходы следуют друг за другом. Анимации показывают каждое действие."
                                       : "Следующий ход начинается только по вашей команде."),
                      x + 30, 523, 510, 15, muted);
            button("Выбрать", x + 30, 586, 530, static_cast<int>(c.value), c.enabled, i == 0);
            ++i;
        }
    }
}
void Scene::actions(const State& s, float x, float y, float width, int columns) {
    std::vector<MenuChoice> entries;
    for (const auto& c : s.choices)
        if (c.label.rfind("Купить:", 0) != 0 &&
            !(s.recruiting && s.recruitment.screen == RecruitmentScreen::Academy &&
              (c.value == 20 || c.value == 21)))
            entries.push_back(c);
    if (entries.empty())
        return;
    columns = std::min(columns, static_cast<int>(entries.size()));
    const float step = (width + 12) / columns;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& c = entries[i];
        button(c.label, x + (i % columns) * step, y + (i / columns) * 56.f, step - 12,
               static_cast<int>(c.value), c.enabled,
               c.label == "К бою" || c.label == "Армия готова" || (i == 0 && entries.size() < 4));
    }
}
void Scene::recruitment(const State& s) {
    const auto& r = s.recruitment;
    const auto color = teamColor(r.activeTeam);
    header("Набор армии", r.activeTeam == 0 ? "Полководец I · Синие" : "Полководец II · Красные");
    label("ПОД ВАШИМ ЗНАМЕНЕМ", 50, 120, 11, color);
    label(methodScreen(s)                            ? "Призовите свой отряд"
          : r.screen == RecruitmentScreen::Generated ? "Ваш отряд готов"
                                                     : "Военная академия",
          48, 145, 43, paper, true);
    box(1008, 120, 382, 65, panel, 12, border);
    label("БЮДЖЕТ", 1028, 132, 10, muted);
    {
        const auto number = std::to_string(r.limit);
        unsigned size = 23;
        while (size > 12 && textWidth(u8(number), size, true) > 100.f)
            --size;
        label(number, 1028, 150, size, paper, true);
    }
    label("ПОТРАЧЕНО", 1150, 132, 10, muted);
    {
        const auto number = std::to_string(r.spent);
        unsigned size = 23;
        while (size > 12 && textWidth(u8(number), size, true) > 100.f)
            --size;
        label(number, 1150, 150, size, paper, true);
    }
    label("ОСТАЛОСЬ", 1280, 132, 10, gold);
    {
        const auto number = std::to_string(r.remaining);
        unsigned size = 23;
        while (size > 12 && textWidth(u8(number), size, true) > 100.f)
            --size;
        label(number, 1280, 150, size, gold, true);
    }
    if (methodScreen(s)) {
        const std::string desc[] = {"Каждый новый строй —\nновая история сражения.",
                                    "Выберите каждого бойца\nи его место в строю.",
                                    "Плотный строй с упором\nна доступных бойцов."};
        int i = 0;
        for (const auto& c : s.choices) {
            if (c.value == 0 || c.value == 30 || c.value == uiAction::Back)
                continue;
            const float x = 128.f + i * 406.f;
            box(x, 246, 374, 285, panel, 16, border);
            warrior(i == 0   ? UnitKind::Archer
                    : i == 1 ? UnitKind::Heavy
                             : UnitKind::Light,
                    x + 305, 278, 65, gold);
            label("0" + std::to_string(i + 1), x + 28, 272, 17, gold);
            label(c.label, x + 28, 330, 31, paper, true);
            paragraph(desc[std::min(i, 2)], x + 28, 385, 310, 15, muted);
            button("Выбрать", x + 28, 457, 318, static_cast<int>(c.value), c.enabled, i == 1);
            ++i;
        }
        State footer = s;
        footer.choices.clear();
        for (const auto& c : s.choices)
            if (c.value == 0 || c.value == 30 || c.value == uiAction::Back)
                footer.choices.push_back(c);
        actions(footer, 50, 800, 1340, 3);
    } else if (r.screen == RecruitmentScreen::Generated) {
        box(50, 225, 1340, 295, panel, 16, border);
        label("Строй сформирован", 86, 254, 40, paper, true);
        paragraph("Проверьте состав перед подтверждением. Можно создать отряд снова, перейти в академию или "
                  "выбрать другой способ набора.",
                  88, 322, 720, 18, muted, 3);
        for (int i = 0; i < 3; ++i)
            warrior(i == 0   ? UnitKind::Light
                    : i == 1 ? UnitKind::Heavy
                             : UnitKind::Archer,
                    990.f + i * 130.f, 292, 105, gold);
        actions(s, 50, 785, 1340, 4);
    } else {
        for (std::size_t i = 0; i < r.offers.size(); ++i) {
            const auto& o = r.offers[i];
            const float step = 1340.f / std::max<std::size_t>(r.offers.size(), 1), x = 50.f + i * step,
                        w = step - 14;
            box(x, 214, w, 320, panel, 14, o.enabled ? border : alpha(border, .5f));
            label(unitKindName(o.kind), x + 18, 228, 29, paper, true);
            label(moneyCount(static_cast<std::size_t>(o.cost)), x + 18, 268, 12, gold);
            warrior(o.kind, x + w - 53, 290, 68, typeColor(o.kind), false, o.enabled ? 1.f : .4f);
            label("HP " + std::to_string(o.hp) + "   АТК " + std::to_string(o.melee) + "   ЗЩТ " +
                      std::to_string(o.defense),
                  x + 18, 363, 12, muted);
            auto desc = specialActionDescription(o.kind, o.ranged, o.range);
            std::replace(desc.begin(), desc.end(), '|', '\n');
            paragraph(desc, x + 18, 390, w - 36, 12, typeColor(o.kind), 4);
            const std::uint32_t id = o.kind == UnitKind::Light    ? 1
                                     : o.kind == UnitKind::Heavy  ? 2
                                     : o.kind == UnitKind::Archer ? 3
                                     : o.kind == UnitKind::Mage   ? 4
                                                                  : 5;
            const auto choice = std::find_if(s.choices.begin(), s.choices.end(), [&](const MenuChoice& c) {
                return c.value == id && c.label.rfind("Купить:", 0) == 0;
            });
            if (choice != s.choices.end())
                button("Нанять", x + 18, 473, w - 36, static_cast<int>(id), choice->enabled, false, 44);
        }
        actions(s, 50, 785, 1340, 5);
    }
    const auto& a = s.battle.armies[r.activeTeam];
    label("Ваш строй / " + unitCount(a.units.size()), 50, 550, 14, color);
    const auto offset =
        std::min(s.offsets[r.activeTeam], a.units.empty() ? 0 : ((a.units.size() - 1) / 8) * 8);
    const bool editable = r.screen == RecruitmentScreen::Academy;
    auto card = [&](const UnitSnapshot& unit, std::size_t index, float x, float y, bool ghost = false) {
        box(x, y + 3, 139, 140, alpha(sf::Color::Black, .2f), 11);
        box(x, y, 139, 140, ghost ? raised : panel, 11, ghost ? gold : border);
        label(std::to_string(index + 1), x + 12, y + 10, 11, color);
        warrior(unit.kind, x + 103, y + 38, 36, typeColor(unit.kind));
        const auto name = battletext::unitLabel(unit.name);
        text(fit(name.first, 111, 14), x + 12, y + 75, 14, paper);
        if (name.second.empty())
            label(unitKindName(unit.kind), x + 12, y + 105, 11, muted);
        else
            text(u8(name.second), x + 12, y + 105, 13, gold);
        if (editable && !ghost) {
            rosterCards_.push_back({{{x, y}, {139, 140}}, index});
            if (!s.rosterTransition)
                button("×", x + 106, y + 8, 24, RemoveBase + static_cast<int>(index), true, false, 24);
        }
    };
    if (a.units.empty()) {
        box(50, 584, 1340, 140, alpha(panel, .7f), 12, border);
        label("История вашего отряда начинается здесь", 82, 612, 30, paper, true);
        label("Выберите способ набора или наймите первого бойца в академии.", 84, 659, 15, muted);
    } else {
        for (std::size_t i = 0; i < 8 && offset + i < a.units.size(); ++i) {
            const auto index = offset + i;
            if (s.drag && s.drag->active && s.drag->from == index) {
                const float x = 50.f + i * 151.f;
                box(x, 584, 139, 140, alpha(raised, .35f), 11, alpha(gold, .5f));
                label("Перемещение", x + 12, 643, 11, muted);
                rosterCards_.push_back({{{x, 584}, {139, 140}}, index});
                continue;
            }
            float x = 50.f + i * 151.f, y = 584.f;
            if (s.rosterTransition) {
                const auto& t = *s.rosterTransition;
                const float smooth = t.progress * t.progress * (3 - 2 * t.progress);
                const auto old = previousArmyIndex(index, t.edit);
                if (t.edit.kind == ArmyEdit::Kind::Move && index == t.edit.to && t.drop) {
                    x = t.drop->x + (x - t.drop->x) * smooth;
                    y = t.drop->y + (y - t.drop->y) * smooth;
                } else if (old >= offset && old < offset + 8) {
                    const float before = 50.f + (old - offset) * 151.f;
                    x = before + (x - before) * smooth;
                }
            }
            card(a.units[index], index, x, y);
        }
        button("<", 1280, 596, 48, 200 + r.activeTeam * 2, offset > 0, false, 42);
        button(">", 1342, 596, 48, 201 + r.activeTeam * 2, offset + 8 < a.units.size(), false, 42);
        label("Страницы строя", 1280, 653, 10, muted);
    }
    if (s.drag && s.drag->active && s.drag->from < a.units.size()) {
        const auto& d = *s.drag;
        const auto nearest = std::min_element(rosterCards_.begin(), rosterCards_.end(),
                                              [&](const RosterCard& x, const RosterCard& y) {
                                                  return std::abs(d.pointer.x - x.bounds.position.x - 69) <
                                                         std::abs(d.pointer.x - y.bounds.position.x - 69);
                                              });
        if (nearest != rosterCards_.end())
            box(nearest->bounds.position.x - 4, 578, 3, 152, gold, 1);
        card(a.units[d.from], d.from, std::clamp(d.pointer.x - 69.f, 48.f, 1248.f),
             std::clamp(d.pointer.y - 35.f, 570.f, 620.f), true);
    }
    label(editable
              ? "Перетаскивайте карточки для перестановки. × — убрать бойца. Колесо мыши — страницы строя."
              : "Подтвердите состав кнопкой «Армия готова» или измените отряд.",
          50, 739, 12, muted);
    notice(s, 825, 724, 565);
}
sf::Vector2f Scene::slotCenter(presentation::Slot slot, const State& s) const {
    if (!slot.valid())
        return {550, 405};
    const int relative =
        std::clamp(slot.index - static_cast<int>(s.offsets[slot.team]), 0, static_cast<int>(PageUnits) - 1);
    return {slot.team == 0 ? 470.f - relative * 95.f : 618.f + relative * 95.f, 399.f};
}
void Scene::army(const State& s, int team) {
    const auto& a = s.battle.armies[team];
    const auto color = teamColor(team);
    const float left = team == 0 ? 52.f : 600.f;
    box(left, 170, 438, 66, alpha(color, .06f), 10, alpha(color, .23f));
    crest(left + 15, 187, 24, color);
    label(a.name.empty() ? (team == 0 ? "Синие" : "Красные") : a.name, left + 54, 178, 29, color, true);
    label(unitCount(a.units.size()) + " · " + moneyCount(static_cast<std::size_t>(a.totalCost)), left + 55,
          215, 11, muted);
    const auto offset = s.offsets[team];
    if (a.units.empty())
        label("Строй пал", left + 120, 418, 30, muted, true);
    for (auto i = offset; i < std::min(a.units.size(), offset + PageUnits); ++i) {
        const auto& u = a.units[i];
        const auto center = slotCenter({team, static_cast<int>(i)}, s);
        float dx = 0, opacity = u.hp > 0 ? 1.f : .2f;
        bool actor = false, target = false;
        if (s.effect) {
            const auto actorIndex = s.effect->actor.index +
                                    (s.effect->committed && s.effect->event.type == EventType::Clone ? 1 : 0);
            actor = s.effect->actor.team == team && actorIndex == static_cast<int>(i);
            target = s.effect->target.team == team && s.effect->target.index == static_cast<int>(i);
            const float p = s.effect->progress();
            if (actor && s.effect->event.type == EventType::Melee)
                dx = (team == 0 ? 1.f : -1.f) * std::sin(p * Pi) * 36;
            if (target &&
                (s.effect->event.type == EventType::Melee || s.effect->event.type == EventType::Shot) &&
                p > .5f)
                dx += std::sin((p - .5f) * Pi * 12) * 6 * (1 - p);
            if (actor && s.effect->event.type == EventType::Death)
                opacity = std::max(.1f, 1 - p);
            if (actor && s.effect->event.type == EventType::Promote && s.effect->fromIndex >= 0) {
                const auto from = slotCenter({team, s.effect->fromIndex}, s);
                dx += (from.x - center.x) * (1 - p) * (1 - p);
            }
        }
        const float x = center.x - 42 + dx;
        box(x, 297, 84, 264, actor ? sf::Color(44, 51, 51) : panel, 12,
            (actor || target) ? alpha(color, .8f) : alpha(border, .75f));
        box(x + 8, 315, 68, 3, typeColor(u.kind), 1);
        label(unitKindName(u.kind), x + 8, 331, 10, typeColor(u.kind));
        disc(x + 42, 288, 11, i == 0 ? color : raised);
        label(std::to_string(i + 1), x + 38, 280, 10, i == 0 ? ink : muted);
        warrior(u.kind, x + 42, 365, 65, typeColor(u.kind), team == 1, opacity);
        const auto name = battletext::unitLabel(u.name);
        text(fit(name.first, 70, 13), x + 7, 451, 13, paper);
        auto badges = u8(name.second);
        unsigned badgeSize = 13;
        while (badgeSize > 1 && textWidth(badges, badgeSize) > 70)
            --badgeSize;
        text(badges, x + 7, 477, badgeSize, gold);
        label(std::to_string(std::max(0, u.hp)) + " / " + std::to_string(u.maxHp), x + 8, 505, 10, muted);
        box(x + 8, 534, 68, 5, ink, 2);
        const float ratio = u.maxHp > 0 ? std::clamp(static_cast<float>(u.hp) / u.maxHp, 0.f, 1.f) : 0;
        if (ratio > 0)
            box(x + 8, 534, 68 * ratio, 5, ratio > .3f ? color : red, 2);
        if (sf::FloatRect({x, 297}, {84, 264}).contains(mouse_) && !s.playing) {
            const float tx = std::clamp(x - 40, 48.f, 760.f);
            box(tx, 577, 274, 68, raised, 9, gold);
            text(fit(battletext::icons(u.name), 250, 14), tx + 12, 586, 14, paper);
            label(std::string(unitKindName(u.kind)) + " · HP " + std::to_string(u.hp) + "/" +
                      std::to_string(u.maxHp),
                  tx + 12, 615, 12, muted);
        }
    }
    button("<", left, 660, 36, 200 + team * 2, offset > 0 && !s.playing, false, 32);
    button(">", left + 46, 660, 36, 201 + team * 2, offset + PageUnits < a.units.size() && !s.playing, false,
           32);
    label(a.units.empty() ? "Нет бойцов"
                          : "Позиции " + std::to_string(offset + 1) + "–" +
                                std::to_string(std::min(a.units.size(), offset + PageUnits)),
          left + 99, 669, 11, muted);
}
void Scene::field(const State& s) {
    header(s.stage == ViewStage::Result   ? "Итоги сражения"
           : s.stage == ViewStage::Battle ? "Ход " + std::to_string(s.turn)
                                          : "Армии готовы",
           s.stage == ViewStage::Battle
               ? (s.playing ? "Каждое действие имеет значение" : "Ваш следующий приказ")
               : "Два знамени. Один передний край.");
    box(32, 140, 1020, 596, alpha(panel, .7f), 18, alpha(border, .7f));
    for (int row = 0; row < 8; ++row)
        line({50, 300.f + row * 44.f}, {1034, 300.f + row * 44.f}, alpha(gold, .035f));
    line({546, 253}, {546, 624}, alpha(gold, .12f));
    label("VS", 530, 374, 27, alpha(gold, .55f), true);
    army(s, 0);
    army(s, 1);
    if (s.stage == ViewStage::Result) {
        label(s.winner == 0   ? "Победа Синих"
              : s.winner == 1 ? "Победа Красных"
                              : "Ничья",
              460, 24, 34,
              s.winner == 0   ? blue
              : s.winner == 1 ? red
                              : gold,
              true);
    } else
        label(s.stage == ViewStage::Battle ? "ПЕРЕДНИЙ КРАЙ" : "ПОСЛЕДНИЙ СМОТР ПЕРЕД БОЕМ",
              s.stage == ViewStage::Battle ? 469.f : 370.f, 251, 10, gold);
    if (s.effect)
        effects(s);
    journal(s);
    if (s.waiting || s.playing) {
        button(s.playing ? "Ход воспроизводится" : "Следующий ход", 32, 778, 254, Next, !s.playing, true, 52);
        button(s.automatic ? "Автобой: вкл." : "Автобой: выкл.", 298, 778, 228, Auto, true, false, 52);
        button("Сохранить", 538, 778, 192, Save, true, false, 52);
        button("В главное меню", 742, 778, 310, 0, true, false, 52);
        label("Enter / пробел — ход     A — режим     S — сохранить", 34, 851, 11, muted);
        button(s.speed == .5f   ? "Темп ×0.5"
               : s.speed == 1.f ? "Темп ×1"
                                : "Темп ×2",
               818, 105, 111, Speed, true, false, 28);
        if (s.playing)
            button(s.paused ? "Продолжить" : "Пауза", 941, 105, 111, Pause, true, false, 28);
    } else
        actions(s, 32, 778, 1020, 4);
}

void Scene::journal(const State& s) {
    box(1072, 140, 336, 690, ink, 16, border);
    label("Летопись боя", 1096, 164, 29, paper, true);
    label("СОБЫТИЯ ПО ПОРЯДКУ", 1096, 205, 9, gold);
    line({1096, 230}, {1384, 230}, alpha(gold, .25f));
    const auto end = static_cast<int>(s.log.lines.size()) - s.logOffset, first = std::max(0, end - LogLines);
    float y = 249;
    for (int i = first; i < end; ++i, y += 22) {
        const auto& row = s.log.lines[static_cast<std::size_t>(i)];
        text(sf::String(row.text), 1096, y, LogSize, row.team == 0 ? blue : row.team == 1 ? red : muted);
    }
    if (s.log.lines.empty()) {
        crest(1195, 366, 73, alpha(gold, .18f));
        paragraph("Сражение ещё не началось.\nПервый ход откроет летопись.", 1114, 492, 250, 14, muted);
    }
    if (static_cast<int>(s.log.lines.size()) > LogLines) {
        const float total = static_cast<float>(s.log.lines.size()),
                    thumb = std::max(18.f, 500 * LogLines / total);
        box(1394, 249, 2, 500, raised, 1);
        box(1394, 249 + (500 - thumb) * first / (total - LogLines), 2, thumb, gold, 1);
    }
    label(s.logOffset == 0 ? "Колесо мыши — история событий" : "Просмотр предыдущих событий", 1096, 793, 10,
          muted);
}
void Scene::effects(const State& s) {
    const auto& step = *s.effect;
    const auto& e = step.event;
    const float p = step.progress(), pulse = std::sin(p * Pi);
    const auto a = slotCenter(step.actor, s), b = slotCenter(step.target, s);
    const auto color = e.type == EventType::Heal                                        ? green
                       : e.type == EventType::Clone || e.type == EventType::CloneFailed ? violet
                       : e.type == EventType::Buff                                      ? gold
                       : e.team == 0                                                    ? blue
                                                                                        : red;
    if (e.type == EventType::Shot) {
        const float t = std::clamp((p - .10f) / .40f, 0.f, 1.f);
        const sf::Vector2f pos = a + (b - a) * t + sf::Vector2f(0, -std::sin(t * Pi) * 86);
        if (p > .10f && p < .50f) {
            const float d = e.team == 0 ? 1.f : -1.f;
            line(pos - sf::Vector2f(d * 25, 0), pos, paper, 2);
            polygon(
                {pos + sf::Vector2f(d * 6, 0), pos + sf::Vector2f(-d * 4, -4), pos + sf::Vector2f(-d * 4, 4)},
                gold);
            line(pos - sf::Vector2f(d * 40, 0), pos - sf::Vector2f(d * 24, 0), alpha(color, .3f), 2);
        }
    }
    if (e.type == EventType::Melee && p > .27f && p < .76f) {
        const auto center = (a + b) / 2.f;
        line(center + sf::Vector2f(-18, -40) * pulse, center + sf::Vector2f(18, 40) * pulse,
             alpha(paper, pulse), 4);
        line(center + sf::Vector2f(22, -26) * pulse, center + sf::Vector2f(-22, 26) * pulse,
             alpha(gold, pulse), 2);
    }
    if ((e.type == EventType::Melee || e.type == EventType::Shot) && p >= .5f && p < .85f) {
        const float fade = 1.f - (p - .5f) / .35f;
        for (int i = 0; i < 7; ++i) {
            const float angle = i * Pi * 2.f / 7.f;
            const auto ray = sf::Vector2f(std::cos(angle), std::sin(angle));
            line(b + ray * (10.f + 35.f * (1 - fade)), b + ray * (16.f + 45.f * (1 - fade)),
                 alpha(gold, fade), 2);
        }
    }
    const auto center = e.type == EventType::Death || e.type == EventType::Promote ? a : b;
    if (e.type == EventType::Heal || e.type == EventType::Clone || e.type == EventType::CloneFailed ||
        e.type == EventType::Buff) {
        sf::CircleShape ring(31 + 25 * p, 56);
        ring.setOrigin({31 + 25 * p, 31 + 25 * p});
        ring.setPosition(center);
        ring.setFillColor(sf::Color::Transparent);
        ring.setOutlineThickness(2);
        ring.setOutlineColor(alpha(color, pulse));
        target_->draw(ring);
        for (int i = 0; i < 8; ++i) {
            const float theta = i * Pi / 4 + p * 1.4f;
            const auto pos = center + sf::Vector2f(std::cos(theta), std::sin(theta)) * (28 + 24 * p);
            disc(pos.x, pos.y, 2.6f, alpha(color, pulse));
        }
        if (e.type == EventType::Heal) {
            line(center + sf::Vector2f(-15, -12), center + sf::Vector2f(15, -12), alpha(green, pulse), 5);
            line(center + sf::Vector2f(0, -27), center + sf::Vector2f(0, 3), alpha(green, pulse), 5);
        }
        if (e.type == EventType::Clone && p < .65f) {
            const auto own = slotCenter(step.actor, s);
            warrior(step.subjectKind, own.x + (e.team == 0 ? 48 : -48), 347, 65, violet, e.team == 1,
                    pulse * .6f);
            line(a, b, alpha(violet, pulse * .3f), 2);
        }
        if (e.type == EventType::Buff) {
            char32_t glyph = U'🛡';
            for (const auto& item : battletext::equipment)
                if (item.first == e.buff)
                    glyph = u8(item.second)[0];
            drawEquipmentGlyph(*target_, glyph, center.x - 16, center.y - 26, 32, alpha(gold, pulse));
        }
    }
    if (p >= .5f) {
        std::string floating;
        if (e.type == EventType::Melee || e.type == EventType::Shot)
            floating = "−" + std::to_string(e.damage);
        if (e.type == EventType::Heal)
            floating = "+" + std::to_string(e.restored);
        if (e.type == EventType::Clone)
            floating = "+1";
        if (e.type == EventType::Buff)
            floating = "Бонус";
        if (!floating.empty())
            label(floating, center.x - 16, center.y - 100 - (p - .5f) * 50, 27, alpha(color, (1 - p) * 2),
                  true);
    }
    box(262, 703, 560, 23, ink, 8, alpha(color, .25f));
    const auto caption = actionName(e.type) + (s.paused ? " · пауза" : "");
    const auto value = fit(caption, 536, 12);
    text(value, 542 - textWidth(value, 12) / 2, 705, 12, color);
}
void Scene::archive(const State& s) {
    const bool deleting = std::any_of(s.choices.begin(), s.choices.end(),
                                      [](const MenuChoice& c) { return c.label == "К загрузке"; });
    header("Сохранения", "Вернитесь к незавершённой истории");
    label(deleting ? "Управление летописями" : "Архив походов", 80, 126, 49, paper, true);
    label("Каждый строй и каждый ход можно продолжить.", 83, 190, 15, muted);
    std::size_t row = 0;
    State nav = s;
    nav.choices.clear();
    for (const auto& c : s.choices) {
        const auto split = c.label.find('\n');
        if (split == std::string::npos) {
            nav.choices.push_back(c);
            continue;
        }
        const float x = 80 + (row % 2) * 650.f, y = 247 + (row / 2) * 155.f;
        const bool hover = sf::FloatRect({x, y}, {630, 133}).contains(mouse_);
        box(x, y, 630, 133, hover ? raised : panel, 14, hover ? gold : border);
        crest(x + 20, y + 28, 38, c.enabled ? gold : muted);
        text(fit(c.label.substr(0, split), 478, 24, true), x + 82, y + 20, 24, c.enabled ? paper : muted,
             true);
        paragraph(c.label.substr(split + 1), x + 82, y + 67, 510, 12, muted, 3);
        if (c.enabled)
            label(deleting ? "×" : "→", x + 586, y + 19, 23, deleting ? red : gold);
        buttons_.push_back({{{x, y}, {630, 133}}, static_cast<int>(c.value), c.enabled});
        ++row;
    }
    actions(nav, 80, 787, 1280, 4);
    notice(s, 80, 722, 1280);
}
void Scene::modal(const State& s) {
    box(0, 0, Width, Height, alpha(sf::Color(6, 12, 16), .78f), 0);
    box(360, 234, 720, 406, sf::Color(26, 36, 40), 18, border);
    line({390, 235}, {1050, 235}, gold, 2);
    const bool entry = s.enteringNumber || s.enteringText;
    label(entry ? (s.enteringText                           ? "Сохранить летопись"
                   : s.numberCancel == NumberCancel::Dialog ? "Изменить строй"
                                                            : "Параметры похода")
                : "Подтвердите решение",
          396, 265, 38, paper, true);
    if (entry) {
        text(fit(s.prompt, 640, 16), 398, 330, 16, muted);
        box(396, 380, 648, 64, ink, 9, s.error.empty() ? gold : red);
        auto value = u8(s.input);
        while (!value.isEmpty() && textWidth(value, 26) > 596)
            value.erase(0);
        if (s.selectAll)
            box(411, 392, std::max(12.f, textWidth(value, 26) + 8), 38, alpha(gold, .25f), 3);
        text(value + (s.selectAll ? sf::String{} : sf::String("|")), 414, 393, 26, paper);
        paragraph(s.error.empty()
                      ? (s.enteringText ? (s.playing ? "Сохранится состояние после хода " +
                                                           std::to_string(std::max(0, s.turn - 1)) +
                                                           ". Текущий ход в сохранение не войдёт."
                                                     : "Пустое поле — предложенное имя. Enter — сохранить.")
                                        : "Введите целое число. Enter — продолжить.")
                      : s.error,
                  398, 461, 630, 13, s.error.empty() ? muted : red);
        button(s.enteringText ? "Сохранить" : "Продолжить", 396, 552, 316, Submit, true, true, 52);
        button(s.enteringText || s.numberCancel == NumberCancel::Dialog ? "Отмена" : "В главное меню", 728,
               552, 316, Cancel, true, false, 52);
    } else {
        if (!s.choices.empty() && contains(s.choices.front().label, "Заменить"))
            paragraph("Текущая партия будет заменена выбранным сохранением.", 398, 336, 630, 15, muted);
        else if (!s.notices.empty())
            paragraph(s.notices.back(), 398, 336, 630, 15, muted);
        for (std::size_t i = 0; i < s.choices.size(); ++i) {
            const auto& c = s.choices[i];
            button(c.label, 396 + i * 332.f, 552, 316, static_cast<int>(c.value), c.enabled, i == 0, 52);
        }
    }
}
bool Scene::saves(const State& s) const {
    return std::any_of(s.choices.begin(), s.choices.end(),
                       [](const MenuChoice& c) { return c.label.find('\n') != std::string::npos; });
}
void Scene::draw(sf::RenderTarget& target, const State& s, sf::Vector2f mouse) {
    target_ = &target;
    mouse_ = mouse;
    buttons_.clear();
    rosterCards_.clear();
    textBounds_.clear();
    background(s);
    if (s.handoff >= 0) {
        header("Передача управления", "Подготовка армий остаётся скрытой");
        crest(656, 205, 126, teamColor(s.handoff));
        label("Теперь ваш ход, полководец", 414, 388, 45, paper, true);
        label("Игрок " + std::to_string(s.handoff + 1), 642, 461, 28, teamColor(s.handoff), true);
        label("Предыдущий игрок должен отвернуться от экрана.", 502, 526, 16, muted);
        button("Я готов — продолжить", 430, 600, 420, Submit, true, true, 58);
        button("Назад", 868, 600, 200, static_cast<int>(uiAction::Back), true, false, 58);
        return;
    }
    const bool confirm = confirmation(s);
    if (saves(s))
        archive(s);
    else if (s.stage == ViewStage::MainMenu) {
        if (confirm) {
            State backdrop = s;
            backdrop.choices = {{1, "Новая партия"}, {2, "Загрузить"}, {0, "Выход"}};
            menu(backdrop);
        } else
            menu(s);
    } else if (s.stage == ViewStage::Setup)
        settings(s);
    else if (s.recruiting)
        recruitment(s);
    else
        field(s);
    if ((s.enteringNumber && !(s.stage == ViewStage::Setup && s.numberCancel != NumberCancel::Dialog)) ||
        s.enteringText || confirm) {
        buttons_.clear();
        modal(s);
    }
}
} // namespace sfmlui
