#pragma once

#include <ISystem.hpp>
#include <array>

#include "components/PongComponents.hpp"
#include "events/ScoreEvent.hpp"
#include "events/busEvents.hpp"
#include "systems/RenderConcept.hpp"

namespace pong::systems {

/**
 * @brief Listens for ScoreChangedEvent on the engine event bus and updates score sprites.
 */
class PongScoreSystem final : public rtk::ecs::ISystem {
public:
  static constexpr std::array<rtk::ecs::concepts::TextureRectU16, 10>
      DIGIT_RECTS = {{
          {4, 3, 16, 32},   // 0
          {23, 3, 4, 32},   // 1
          {30, 3, 16, 32},  // 2
          {49, 3, 16, 32},  // 3
          {68, 3, 16, 32},  // 4
          {87, 3, 16, 32},  // 5
          {106, 3, 16, 32}, // 6
          {125, 3, 16, 32}, // 7
          {144, 3, 16, 32}, // 8
          {163, 3, 16, 32}  // 9
      }};

  /**
   * @brief Constructs the score system and subscribes to ScoreChangedEvent.
   * @param reg ECS entity registry.
   * @param eventBus Engine event bus.
   */
  PongScoreSystem(rtk::ecs::Registry &reg, busEvent &eventBus) : _reg(reg) {
    eventBus.subscribe<pong::events::ScoreChangedEvent>(
        [this](const pong::events::ScoreChangedEvent &event) {
          onScoreChanged(event);
        });
  }

  ~PongScoreSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {}
  void update(rtk::ecs::Registry &, float) override {}
  void onStop(rtk::ecs::Registry &) override {}

  /**
   * @brief Updates sprite textureRect for score entities when a point is scored.
   * @param event Event payload with current match scores.
   */
  void onScoreChanged(const pong::events::ScoreChangedEvent &event) {
    auto view = _reg.view<rtk::ecs::concepts::SpriteData,
                          pong::component::ScoreDisplay>();
    auto &sprites = _reg.get_components<rtk::ecs::concepts::SpriteData>();
    auto &scores = _reg.get_components<pong::component::ScoreDisplay>();

    for (auto ent : view) {
      auto *sprite = sprites.get(ent);
      auto *score = scores.get(ent);
      if (!sprite || !score)
        continue;

      if (score->side == pong::component::Paddle::Side::Left) {
        score->score = event.scoreP1;
        sprite->textureRect = DIGIT_RECTS[event.scoreP1 % 10];
      } else {
        score->score = event.scoreP2;
        sprite->textureRect = DIGIT_RECTS[event.scoreP2 % 10];
      }
    }
  }

private:
  rtk::ecs::Registry &_reg;
};

} // namespace pong::systems
