#pragma once

#include <functional>
#include <stdexcept>
#include <vector>

#include "components/Transform.hpp"
#include <ISystem.hpp>
#include <Logger/Logger.hpp>

#include "systems/RenderConcept.hpp"

namespace rtk::systems {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Draws every entity that has a Transform and a SpriteData.
 * @tparam WindowType Window satisfying IsRenderWindow.
 */
template <rtk::ecs::concepts::IsRenderWindow WindowType>
class RenderSystem final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Creates the system.
   * @param window Window to draw into.
   */
  RenderSystem(std::reference_wrapper<WindowType> window)
      : _window(window.get()) {}

  ~RenderSystem() = default;

  /**
   * @brief Logs that the system is loaded.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Render System");
  }

  /**
   * @brief Copies transforms into sprites and draws one frame.
   * @param reg Registry holding the entities.
   */
  void update(rtk::ecs::Registry &reg, float) override {
    _spritesToDraw.clear();

    auto view =
        reg.view<component::Transform, rtk::ecs::concepts::SpriteData>();

    auto &transforms = reg.get_components<component::Transform>();
    auto &sprites = reg.get_components<rtk::ecs::concepts::SpriteData>();

    for (auto entity : view) {
      const auto *transform = transforms.get(entity);
      const auto *sourceSprite = sprites.get(entity);

      if (!transform || !sourceSprite)
        continue;

      rtk::ecs::concepts::SpriteData sprite = *sourceSprite;

      sprite.position = transform->position;
      sprite.rotation = transform->rotation;
      sprite.scale = transform->scale;

      _spritesToDraw.push_back(std::move(sprite));
    }

    if (!_window.beginFrame(rtk::ecs::concepts::ColorRGBA8{0, 0, 0, 255}))
      return;

    _window.draw(_spritesToDraw);
    _window.endFrame();
  }

  /**
   * @brief Does nothing.
   */
  void onStop(rtk::ecs::Registry &) override {}

private:
  WindowType &_window;
  std::vector<rtk::ecs::concepts::SpriteData> _spritesToDraw;
};

/** @} */
} // namespace rtk::systems
