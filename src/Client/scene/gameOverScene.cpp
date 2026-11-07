#include "gameOverScene.hpp"

#include <cmath>
#include <string>

#include "components/Transform.hpp"
#include "events/GameEvents.hpp"
#include "game.hpp"
#include "input/action.hpp"
#include "systems/RenderConcept.hpp"

namespace rtype::client {

constexpr float BASE_GLYPH_ADVANCE = 40.0f;
constexpr float SCREEN_CENTER_X = 640.0f;
constexpr float NAV_COOLDOWN = 0.22f;

void addCenteredText(rtk::EngineContext &context, rtk::RenderWindow &window,
                     const rtk::Font &font, const std::string &content, float y,
                     const rtk::ecs::concepts::ColorRGBA8 &color,
                     float scaleFactor, std::vector<std::size_t> &entities) {
  if (!font.isValid() || content.empty())
    return;

  const float textWidth =
      static_cast<float>(content.length()) * BASE_GLYPH_ADVANCE * scaleFactor;
  const float startX = SCREEN_CENTER_X - (textWidth * 0.5f);

  const rtk::Text text =
      window.createText(content, font, {startX, y},
                        rtk::ColorRGBA8{color.r, color.g, color.b, color.a});

  for (const auto &glyph : text.getSprites()) {
    const auto entity = context.registry.spawn_entity();

    rtk::ecs::concepts::SpriteData sprite{};
    sprite.textureId = glyph.textureId;
    sprite.textureRect.x = glyph.textureRect.left;
    sprite.textureRect.y = glyph.textureRect.top;
    sprite.textureRect.width = glyph.textureRect.width;
    sprite.textureRect.height = glyph.textureRect.height;
    sprite.color = {glyph.color.r, glyph.color.g, glyph.color.b, glyph.color.a};
    sprite.size = {glyph.size.x, glyph.size.y};
    sprite.origin = {glyph.origin.x, glyph.origin.y};
    sprite.scale = {glyph.scale.x * scaleFactor, glyph.scale.y * scaleFactor};
    sprite.layer = 101;
    sprite.flags = 0;

    component::Transform transform{};
    transform.position.x = startX + (glyph.position.x - startX) * scaleFactor;
    transform.position.y = y + (glyph.position.y - y) * scaleFactor;
    transform.scale = {glyph.scale.x * scaleFactor,
                       glyph.scale.y * scaleFactor};
    transform.rotation = 0.0f;

    context.registry.get_components<component::Transform>().insert_at(
        entity, transform);
    context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
        entity, sprite);
    entities.push_back(entity);
  }
}

GameOverScene::GameOverScene(clientDependancy &dep) : _dependency(dep) {}

void GameOverScene::createBackground(rtk::EngineContext &context) {
  constexpr float backgroundWidth = 720.0f;

  const auto createLayer = [&](const std::string &blueprint) {
    for (int i = 0; i < 3; ++i) {
      const auto entity = context._entityFactory.createFromBlueprint(
          context.registry, blueprint);
      auto *transform =
          context.registry.get_components<component::Transform>().get(entity);
      if (transform) {
        transform->position.x = static_cast<float>(i) * backgroundWidth;
        transform->position.y = 0.0f;
      }
      _staticEntities.push_back(entity);
    }
  };

  createLayer("background_stars");
  createLayer("background_fast_stars");
}

void GameOverScene::onEnter(rtk::EngineContext &context) {
  _dependency.game.setGameStarting(false);
  _dependency.game.clearNetworkEntities(context);

  _font = _dependency.game.getFont();
  if (!_font.isValid()) {
    _font = _dependency.window.loadFont("./assets/font/r-type.otf", 40.0f);
  }

  _staticEntities.clear();
  _dynamicEntities.clear();
  _selectedOption = static_cast<int>(GameOverOption::ReturnToLobby);
  _inputCooldown = 0.0f;
  _elapsedTime = 0.0f;
  _previousUp = false;
  _previousDown = false;
  _previousConfirm = false;
  _previousEscape = false;
  _firstFrame = true;

  createBackground(context);
  context.eventBus.publish(rtype::events::StopMusicEvent{});
  context.eventBus.publish(rtype::events::PlayMusicEvent{
      .path = "assets/sound/game_over.mp3",
      .loop = true,
  });
  updateVisuals(context);
}

void GameOverScene::clearDynamicEntities(rtk::EngineContext &context) {
  for (const auto entity : _dynamicEntities) {
    context.registry.remove_entity_from_all_pools(entity);
    context.registry.kill_entity(entity);
  }
  _dynamicEntities.clear();
}

