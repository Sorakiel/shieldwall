#include "SfmlScene.h"
#include "EquipmentGlyphs.h"
#include "UnitPresentation.h"
#include "CloneIdentity.h"
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
    case UnitKind::GulyayGorod:
        return sf::Color(158, 172, 183);
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
        return "Атака в ближнем бою";
    case EventType::Shot:
        return "Выстрел";
    case EventType::Heal:
        return "Лечение";
    case EventType::Clone:
        return "Клонирование";
    case EventType::CloneFailed:
        return "Клонирование не удалось";
    case EventType::Buff:
        return "Выдача снаряжения";
    case EventType::Death:
        return "Боец погиб";
    case EventType::Promote:
        return "Смена первого бойца";
    case EventType::BattleEnd:
        return "Бой завершён";
    case EventType::Miss:
        return "Атака пропущена";
    case EventType::OutOfRange:
        return "Недостаточно дальности";
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
            if (bounds.size.x > 0 && bounds.size.y > 0) {
                if (clip_) {
                    if (const auto visible = bounds.findIntersection(*clip_)) textBounds_.push_back(*visible);
                } else textBounds_.push_back(bounds);
            }
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
void Scene::callout(const std::string& value, sf::Vector2f center, sf::Color color, float opacity,
                    unsigned size, char32_t glyph) {
    if (value.empty() || opacity <= 0) return;
    sf::Text caption(body_, fit(value, 280, size), size);
    caption.setStyle(sf::Text::Bold);
    caption.setOutlineThickness(1.f);
    caption.setOutlineColor(alpha(sf::Color::Black, opacity));
    const auto bounds = caption.getLocalBounds();
    const float icon = glyph ? 34.f : 0.f, w = bounds.size.x + 28 + icon, h = size + 20.f;
    const float x = std::clamp(center.x - w / 2, 42.f, 1044.f - w);
    const float y = std::clamp(center.y - h / 2, 244.f, 676.f - h);
    box(x, y + 4, w, h, alpha(sf::Color::Black, opacity * .6f), 9);
    box(x, y, w, h, alpha(ink, opacity * .98f), 9, alpha(color, opacity));
    if (glyph) drawEquipmentGlyph(*target_, glyph, x + 10, y + (h - 28) / 2, 28, alpha(color, opacity));
    caption.setFillColor(alpha(paper, opacity));
    caption.setPosition({x + 14 + icon - bounds.position.x, y + (h - bounds.size.y) / 2 - bounds.position.y});
    target_->draw(caption);
    textBounds_.push_back(caption.getGlobalBounds());
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
    if (unitui::fortified(kind)) {
        // A wall of shields on wheels, with no human silhouette or weapon.
        polygon({p(-.48f, .15f), p(.48f, .15f), p(.48f, .76f), p(-.48f, .76f)}, dark);
        for (int i = 0; i < 4; ++i) {
            const float a = -.43f + i * .22f;
            polygon({p(a, .20f), p(a + .19f, .20f), p(a + .19f, .70f), p(a, .70f)}, c);
            line(p(a + .095f, .32f), p(a + .095f, .43f), dark, z * .035f);
            const auto rivet = p(a + .095f, .62f);
            disc(rivet.x, rivet.y, z * .022f, metal);
        }
        line(p(-.47f, .15f), p(.47f, .15f), metal, z * .045f);
        line(p(-.49f, .76f), p(.49f, .76f), metal, z * .06f);
        for (const float a : {-.3f, .3f}) {
            const auto wheel = p(a, .85f);
            disc(wheel.x, wheel.y, z * .13f, dark);
            disc(wheel.x, wheel.y, z * .09f, metal);
            disc(wheel.x, wheel.y, z * .035f, c);
        }
        return;
    }
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
void Scene::header() {
    crest(34, 28, 31, gold);
    label("SHIELDWALL", 80, 23, 27, paper, true);
    label("ТАКТИЧЕСКАЯ ДУЭЛЬ", 81, 58, 9, muted);
}
void Scene::notice(const State& s, float x, float y, float width) {
    if (s.notices.empty())
        return;
    const auto& v = s.notices.back();
    if (contains(v, "Shieldwall —") || contains(v, "Набор армии:") ||
        contains(v, "Управление ходами:") || contains(v, "Видимость армий при наборе:"))
        return;
    box(x, y, width, 50, alpha(raised, .85f), 9, border);
    disc(x + 20, y + 24, 3, gold);
    text(fit(v, width - 55, 13), x + 35, y + 14, 13, muted);
}
void Scene::menu(const State& s) {
    header();
    label("SHIELDWALL", 106, 204, 88, paper, true);
    for (std::size_t i = 0; i < s.choices.size(); ++i) {
        const auto& c = s.choices[i];
        button(c.label, 114, 345.f + i * 70.f, 365, static_cast<int>(c.value), c.enabled, i == 0, 54);
    }
    disc(1008, 389, 207, alpha(gold, .025f));
    disc(1008, 389, 163, alpha(gold, .035f));
    crest(903, 257, 222, gold);
    notice(s, 114, 764, 1212);
}

void Scene::settings(const State& s) {
    header();
    const bool shared = std::any_of(s.choices.begin(), s.choices.end(),
                                    [](const MenuChoice& c) { return c.label == "Открытый набор"; });
    const bool seed = contains(s.prompt, "Seed");
    label("НАСТРОЙКИ ПАРТИИ", 104, 156, 12, gold);
    label(s.enteringNumber ? (seed ? "Seed партии" : "Бюджет армий")
          : shared         ? "Видимость армий"
                           : "Управление ходами",
          100, 190, 58, paper, true);
    paragraph(
        s.enteringNumber
            ? (seed ? "Seed — число для генерации армий и случайных действий в бою. Одинаковые seed, "
                      "настройки и действия игроков дают одинаковый результат."
                    : "Укажите максимальную стоимость каждой армии. Общая цена нанятых бойцов "
                      "не может превышать этот лимит.")
        : shared
            ? "Выберите, показывать ли армии обоим игрокам во время набора или скрывать состав "
              "соперника до начала боя."
            : "Выберите, начинать ли ходы автоматически или запускать каждый ход кнопкой. "
              "Бойцы выбирают свои действия сами в обоих режимах.",
        104, 279, 640, 17, muted);
    crest(1120, 196, 130, alpha(gold, .18f));
    if (s.enteringNumber && s.numberCancel != NumberCancel::Dialog) {
        box(104, 392, 584, 286, panel, 16, border);
        label(seed ? "Seed партии" : "Лимит каждой армии", 132, 412, 23, paper, true);
        box(132, 458, 528, 64, ink, 10, s.error.empty() ? gold : red);
        const auto value = fit(s.input, 476, 32);
        if (s.selectAll)
            box(147, 467, std::max(14.f, textWidth(value, 32) + 8), 46, alpha(gold, .25f), 4);
        text(value + (s.selectAll ? sf::String{} : sf::String("|")), 150, 467, 32, paper);
        paragraph(s.error.empty() ? (seed ? "От 0 до 4 294 967 295. Ноль — допустимый seed."
                                          : "Одинаковый бюджет для двух армий.")
                                  : s.error,
                  134, 538, 512, 14, s.error.empty() ? muted : red, 2);
        button("Продолжить", 132, 610, FooterButtonWidth, Submit);
        button("Назад", 104, 741, FooterButtonWidth, static_cast<int>(uiAction::Back), true, false,
               FooterHeight);
        button("В главное меню", 104 + FooterButtonWidth + FooterGap, 741, FooterButtonWidth, 0, true, false,
               FooterHeight);
    }
    if (!s.enteringNumber && !s.enteringText) {
        std::size_t i = 0;
        for (const auto& c : s.choices) {
            if (c.value == 0 || c.value == uiAction::Back) {
                button(c.label, c.value == uiAction::Back ? 104 : 104 + FooterButtonWidth + FooterGap, 741,
                       FooterButtonWidth, static_cast<int>(c.value), c.enabled, false, FooterHeight);
                continue;
            }
            const float x = 104.f + i * 622.f;
            box(x, 400, 590, 260, panel, 16, border);
            label(shared ? (i == 0 ? "I  ОТКРЫТЫЙ НАБОР" : "II  СКРЫТЫЙ НАБОР")
                         : (i == 0 ? "I  БЕЗ ПОДТВЕРЖДЕНИЯ" : "II  ПО ПОДТВЕРЖДЕНИЮ"),
                  x + 30, 428, 12, gold);
            label(c.label, x + 30, 465, 37, paper, true);
            paragraph(shared ? (i == 0 ? "Оба игрока видят армии во время набора."
                                       : "Игроки набирают армии по очереди. Состав соперника скрыт до начала боя.")
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
        if (c.label.rfind("Купить:", 0) != 0 && !(s.recruiting && (c.value == 20 || c.value == 21)))
            entries.push_back(c);
    if (entries.empty())
        return;
    const float step = FooterButtonWidth + FooterGap;
    columns = std::max(1, std::min(columns, static_cast<int>((width + FooterGap) / step)));
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& c = entries[i];
        button(c.label, x + (i % columns) * step, y + (i / columns) * (FooterHeight + FooterGap),
               FooterButtonWidth, static_cast<int>(c.value), c.enabled, false, FooterHeight);
    }
}
void Scene::recruitment(const State& s) {
    const auto& r = s.recruitment;
    const auto color = teamColor(r.activeTeam);
    header();
    label(r.activeTeam == 0 ? "Синяя команда" : "Красная команда", 50, 120, 15, color);
    label(methodScreen(s) ? "Набор армии" : "Военная академия", 48, 145, 43, paper, true);
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
        const std::string desc[] = {"Случайный выбор бойцов\nв пределах бюджета.",
                                    "Выберите каждого бойца\nи его место в строю.",
                                    "Автоматический набор\nот дорогих бойцов к дешёвым."};
        int i = 0;
        for (const auto& c : s.choices) {
            if (c.value < 1 || c.value > 3)
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
            if (c.value == 0 || c.value == 30 || c.value == uiAction::ClearArmy || c.value == uiAction::Back)
                footer.choices.push_back(c);
        actions(footer, 50, 800, 1340, 4);
    } else {
        for (std::size_t i = 0; i < r.offers.size(); ++i) {
            const auto& o = r.offers[i];
            const float step = 1340.f / std::max<std::size_t>(r.offers.size(), 1), x = 50.f + i * step,
                        w = step - 14;
            box(x, 214, w, 320, panel, 14, o.enabled ? border : alpha(border, .5f));
            unsigned titleSize = 29;
            while (titleSize > 20 && textWidth(u8(unitKindName(o.kind)), titleSize, true) > w - 36)
                --titleSize;
            label(unitKindName(o.kind), x + 18, 228, titleSize, paper, true);
            label(moneyCount(static_cast<std::size_t>(o.cost)), x + 18, 268, 12, gold);
            warrior(o.kind, x + w - 44, unitui::fortified(o.kind) ? 264.f : 250.f,
                    unitui::fortified(o.kind) ? 34.f : 42.f, typeColor(o.kind), false,
                    o.enabled ? 1.f : .4f);
            box(x + 14, 304, w - 28, 82, raised, 8);
            const std::pair<const char*, std::string> stats[] = {
                {unitui::healthName(o.kind), unitui::academyHealth(o.kind, o.hp)},
                {"Атака", unitui::attackValue(o.kind, o.melee)},
                {unitui::defenseName(o.kind), unitui::defenseValue(o.kind, o.defense)}};
            for (int row = 0; row < 3; ++row) {
                const float y = 310.f + row * 24.f;
                label(stats[row].first, x + 24, y, 14, paper);
                const auto value = fit(stats[row].second, w - 130, 16);
                text(value, x + w - 24 - textWidth(value, 16), y - 1, 16, gold);
            }
            label("СПОСОБНОСТЬ", x + 18, 400, 10, muted);
            auto desc = specialActionDescription(o.kind, o.ranged, o.range);
            std::replace(desc.begin(), desc.end(), '|', '\n');
            paragraph(desc, x + 18, 418, w - 36, r.offers.size() > 5 ? 10 : 11, typeColor(o.kind),
                      r.offers.size() > 5 ? 4 : 3);
            const auto id = unitui::buyAction(o.kind);
            const auto choice = std::find_if(s.choices.begin(), s.choices.end(), [&](const MenuChoice& c) {
                return c.value == id && c.label.rfind("Купить:", 0) == 0;
            });
            if (choice != s.choices.end())
                button(unitui::fortified(o.kind) ? "Построить" : "Нанять", x + 18, 482, w - 36,
                       static_cast<int>(id), choice->enabled, false, 40);
        }
        actions(s, 50, 800, 1340, 4);
    }
    const auto& a = s.battle.armies[r.activeTeam];
    label("Состав армии / " + unitCount(a.units.size()), 50, 550, 14, color);
    const auto offset =
        std::min(s.offsets[r.activeTeam], a.units.empty() ? 0 : ((a.units.size() - 1) / 8) * 8);
    auto card = [&](const UnitSnapshot& unit, std::size_t index, float x, float y, bool ghost = false) {
        box(x, y + 3, 139, 140, alpha(sf::Color::Black, .2f), 11);
        box(x, y, 139, 140, ghost ? raised : panel, 11, ghost ? gold : border);
        label(std::to_string(index + 1), x + 12, y + 10, 11, color);
        warrior(unit.kind, x + 103, y + 38, 36, typeColor(unit.kind));
        const auto identity = cloneidentity::parse(unit.name);
        const auto name = battletext::unitLabel(identity.base);
        text(fit(name.first, 111, 14), x + 12, y + 75, 14, paper);
        if (identity.number > 0)
            text(fit("Клон " + std::to_string(identity.number), 111, 11), x + 12, y + 105, 11, violet);
        else if (name.second.empty())
            label(unitKindName(unit.kind), x + 12, y + 105, 11, muted);
        else
            text(u8(name.second), x + 12, y + 105, 13, gold);
        if (!ghost) {
            rosterCards_.push_back({{{x, y}, {139, 140}}, index});
            if (!s.rosterTransition)
                button("×", x + 106, y + 8, 24, RemoveBase + static_cast<int>(index), true, false, 24);
        }
    };
    if (a.units.empty()) {
        box(50, 584, 1340, 140, alpha(panel, .7f), 12, border);
        label("В армии пока нет бойцов", 82, 612, 30, paper, true);
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
        label("Другие бойцы", 1280, 653, 10, muted);
    }
    if (s.drag && s.drag->active && s.drag->from < a.units.size()) {
        const auto& d = *s.drag;
        if (const auto target = dropTarget(d); target && *target != d.from) {
            const auto anchor = std::find_if(rosterCards_.begin(), rosterCards_.end(),
                                             [&](const RosterCard& c) { return c.index == *target; });
            const float marker =
                anchor != rosterCards_.end()
                    ? anchor->bounds.position.x + (*target > d.from ? RosterCardSize.x + 4 : -4)
                : *target > rosterCards_.back().index
                    ? rosterCards_.back().bounds.position.x + RosterCardSize.x + 4
                    : rosterCards_.front().bounds.position.x - 4;
            box(marker, 578, 3, 152, gold, 1);
        }
        const auto position = dragPosition(d);
        card(a.units[d.from], d.from, position.x, position.y, true);
    }
    if (s.drag && s.drag->active) {
        label("Удерживайте карточку у края, чтобы перейти на другую страницу. Колесо мыши тоже работает.",
              50, 739, 12, gold);
        const auto center = dragPosition(*s.drag) + RosterCardSize / 2.f;
        const int direction = presentation::dragPageDirection(center.x, center.y);
        if ((direction < 0 && offset > 0) || (direction > 0 && offset + 8 < a.units.size())) {
            const float x = direction < 0 ? 44.f : 1252.f;
            const float brightness = .6f + .4f * std::sin(s.ambience * 6);
            line({x, 584}, {x, 724}, alpha(gold, brightness), 4);
        }
    } else
        label("Перетаскивайте бойцов, чтобы изменить порядок. × — удалить бойца. Колесо мыши — прокрутить состав.",
              50, 739, 12, muted);
    notice(s, 825, 724, 565);
}
std::optional<std::size_t> Scene::dropTarget(const DragState& drag) const {
    const auto center = dragPosition(drag) + RosterCardSize / 2.f;
    if (!sf::FloatRect({48, 570}, {1210, 174}).contains(center))
        return std::nullopt;
    std::vector<ArmyCardSlot> slots;
    for (const auto& card : rosterCards_)
        slots.push_back({card.index, card.bounds.position.x + card.bounds.size.x / 2});
    return armyDropIndex(drag.from, center.x, slots);
}
sf::Vector2f Scene::slotCenter(presentation::Slot slot, const State& s) const {
    if (!slot.valid())
        return {550, 405};
    const float relative = static_cast<float>(slot.index) - displayedOffset(s, slot.team);
    return {slot.team == 0 ? 470.f - relative * 95.f : 618.f + relative * 95.f, 399.f};
}
void Scene::army(const State& s, int team) {
    const auto& a = s.battle.armies[team];
    const auto color = teamColor(team);
    const float left = team == 0 ? 52.f : 600.f;
    box(left, 170, 438, 66, alpha(color, .06f), 10, alpha(color, .23f));
    crest(left + 15, 187, 24, color);
    label(a.name.empty() ? (team == 0 ? "Синие" : "Красные") : a.name, left + 54, 178, 29, color, true);
    const bool knownCost = std::all_of(a.units.begin(), a.units.end(),
                                       [](const UnitSnapshot& unit) { return unit.detailsKnown; });
    label(unitCount(a.units.size()) + " · " + (knownCost ? moneyCount(static_cast<std::size_t>(a.totalCost))
                                                        : "стоимость —"), left + 55, 215, 11, muted);
    const auto offset = s.offsets[team];
    if (a.units.empty())
        label("Нет живых бойцов", left + 120, 418, 30, muted, true);
    const bool shifting = s.effect && s.effect->event.type == EventType::Clone &&
                          s.effect->actor.team == team && s.effect->progress() >= .2f &&
                          s.effect->progress() < .72f;
    const bool moving = s.scrolls[team].has_value() || shifting;
    const auto savedView = target_->getView();
    if (moving) {
        const sf::FloatRect region({team == 0 ? 44.f : 572.f, 272.f}, {476.f, 294.f});
        const auto size = savedView.getSize(), origin = savedView.getCenter() - size / 2.f;
        const auto viewport = savedView.getViewport();
        sf::View clipped(region);
        clipped.setViewport({{viewport.position.x + (region.position.x - origin.x) / size.x * viewport.size.x,
                              viewport.position.y + (region.position.y - origin.y) / size.y * viewport.size.y},
                             {region.size.x / size.x * viewport.size.x,
                              region.size.y / size.y * viewport.size.y}});
        target_->setView(clipped);
        clip_ = region;
    }
    const auto display = displayedOffset(s, team);
    const auto first = moving ? static_cast<std::size_t>(std::max(0.f, std::floor(display) - 1)) : offset;
    const auto last = moving ? static_cast<std::size_t>(std::ceil(display)) + PageUnits + 1 : offset + PageUnits;
    for (auto i = first; i < std::min(a.units.size(), last); ++i) {
        const auto& u = a.units[i];
        const auto center = slotCenter({team, static_cast<int>(i)}, s);
        float dx = 0, opacity = !u.detailsKnown || u.hp > 0 ? 1.f : .2f;
        bool actor = false, target = false;
        if (s.effect) {
            const auto acting = presentation::actorSlot(*s.effect), affected = presentation::targetSlot(*s.effect);
            actor = acting.team == team && acting.index == static_cast<int>(i);
            target = affected.team == team && affected.index == static_cast<int>(i);
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
        if (shifting && static_cast<int>(i) >= s.effect->actor.index) {
            if (!s.effect->committed)
                dx += (team == 0 ? -95.f : 95.f) * presentation::smooth((s.effect->progress() - .2f) / .3f);
            else if (static_cast<int>(i) == s.effect->actor.index)
                opacity *= .75f + .25f * presentation::smooth((s.effect->progress() - .5f) / .2f);
        }
        const float x = center.x - 42 + dx;
        const auto accent = s.effect && (s.effect->event.type == EventType::Clone) ? violet
                          : s.effect && s.effect->event.type == EventType::Heal ? green
                          : s.effect && s.effect->event.type == EventType::Buff ? gold : color;
        box(x, 297, 84, 264, target ? sf::Color(39, 49, 51) : actor ? sf::Color(44, 51, 51) : panel, 12,
            (actor || target) ? accent : alpha(border, .75f));
        if (target || actor) {
            line({x + 8, 307}, {x + 76, 307}, accent, 3);
            line({x + 8, 553}, {x + 76, 553}, accent, 3);
        }
        box(x + 8, 315, 68, 3, typeColor(u.kind), 1);
        text(fit(u.detailsKnown ? unitKindName(u.kind) : "Клон", 68, unitui::fortified(u.kind) ? 9 : 10), x + 8, 331,
             unitui::fortified(u.kind) ? 9 : 10, typeColor(u.kind));
        disc(x + 42, 288, 11, i == 0 ? color : raised);
        label(std::to_string(i + 1), x + 38, 280, 10, i == 0 ? ink : muted);
        if (u.detailsKnown) warrior(u.kind, x + 42, 351, 58, typeColor(u.kind), team == 1, opacity);
        else {
            box(x + 21, 359, 34, 43, raised, 5, violet);
            box(x + 29, 368, 34, 43, panel, 5, violet);
            label("?", x + 39, 374, 23, violet);
        }
        const auto identity = cloneidentity::parse(u.name);
        const auto name = battletext::unitLabel(identity.base);
        text(fit(name.first, 70, 13), x + 7, 421, 13, paper);
        auto badges = u8(name.second);
        unsigned badgeSize = 13;
        while (badgeSize > 1 && textWidth(badges, badgeSize) > 70)
            --badgeSize;
        if (identity.number > 0) {
            const auto mark = "Клон " + std::to_string(identity.number);
            unsigned size = 10;
            while (size > 4 && textWidth(u8(mark), size) > 68) --size;
            box(x + 7, 441, 70, 16, ink, 4, alpha(violet, .7f));
            label(mark, x + 9, 442, size, violet);
        } else text(badges, x + 7, 442, badgeSize, gold);
        const auto statColor =
            target && s.effect && s.effect->event.type == EventType::Buff && s.effect->committed ? gold
                                                                                                 : paper;
        text(fit("Атака " + (u.detailsKnown ? unitui::attackValue(u.kind, u.attack) : "—"), 68, 11), x + 8, 462, 11, statColor);
        text(fit(std::string(unitui::defenseName(u.kind)) + " " + (u.detailsKnown ? unitui::defenseValue(u.kind, u.defense) : "—"), 68, 11), x + 8, 480, 11, statColor);
        if (u.kind == UnitKind::Archer)
            text(fit("Выстрел " + std::to_string(u.rangedAttack), 68, 10), x + 8, 500, 10, typeColor(u.kind));
        else if (unitui::fortified(u.kind))
            label("Прочность", x + 8, 500, 10, muted);
        label(u.detailsKnown ? std::to_string(std::max(0, u.hp)) + " / " + std::to_string(u.maxHp) : "?",
              x + 8, 522, 10, muted);
        box(x + 8, 542, 68, 5, ink, 2);
        const float ratio = u.maxHp > 0 ? std::clamp(static_cast<float>(u.hp) / u.maxHp, 0.f, 1.f) : 0;
        if (ratio > 0)
            box(x + 8, 542, 68 * ratio, 5, ratio > .3f ? color : red, 2);
        if (sf::FloatRect({x, 297}, {84, 264}).contains(mouse_) && !s.playing && !moving && u.detailsKnown) {
            const float tx = std::clamp(x - 40, 48.f, 760.f);
            box(tx, 573, 312, 79, raised, 9, gold);
            text(fit(battletext::icons(u.name), 250, 14), tx + 12, 586, 14, paper);
            text(fit(std::string(unitui::healthName(u.kind)) + " " + std::to_string(u.hp) + "/" + std::to_string(u.maxHp), 288, 12), tx + 12,
                 608, 12, muted);
            text(fit("Атака " + unitui::attackValue(u.kind, u.attack) + " · " +
                         unitui::defenseName(u.kind) + " " + unitui::defenseValue(u.kind, u.defense) +
                         (u.kind == UnitKind::Archer ? " · Выстрел " + std::to_string(u.rangedAttack) : ""),
                     288, 12),
                 tx + 12, 628, 12, paper);
        }
    }
    if (moving) { target_->setView(savedView); clip_.reset(); }
    button("<", left, 660, 36, 200 + team * 2, offset > 0 && !s.playing, false, 32);
    button(">", left + 46, 660, 36, 201 + team * 2, offset + PageUnits < a.units.size() && !s.playing, false,
           32);
    label(a.units.empty() ? "Нет бойцов"
                          : "Позиции " + std::to_string(offset + 1) + "–" +
                                std::to_string(std::min(a.units.size(), offset + PageUnits)),
          left + 99, 669, 11, muted);
}
void Scene::field(const State& s) {
    header();
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
        label(s.stage == ViewStage::Battle ? "БОЕВОЙ СТРОЙ" : "АРМИИ ГОТОВЫ К БОЮ",
              s.stage == ViewStage::Battle ? 469.f : 370.f, 251, 10, gold);
    if (s.effect && !scrolling(s))
        effects(s);
    journal(s);
    if (s.waiting || s.playing) {
        const float step = FooterButtonWidth + FooterGap;
        button(s.playing ? "Ход воспроизводится" : "Следующий ход", 32, 778, FooterButtonWidth, Next,
               !s.playing, false, FooterHeight);
        button(s.automatic ? "Автобой: вкл." : "Автобой: выкл.", 32 + step, 778, FooterButtonWidth, Auto,
               true, false, FooterHeight);
        button("Сохранить", 32 + 2 * step, 778, FooterButtonWidth, Save, true, false, FooterHeight);
        button("В главное меню", 32 + 3 * step, 778, FooterButtonWidth, 0, true, false, FooterHeight);
        label("Enter / пробел — следующий ход     A — автобой     S — сохранить", 34, 851, 11, muted);
        label("Скорость", 575, 109, 12, muted);
        const int speedActions[] = {SpeedHalf, Speed, Speed2, Speed4};
        const std::string speedNames[] = {"0.5×", "1×", "2×", "4×"};
        for (std::size_t i = 0; i < presentation::Speeds.size(); ++i) {
            const float x = 650.f + i * 68.f;
            const bool selected = s.speed == presentation::Speeds[i];
            if (selected) {
                box(x - 4, 101, 70, 36, alpha(gold, .12f), 12);
                box(x - 2, 103, 66, 32, alpha(gold, .25f), 10);
            }
            button(speedNames[i], x, 105, 62, speedActions[i], true, false, 28);
            if (selected) {
                box(x, 105, 62, 28, sf::Color(35, 43, 46), 8, gold);
                box(x + 3, 108, 56, 22, sf::Color(42, 48, 46), 5, alpha(gold, .25f));
                sf::Text caption(body_, u8(speedNames[i]), 15);
                caption.setStyle(sf::Text::Bold);
                const auto bounds = caption.getLocalBounds();
                caption.setPosition({x + (56 - bounds.size.x) / 2 - bounds.position.x,
                                     105 + (28 - bounds.size.y) / 2 - bounds.position.y});
                caption.setFillColor(paper);
                target_->draw(caption);
                textBounds_.push_back(caption.getGlobalBounds());
                line({x + 49, 118}, {x + 52, 121}, gold, 1.8f);
                line({x + 52, 121}, {x + 58, 113}, gold, 1.8f);
            }
        }
        if (s.playing)
            button(s.paused ? "Продолжить" : "Пауза", 941, 105, 111, Pause, true, false, 28);
    } else
        actions(s, 32, 778, 1020, 4);
}

void Scene::journal(const State& s) {
    box(1072, 140, 336, 690, ink, 16, border);
    label("Журнал боя", 1096, 164, 29, paper, true);
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
        paragraph("Здесь появятся события боя.", 1114, 492, 250, 14, muted);
    }
    if (static_cast<int>(s.log.lines.size()) > LogLines) {
        const float total = static_cast<float>(s.log.lines.size()),
                    thumb = std::max(18.f, 500 * LogLines / total);
        box(1394, 249, 2, 500, raised, 1);
        box(1394, 249 + (500 - thumb) * first / (total - LogLines), 2, thumb, gold, 1);
    }
    label(s.logOffset == 0 ? "Колесо мыши — прокрутка журнала" : "Просмотр предыдущих событий", 1096, 793, 10,
          muted);
}
void Scene::effects(const State& s) {
    const auto& step = *s.effect;
    const auto& e = step.event;
    const float p = step.progress(), pulse = std::sin(p * Pi);
    const auto position = [&](presentation::Slot slot) {
        auto value = slotCenter(slot, s);
        if (e.type == EventType::Clone && !step.committed && slot.valid() &&
            slot.team == step.actor.team && slot.index >= step.actor.index)
            value.x += (slot.team == 0 ? -95.f : 95.f) * presentation::smooth((p - .2f) / .3f);
        return value;
    };
    const auto a = position(presentation::actorSlot(step)), b = position(presentation::targetSlot(step));
    const auto created = slotCenter(step.actor, s);
    const bool sourceVisible = step.target.valid() &&
        step.target.index >= static_cast<int>(s.offsets[step.target.team]) &&
        step.target.index < static_cast<int>(s.offsets[step.target.team] + PageUnits);
    const auto color = e.type == EventType::Heal ? green : e.type == EventType::Clone ? violet
                     : e.type == EventType::Buff ? gold : e.team == 0 ? blue : red;
    const auto arc = [](sf::Vector2f from, sf::Vector2f to, float t, float height) {
        return from + (to - from) * t + sf::Vector2f(0, -std::sin(t * Pi) * height);
    };
    const auto cross = [&](sf::Vector2f at, float size, float opacity) {
        line(at - sf::Vector2f(size, 0), at + sf::Vector2f(size, 0), alpha(ink, opacity), 9);
        line(at - sf::Vector2f(0, size), at + sf::Vector2f(0, size), alpha(ink, opacity), 9);
        line(at - sf::Vector2f(size, 0), at + sf::Vector2f(size, 0), alpha(green, opacity), 5);
        line(at - sf::Vector2f(0, size), at + sf::Vector2f(0, size), alpha(green, opacity), 5);
    };
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
    if (e.type == EventType::Heal) {
        for (int i = 0; i < 3; ++i) {
            const auto t = std::clamp((p - .08f - i * .045f) / .36f, 0.f, 1.f);
            if (t > 0 && t < 1) {
                const auto at = arc(a, b, t, 70.f + i * 12.f);
                cross(at, 7.f + i * 2.f, .95f);
                line(at - sf::Vector2f((b.x - a.x) * .08f, 6), at, alpha(green, .45f), 3);
            }
        }
        if (p >= .45f) {
            cross(b - sf::Vector2f(0, 12), 17 + 6 * pulse, .95f);
            for (int i = 0; i < 3; ++i)
                cross(b + sf::Vector2f(-24.f + i * 24.f, 25 - (p - .45f) * 85), 5, .8f);
        }
    }
    char32_t equipment = 0;
    std::string equipmentName = e.buff;
    if (e.type == EventType::Buff) {
        for (const auto& item : battletext::equipment)
            if (item.first == e.buff) equipment = u8(item.second)[0];
        if (e.buff == "шлем") equipmentName = "Шлем";
        else if (e.buff == "щит") equipmentName = "Щит";
        else if (e.buff == "копьё") equipmentName = "Копьё";
        else if (e.buff == "конь") equipmentName = "Конь";
        const auto t = std::clamp((p - .08f) / .42f, 0.f, 1.f);
        const auto item = arc(a, b, t, 88);
        if (p < .5f) {
            line(a, item, alpha(gold, .4f), 3);
            box(item.x - 24, item.y - 24, 48, 48, ink, 9, gold);
            if (equipment) drawEquipmentGlyph(*target_, equipment, item.x - 19, item.y - 19, 38, gold);
        } else {
            if (equipment) drawEquipmentGlyph(*target_, equipment, b.x - 27, b.y - 36, 54, gold);
            for (int i = 0; i < 5; ++i) {
                const auto at = b + sf::Vector2f(-30.f + 15.f * i, -48 - (p - .5f) * 40);
                polygon({at + sf::Vector2f(0, -5), at + sf::Vector2f(3, 0),
                         at + sf::Vector2f(0, 5), at + sf::Vector2f(-3, 0)}, alpha(gold, .8f));
            }
        }
    }
    if (e.type == EventType::Clone) {
        if (p < .5f) {
            box(a.x - 17, a.y - 25, 25, 34, ink, 4, violet);
            box(a.x - 6, a.y - 15, 25, 34, raised, 4, violet);
            if (sourceVisible) {
                line(a, b, alpha(violet, .8f), 3);
                callout("Оригинал", b - sf::Vector2f(0, 126), violet, 1, 14);
            }
            const float t = presentation::smooth((p - .20f) / .30f);
            if (p >= .20f) {
                const auto ghost = arc(sourceVisible ? b : a, created, t, 80);
                box(ghost.x - 35, ghost.y - 48, 70, 86, alpha(ink, .8f), 9, violet);
                if (step.clone && step.clone->detailsKnown)
                    warrior(step.clone->kind, ghost.x, ghost.y - 42, 65, typeColor(step.clone->kind),
                            e.team == 1, .85f);
                else {
                    box(ghost.x - 18, ghost.y - 31, 30, 39, raised, 4, violet);
                    box(ghost.x - 7, ghost.y - 20, 30, 39, panel, 4, violet);
                }
                line(sourceVisible ? b : a, ghost, alpha(violet, .5f), 2);
            }
        } else {
            // A new card occupies the space opened directly before the caster.
            line(created + sf::Vector2f(-37, -98), created + sf::Vector2f(37, -98), violet, 4);
            line(created + sf::Vector2f(-37, 153), created + sf::Vector2f(37, 153), violet, 4);
        }
    }
    if (e.type == EventType::Death && p >= .25f) {
        line(a + sf::Vector2f(-22, -26), a + sf::Vector2f(22, 18), alpha(red, pulse), 5);
        line(a + sf::Vector2f(22, -26), a + sf::Vector2f(-22, 18), alpha(red, pulse), 5);
    }
    if (e.type == EventType::Promote) {
        for (int i = 0; i < 3; ++i) {
            const float direction = e.team == 0 ? 1.f : -1.f;
            const auto at = a + sf::Vector2f(-direction * (45.f + i * 18.f), 208);
            line(at + sf::Vector2f(-direction * 6, -7), at, alpha(gold, pulse), 3);
            line(at, at + sf::Vector2f(-direction * 6, 7), alpha(gold, pulse), 3);
        }
    }
    if (p >= .5f) {
        const float opacity = p < .94f ? 1.f : (1 - p) / .06f;
        const auto center = b - sf::Vector2f(0, 119 + (p - .5f) * 40);
        if (e.type == EventType::Melee || e.type == EventType::Shot)
            callout("−" + std::to_string(e.damage), center, red, opacity, 25);
        else if (e.type == EventType::Heal)
            callout("+" + std::to_string(e.restored), center, green, opacity, 25);
        else if (e.type == EventType::Buff)
            callout(equipmentName, center, gold, opacity, 21, equipment);
        else if (e.type == EventType::Clone && step.clone) {
            const auto number = cloneidentity::parse(step.clone->name).number;
            callout(number ? "Клон " + std::to_string(number) : "Копия",
                    created - sf::Vector2f(0, 123), violet, opacity, 19);
        }
    }
    box(262, 703, 560, 23, ink, 8, alpha(color, .55f));
    std::string caption = actionName(e.type);
    if (e.type == EventType::Clone && step.clone) caption += ": " + step.clone->name;
    if (e.type == EventType::Buff && !equipmentName.empty()) caption += ": " + equipmentName;
    if (s.paused) caption += " · пауза";
    const auto value = fit(caption, 536, 12);
    text(value, 542 - textWidth(value, 12) / 2, 705, 12, paper);
}
void Scene::archive(const State& s) {
    const bool deleting = std::any_of(s.choices.begin(), s.choices.end(),
                                      [](const MenuChoice& c) { return c.label == "К списку сохранений"; });
    header();
    label(deleting ? "Удаление сохранений" : "Сохранения", 80, 126, 49, paper, true);
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
    label(entry ? (s.enteringText                           ? "Сохранить игру"
                   : s.numberCancel == NumberCancel::Dialog ? "Изменение состава армии"
                                                            : "Настройки партии")
                : "Подтверждение действия",
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
                                                     : "Оставьте поле пустым, чтобы использовать предложенное имя. Enter — сохранить.")
                                        : "Введите целое число. Enter — продолжить.")
                      : s.error,
                  398, 461, 630, 13, s.error.empty() ? muted : red);
        button(s.enteringText ? "Сохранить" : "Продолжить", 468, 552, FooterButtonWidth, Submit, true, false,
               FooterHeight);
        button(s.enteringText || s.numberCancel == NumberCancel::Dialog ? "Отмена" : "В главное меню",
               468 + FooterButtonWidth + FooterGap, 552, FooterButtonWidth, Cancel, true, false,
               FooterHeight);
    } else {
        if (!s.choices.empty() && contains(s.choices.front().label, "Заменить"))
            paragraph("Текущая партия будет заменена выбранным сохранением.", 398, 336, 630, 15, muted);
        else if (!s.notices.empty())
            paragraph(s.notices.back(), 398, 336, 630, 15, muted);
        for (std::size_t i = 0; i < s.choices.size(); ++i) {
            const auto& c = s.choices[i];
            button(c.label, 468 + i * (FooterButtonWidth + FooterGap), 552, FooterButtonWidth,
                   static_cast<int>(c.value), c.enabled, false, FooterHeight);
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
    clip_.reset();
    background(s);
    if (s.handoff >= 0) {
        header();
        crest(656, 205, 126, teamColor(s.handoff));
        label("Передача управления", 414, 388, 45, paper, true);
        label("Игрок " + std::to_string(s.handoff + 1), 642, 461, 28, teamColor(s.handoff), true);
        label("Другой игрок не должен видеть, как вы набираете армию.", 502, 526, 16, muted);
        button("Начать набор армии", 468, 600, FooterButtonWidth, Submit, true, false, FooterHeight);
        button("Назад", 468 + FooterButtonWidth + FooterGap, 600, FooterButtonWidth,
               static_cast<int>(uiAction::Back), true, false, FooterHeight);
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
