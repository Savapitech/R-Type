#include "gameScene.hpp"
#include "EngineContext.hpp"
#include "components/AABBCollider.hpp"
#include "components/Controllable.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "events/GameEvents.hpp"

#include "game.hpp"
#include "scene/clientDependency.hpp"
#include "systems/AnimationSystem.hpp"
#include "systems/RenderConcept.hpp"

#include "../input/action.hpp"
#include "ui/Text.hpp"

namespace rtype::client {
GameScene::GameScene(clientDependancy &dep) : _dependency(dep) {}

void GameScene::onEnter(rtk::EngineContext &context) {
  createParallaxBackground(context, "background_stars");
  createParallaxBackground(context, "background_fast_stars");

  context.eventBus.publish(rtype::events::PlayMusicEvent{
      .path = "assets/sound/r-type_music.mp3",
      .loop = true,
  });
}

std::optional<SceneType> GameScene::onUpdate(rtk::EngineContext &context,
                                             const rtk::InputState &inputState,
                                             float dt) {
  if (!_dependency.game.isGameStarting()) {
    context.eventBus.publish(rtype::events::StopMusicEvent{});
    return SceneType::GameOver;
  }

  if (_dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Escape) ||
      rtype::Input::hasAction(inputState.actionMask,
                              rtype::Action::LeaveRoom)) {
    context.eventBus.publish(rtype::events::StopMusicEvent{});
    return SceneType::GameOver;
  }

  const std::string score =
      "SCORE " + std::to_string(_dependency.game.getRoundScore());
  if (score != _scoreText) {
    _scoreText = score;
    ui::clearText(context.registry, _scoreEntities);
    ui::drawText(context.registry, _dependency.window,
                 _dependency.game.getFont(), score, 1060.0f, 50.0f, 0.5f,
                 _scoreEntities);
  }
  return std::nullopt;
}

void GameScene::onExit(rtk::EngineContext &context) {
  context.eventBus.publish(rtype::events::StopMusicEvent{});
  _dependency.game.setGameStarting(false);
  _dependency.game.clearNetworkEntities(context);

  for (const auto entity : _entities)
    context.registry.kill_entity(entity);
  ui::clearText(context.registry, _scoreEntities);
  _scoreText.clear();

  context.registry.flush();

  _entities.clear();
  _playerEntity.reset();
}

} // namespace rtype::client