void GameOverScene::updateVisuals(rtk::EngineContext &context) {
  clearDynamicEntities(context);

  addCenteredText(context, _dependency.window, _font, "GAME OVER", 130.0f,
                  {255, 45, 45, 255}, 1.35f, _dynamicEntities);

  addCenteredText(context, _dependency.window, _font, ". MISSION FAILED .",
                  225.0f, {255, 175, 40, 230}, 0.45f, _dynamicEntities);

  struct OptionDef {
    GameOverOption option;
    std::string label;
    float y;
  };

  const OptionDef options[3] = {
      {GameOverOption::ReturnToLobby, "RETURN TO LOBBY", 345.0f},
      {GameOverOption::RoomList, "ROOM LIST", 420.0f},
      {GameOverOption::MainMenu, "MAIN MENU", 495.0f},
  };

  for (const auto &opt : options) {
    const bool isSelected = (_selectedOption == static_cast<int>(opt.option));
    const std::string text =
        isSelected ? (". " + opt.label + " .") : ("  " + opt.label + "  ");
    const float scale = isSelected ? 0.56f : 0.46f;
    const rtk::ecs::concepts::ColorRGBA8 color =
        isSelected ? rtk::ecs::concepts::ColorRGBA8{255, 235, 70, 255}
                   : rtk::ecs::concepts::ColorRGBA8{140, 150, 175, 180};

    addCenteredText(context, _dependency.window, _font, text, opt.y, color,
                    scale, _dynamicEntities);
  }

  const auto roomId = _dependency.game.getCurrentRoomId();
  const std::string roomText =
      roomId.has_value() ? "ROOM " + std::to_string(*roomId) : "OFFLINE";
  addCenteredText(context, _dependency.window, _font, roomText, 570.0f,
                  {120, 140, 180, 190}, 0.36f, _dynamicEntities);

  addCenteredText(context, _dependency.window, _font,
                  "UP DOWN TO NAVIGATE   ENTER TO SELECT   ESC TO RETURN",
                  640.0f, {110, 125, 155, 210}, 0.34f, _dynamicEntities);
}

std::optional<SceneType>
GameOverScene::onUpdate(rtk::EngineContext &context,
                        const rtk::InputState &inputState, float dt) {
  _elapsedTime += dt;

  if (_inputCooldown > 0.0f) {
    _inputCooldown -= dt;
  }

  if (_firstFrame) {
    _firstFrame = false;
    _previousUp = true;
    _previousDown = true;
    _previousConfirm = true;
    _previousEscape = true;
    return std::nullopt;
  }

  const bool isUpHeld =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Up) ||
      _dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Up) ||
      _dependency.game.isKeyPressed(rtk::ecs::concepts::Key::W) ||
      _dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Z);

  const bool isDownHeld =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Down) ||
      _dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Down) ||
      _dependency.game.isKeyPressed(rtk::ecs::concepts::Key::S);

  const bool isConfirmHeld =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Confirm) ||
      _dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Enter) ||
      _dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Space);

  const bool isEscapeHeld =
      rtype::Input::hasAction(inputState.actionMask,
                              rtype::Action::LeaveRoom) ||
      _dependency.game.isKeyPressed(rtk::ecs::concepts::Key::Escape);

  if (!isUpHeld && !isDownHeld) {
    _inputCooldown = 0.0f;
  }

  if (isUpHeld && (_inputCooldown <= 0.0f || !_previousUp)) {
    _selectedOption =
        (_selectedOption - 1 + static_cast<int>(GameOverOption::Count)) %
        static_cast<int>(GameOverOption::Count);
    _inputCooldown = NAV_COOLDOWN;
    _dependency.game.playSelectSound();
    updateVisuals(context);
  } else if (isDownHeld && (_inputCooldown <= 0.0f || !_previousDown)) {
    _selectedOption =
        (_selectedOption + 1) % static_cast<int>(GameOverOption::Count);
    _inputCooldown = NAV_COOLDOWN;
    _dependency.game.playSelectSound();
    updateVisuals(context);
  }

  const bool confirmTriggered = isConfirmHeld && !_previousConfirm;
  const bool escapeTriggered = isEscapeHeld && !_previousEscape;

  _previousUp = isUpHeld;
  _previousDown = isDownHeld;
  _previousConfirm = isConfirmHeld;
  _previousEscape = isEscapeHeld;

  if (escapeTriggered) {
    _dependency.game.setGameStarting(false);
    _dependency.game.clearNetworkEntities(context);
    if (_dependency.game.getCurrentRoomId().has_value()) {
      return SceneType::RoomLobby;
    }
    return SceneType::Menu;
  }

  if (confirmTriggered) {
    _dependency.game.playSelectSound();
    const auto option = static_cast<GameOverOption>(_selectedOption);
    switch (option) {
    case GameOverOption::ReturnToLobby:
      _dependency.game.setGameStarting(false);
      _dependency.game.clearNetworkEntities(context);
      if (_dependency.game.getCurrentRoomId().has_value()) {
        return SceneType::RoomLobby;
      }
      return SceneType::RoomList;

    case GameOverOption::RoomList:
      _dependency.game.setGameStarting(false);
      _dependency.game.requestLeaveRoom();
      _dependency.game.clearNetworkEntities(context);
      return SceneType::RoomList;

    case GameOverOption::MainMenu:
      _dependency.game.setGameStarting(false);
      _dependency.game.requestLeaveRoom();
      _dependency.game.clearNetworkEntities(context);
      return SceneType::Menu;

    default:
      break;
    }
  }

  return std::nullopt;
}

void GameOverScene::onExit(rtk::EngineContext &context) {
  context.eventBus.publish(rtype::events::StopMusicEvent{});
  _dependency.game.setGameStarting(false);
  _dependency.game.clearNetworkEntities(context);

  for (const auto entity : _staticEntities) {
    context.registry.remove_entity_from_all_pools(entity);
    context.registry.kill_entity(entity);
  }
  _staticEntities.clear();

  clearDynamicEntities(context);
  context.registry.flush();
}

} // namespace rtype::client
