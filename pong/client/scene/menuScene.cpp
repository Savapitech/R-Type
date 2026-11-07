#include "scene/menuScene.hpp"

#include <cmath>

#include "action.hpp"
#include "components/Transform.hpp"
#include "systems/RenderConcept.hpp"

namespace pong::client {

PongMenuScene::PongMenuScene(rtk::RenderWindow &window) : _window(window) {}

void PongMenuScene::onEnter(rtk::EngineContext &context) {
  _entities.clear();

  _entities.push_back(
      context._entityFactory.createFromBlueprint(context.registry, "wall_top"));
  _entities.push_back(context._entityFactory.createFromBlueprint(
      context.registry, "wall_bottom"));

  for (float y = 14.0f; y < 706.0f; y += 22.0f) {
    const auto dash = context._entityFactory.createFromBlueprint(
        context.registry, "center_dash");
    if (auto *trans =
            context.registry.get_components<::component::Transform>().get(
                dash)) {
      trans->position.y = y;
    }
    _entities.push_back(dash);
  }

  _entities.push_back(context._entityFactory.createFromBlueprint(
      context.registry, "paddle_left"));
  _entities.push_back(context._entityFactory.createFromBlueprint(
      context.registry, "paddle_right"));

  _entities.push_back(
      context._entityFactory.createFromBlueprint(context.registry, "ball"));

  _promptEntity = context._entityFactory.createFromBlueprint(context.registry,
                                                             "menu_prompt");
  _entities.push_back(_promptEntity);

  _blinkTimer = 0.0f;
  _matchStarted = false;
}

std::optional<PongSceneType>
PongMenuScene::onUpdate(rtk::EngineContext &context,
                        const rtk::InputState &input, float dt) {
  _blinkTimer += dt;
  if (_promptEntity != 0) {
    if (auto *sprite =
            context.registry.get_components<rtk::ecs::concepts::SpriteData>()
                .get(_promptEntity)) {
      const auto alpha = static_cast<std::uint8_t>(
          (std::sin(_blinkTimer * 5.0f) > 0.0f) ? 255 : 40);
      sprite->color.a = alpha;
    }
  }

  if (_matchStarted ||
      pong::Input::hasAction(input.actionMask, pong::Action::Confirm)) {
    return PongSceneType::Game;
  }

  return std::nullopt;
}

void PongMenuScene::onExit(rtk::EngineContext &context) {
  for (auto ent : _entities) {
    context.registry.remove_entity_from_all_pools(ent);
    context.registry.kill_entity(ent);
  }
  _entities.clear();
  _promptEntity = 0;
}

} // namespace pong::client
