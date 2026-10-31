#pragma once
#include <SFML/Graphics.hpp>
#include <initializer_list>

inline bool equipmentGlyph(char32_t point) {
    return point == U'🛡' || point == U'🐎' || point == U'🪖' || point == U'🗡';
}

// Small vector emoji: available on every platform, including fonts without emoji.
inline void drawEquipmentGlyph(sf::RenderTarget& target, char32_t point,
    float x, float y, float size, sf::Color color) {
    auto polygon = [&](std::initializer_list<sf::Vector2f> points, sf::Color fill) {
        sf::ConvexShape shape(points.size());
        std::size_t i = 0;
        for (const auto p : points) shape.setPoint(i++, {x + p.x * size, y + p.y * size});
        shape.setFillColor(fill);
        target.draw(shape);
    };
    const sf::Color dark(23, 32, 45);
    if (point == U'🛡') {
        polygon({{.1f,.12f},{.5f,.02f},{.9f,.12f},{.84f,.61f},{.5f,.98f},{.16f,.61f}}, color);
        polygon({{.48f,.14f},{.56f,.14f},{.56f,.8f},{.48f,.86f}}, dark);
        polygon({{.24f,.38f},{.77f,.38f},{.76f,.47f},{.25f,.47f}}, dark);
    } else if (point == U'🪖') {
        polygon({{.12f,.63f},{.14f,.3f},{.31f,.08f},{.69f,.08f},{.86f,.3f},{.88f,.63f}}, color);
        polygon({{.37f,.61f},{.65f,.61f},{.65f,.92f},{.51f,.98f},{.37f,.92f}}, color);
        polygon({{.21f,.5f},{.46f,.5f},{.46f,.58f},{.21f,.58f}}, dark);
        polygon({{.57f,.5f},{.8f,.5f},{.8f,.58f},{.57f,.58f}}, dark);
        polygon({{.48f,.15f},{.54f,.15f},{.54f,.43f},{.48f,.43f}}, dark);
    } else if (point == U'🐎') {
        // Knight silhouette, composed of convex pieces for SFML triangulation.
        polygon({{.29f,.85f},{.59f,.25f},{.74f,.32f},{.83f,.58f},{.72f,.85f}}, color);
        polygon({{.05f,.47f},{.15f,.32f},{.36f,.18f},{.65f,.18f},{.65f,.43f},{.21f,.58f}}, color);
        polygon({{.3f,.05f},{.46f,.23f},{.31f,.25f}}, color);
        polygon({{.59f,.07f},{.68f,.25f},{.44f,.23f}}, color);
        polygon({{.15f,.88f},{.83f,.88f},{.83f,.98f},{.15f,.98f}}, color);
        polygon({{.32f,.29f},{.4f,.29f},{.4f,.36f},{.32f,.36f}}, dark);
        polygon({{.57f,.23f},{.64f,.25f},{.72f,.5f},{.67f,.61f},{.65f,.39f}}, dark);
    } else if (point == U'🗡') {
        // A spear rather than the dagger shape of the Unicode fallback.
        polygon({{.46f,.98f},{.54f,.98f},{.54f,.34f},{.46f,.34f}}, color);
        polygon({{.5f,.01f},{.69f,.3f},{.5f,.43f},{.31f,.3f}}, color);
        polygon({{.49f,.12f},{.52f,.16f},{.52f,.34f},{.49f,.36f}}, dark);
    }
}
