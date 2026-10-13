#include "gameScene.hpp"
#include "EngineContext.hpp"
#include "components/AABBCollider.hpp"
#include "components/Controllable.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"

#include "systems/AnimationSystem.hpp"
#include "systems/RenderConcept.hpp"

#include "../input/action.hpp"

namespace rtype::client {
GameScene::GameScene(rtk::RenderWindow &window) : _window(window) {}

void GameScene::onEnter(rtk::EngineContext &context) {
  createBackground(context);
}

std::optional<SceneType> GameScene::onUpdate(rtk::EngineContext &context,
                                             const rtk::InputState &inputState,
                                             float dt) {

  return std::nullopt;
}

void GameScene::onExit(rtk::EngineContext &context) {
  for (const auto entity : _entities)
    context.registry.kill_entity(entity);

  context.registry.flush();

  _entities.clear();
  _playerEntity.reset();
}

} // namespace rtype::client
