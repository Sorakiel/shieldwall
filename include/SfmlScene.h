#pragma once
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <optional>
#include <algorithm>
#include "View.h"
#include "BattleLog.h"
#include "BattlePlayback.h"
#include "ArmyUi.h"
#include "BattleMotion.h"
#include "NumberInput.h"

namespace sfmlui {
inline constexpr float Width = 1440.f, Height = 900.f;
inline constexpr int Next = 100, Auto = 101, Submit = 102, Save = 103, Cancel = 104, Speed = 105, Pause = 106,
                     Speed2 = 107, Speed4 = 108, SpeedHalf = 109;
inline constexpr int RemoveBase = 1000;
inline constexpr float FooterButtonWidth = 246.f, FooterGap = 12.f, FooterHeight = 52.f;
inline constexpr sf::Vector2f RosterCardSize{139.f, 140.f};
inline constexpr std::size_t RosterUnits = 9;
inline constexpr float RosterPitch = 149.f;
inline const sf::FloatRect RosterArea{{50, 578}, {1331, 152}};
inline const std::array<sf::FloatRect, 2> ArmyAreas{{{{44, 170}, {476, 530}}, {{572, 170}, {476, 530}}}};
struct RosterCard {
    sf::FloatRect bounds;
    std::size_t index;
};
struct DragState {
    std::size_t from = 0;
    sf::Vector2f origin, pointer;
    bool active = false;
    sf::Vector2f grab{RosterCardSize.x / 2, RosterCardSize.y / 2};
};
inline sf::Vector2f dragPosition(const DragState& drag) {
    return {std::clamp(drag.pointer.x - drag.grab.x, 48.f, 1248.f),
            std::clamp(drag.pointer.y - drag.grab.y, 120.f, 740.f)};
}
struct RosterTransition {
    ArmyEdit edit;
    UnitSnapshot unit;
    float progress = 0;
    std::optional<sf::Vector2f> drop;
};
inline constexpr std::size_t PageUnits = 5;
inline constexpr unsigned LogSize = 14;
inline constexpr int LogLines = 23;
inline constexpr float LogWidth = 288.f;
struct Button {
    sf::FloatRect bounds;
    int action;
    bool enabled;
};
struct State {
    ViewStage stage = ViewStage::MainMenu;
    BattleSnapshot battle;
    RecruitmentSnapshot recruitment;
    battletext::Log log;
    std::vector<MenuChoice> choices;
    std::vector<std::string> notices;
    std::array<float, 2> offsets{};
    std::array<std::optional<presentation::Scroll>, 2> scrolls{};
    std::array<bool, 2> manualScrolls{};
    bool recruiting = false, waiting = false, automatic = false, playing = false, paused = false;
    bool enteringNumber = false, enteringText = false, selectAll = false;
    NumberCancel numberCancel = NumberCancel::MainMenu;
    int handoff = -1, turn = 0, winner = -1, logOffset = 0;
    float speed = 1.f, ambience = 0.f;
    std::string prompt, input, error, result;
    NumberInput number;
    std::optional<presentation::Step> effect;
    std::optional<DragState> drag;
    std::optional<RosterTransition> rosterTransition;
};
inline float displayedOffset(const State& state, int team) {
    return state.scrolls[team] ? state.scrolls[team]->value() : state.offsets[team];
}
inline bool scrolling(const State& state) { return state.scrolls[0] || state.scrolls[1]; }
inline std::optional<int> hoveredArmy(const State& state, sf::Vector2f point) {
    if (state.recruiting) return RosterArea.contains(point) ? std::optional<int>(state.recruitment.activeTeam) : std::nullopt;
    for (int team = 0; team < 2; ++team)
        if (ArmyAreas[team].contains(point)) return team;
    return std::nullopt;
}
class Scene {
public:
    explicit Scene(const std::filesystem::path& assetDirectory);
    void draw(sf::RenderTarget& target, const State& state, sf::Vector2f mouse = {-1.f, -1.f});
    const std::vector<Button>& buttons() const {
        return buttons_;
    }
    const std::vector<RosterCard>& rosterCards() const {
        return rosterCards_;
    }
    std::optional<std::size_t> dropTarget(const DragState& drag) const;
    float textWidth(const sf::String& value, unsigned size = LogSize, bool display = false) const;
    const std::vector<sf::FloatRect>& textBounds() const {
        return textBounds_;
    }
    bool saves(const State& state) const;
    bool placeNumberCaret(State& state, sf::Vector2f point, bool extend = false) const;
    sf::Vector2f slotCenter(presentation::Slot slot, const State& state) const;

private:
    sf::Font body_, display_;
    sf::RenderTarget* target_ = nullptr;
    sf::Vector2f mouse_;
    std::vector<Button> buttons_;
    std::vector<RosterCard> rosterCards_;
    std::vector<sf::FloatRect> textBounds_;
    std::optional<sf::FloatRect> clip_;
    sf::View beginClip(const sf::FloatRect& area);
    void endClip(const sf::View& previous);
    void numberField(const State& state, float x, float y, unsigned size);
    void box(float x, float y, float w, float h, sf::Color fill, float radius = 12.f,
             sf::Color stroke = sf::Color::Transparent);
    void line(sf::Vector2f a, sf::Vector2f b, sf::Color color, float thickness = 1.f);
    void disc(float x, float y, float radius, sf::Color color);
    void polygon(std::initializer_list<sf::Vector2f> points, sf::Color color);
    void text(const sf::String& value, float x, float y, unsigned size = 16,
              sf::Color color = sf::Color(229, 225, 213), bool display = false);
    void label(const std::string& value, float x, float y, unsigned size = 16,
               sf::Color color = sf::Color(229, 225, 213), bool display = false);
    sf::String fit(const std::string& value, float width, unsigned size, bool display = false) const;
    void paragraph(const std::string& value, float x, float y, float width, unsigned size, sf::Color color,
                   unsigned maxLines = 0);
    void button(const std::string& label, float x, float y, float w, int action, bool enabled = true,
                bool primary = false, float h = 46.f);
    void crest(float x, float y, float size, sf::Color color);
    void callout(const std::string& value, sf::Vector2f center, sf::Color color, float opacity = 1.f,
                 unsigned size = 23, char32_t glyph = 0);
    void warrior(UnitKind kind, float x, float y, float size, sf::Color color, bool facingLeft = false,
                 float opacity = 1.f);
    void background(const State& state);
    void header();
    void menu(const State& state);
    void settings(const State& state);
    void recruitment(const State& state);
    void field(const State& state);
    void army(const State& state, int team);
    void journal(const State& state);
    void effects(const State& state);
    void archive(const State& state);
    void modal(const State& state);
    void actions(const State& state, float x, float y, float width, int columns = 4);
    void notice(const State& state, float x, float y, float width);
};
} // namespace sfmlui
