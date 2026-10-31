#pragma once
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <optional>
#include "View.h"
#include "BattleLog.h"
#include "BattlePlayback.h"
#include "ArmyUi.h"

namespace sfmlui {
inline constexpr float Width = 1440.f, Height = 900.f;
inline constexpr int Next = 100, Auto = 101, Submit = 102, Save = 103, Cancel = 104, Speed = 105, Pause = 106;
inline constexpr int RemoveBase = 1000;
struct RosterCard {
    sf::FloatRect bounds;
    std::size_t index;
};
struct DragState {
    std::size_t from = 0;
    sf::Vector2f origin, pointer;
    bool active = false;
};
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
    std::array<std::size_t, 2> offsets{};
    bool recruiting = false, waiting = false, automatic = false, playing = false, paused = false;
    bool enteringNumber = false, enteringText = false, selectAll = false;
    NumberCancel numberCancel = NumberCancel::MainMenu;
    int handoff = -1, turn = 0, winner = -1, logOffset = 0;
    float speed = 1.f, ambience = 0.f;
    std::string prompt, input, error, result;
    std::optional<presentation::Step> effect;
    std::optional<DragState> drag;
    std::optional<RosterTransition> rosterTransition;
};
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
    float textWidth(const sf::String& value, unsigned size = LogSize, bool display = false) const;
    const std::vector<sf::FloatRect>& textBounds() const {
        return textBounds_;
    }
    bool saves(const State& state) const;
    sf::Vector2f slotCenter(presentation::Slot slot, const State& state) const;

private:
    sf::Font body_, display_;
    sf::RenderTarget* target_ = nullptr;
    sf::Vector2f mouse_;
    std::vector<Button> buttons_;
    std::vector<RosterCard> rosterCards_;
    std::vector<sf::FloatRect> textBounds_;
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
    void warrior(UnitKind kind, float x, float y, float size, sf::Color color, bool facingLeft = false,
                 float opacity = 1.f);
    void background(const State& state);
    void header(const std::string& section, const std::string& subtitle);
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
