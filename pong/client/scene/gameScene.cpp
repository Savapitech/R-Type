#include "scene/gameScene.hpp"

#include "action.hpp"
#include "components/Transform.hpp"
#include "systems/PongBallSystem.hpp"

namespace pong::client {

PongGameScene::PongGameScene(rtk::RenderWindow &window) : _window(window) {}

void PongGameScene::onEnter(rtk::EngineContext &context) {
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

  // Paddles
  _entities.push_back(context._entityFactory.createFromBlueprint(
      context.registry, "paddle_left"));
  _entities.push_back(context._entityFactory.createFromBlueprint(
      context.registry, "paddle_right"));

  // Ball
  _entities.push_back(
      context._entityFactory.createFromBlueprint(context.registry, "ball"));

  // Scores
  _entities.push_back(
      context._entityFactory.createFromBlueprint(context.registry, "score_p1"));
  _entities.push_back(
      context._entityFactory.createFromBlueprint(context.registry, "score_p2"));

  if (_ballSystem && _ballSystem->isEnabled()) {
    _ballSystem->resetMatch(context.registry);
  }
}

std::optional<PongSceneType>
PongGameScene::onUpdate(rtk::EngineContext &context,
                        const rtk::InputState &input, float dt) {
  (void)context;
  (void)dt;

  if (pong::Input::hasAction(input.actionMask, pong::Action::Back)) {
    return PongSceneType::Menu;
  }

  return std::nullopt;
}

void PongGameScene::onExit(rtk::EngineContext &context) {
  for (auto ent : _entities) {
    context.registry.remove_entity_from_all_pools(ent);
    context.registry.kill_entity(ent);
  }
  _entities.clear();
}

} // namespace pong::client
