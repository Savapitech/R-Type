#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <Registry.hpp>
#include <font/font.hpp>
#include <graphical/renderWindow.hpp>

#include "components/Transform.hpp"
#include "systems/RenderConcept.hpp"

namespace rtype::client::ui {

/**
 * @brief Spawns one sprite entity per glyph of a text, one line per '\n'.
 * @param registry Registry receiving the entities.
 * @param window Window building the glyphs.
 * @param font Font of the text.
 * @param text Text to draw.
 * @param x Left of the text, in pixels.
 * @param y Top of the first line, in pixels.
 * @param scale Size of the text relative to the font size.
 * @param entities Receives the spawned entities.
 */
inline void drawText(rtk::ecs::Registry &registry,
                     const rtk::RenderWindow &window, const rtk::Font &font,
                     const std::string &text, float x, float y, float scale,
                     std::vector<std::size_t> &entities) {
  std::size_t start = 0;
  while (start <= text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == std::string::npos)
      end = text.size();
    const rtk::Text line =
        window.createText(text.substr(start, end - start), font, {x, y});
    for (const auto &glyph : line.getSprites()) {
      const auto entity = registry.spawn_entity();
      rtk::ecs::concepts::SpriteData sprite{};
      sprite.textureId = glyph.textureId;
      sprite.textureRect = {glyph.textureRect.left, glyph.textureRect.top,
                            glyph.textureRect.width, glyph.textureRect.height};
      sprite.color = {255, 255, 255, 255};
      sprite.size = {glyph.size.x, glyph.size.y};
      sprite.origin = {glyph.origin.x, glyph.origin.y};
      sprite.layer = 120;
      component::Transform transform{};
      transform.position = {x + (glyph.position.x - x) * scale,
                            y + (glyph.position.y - y) * scale};
      transform.scale = {glyph.scale.x * scale, glyph.scale.y * scale};
      registry.get_components<component::Transform>().insert_at(entity,
                                                                transform);
      registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
          entity, sprite);
      entities.push_back(entity);
    }
    y += 60.0f * scale;
    start = end + 1;
  }
}

/**
 * @brief Kills entities and clears their list.
 * @param registry Registry holding the entities.
 * @param entities Entities to kill.
 */
inline void clearText(rtk::ecs::Registry &registry,
                      std::vector<std::size_t> &entities) {
  for (const auto entity : entities)
    registry.kill_entity(entity);
  entities.clear();
}

} // namespace rtype::client::ui
