#include "roomListScene.hpp"
#include "../input/action.hpp"
#include "EngineContext.hpp"
#include "NetworkClient.hpp"
#include "components/Transform.hpp"
#include "game.hpp"
#include "scene/gameScene.hpp"
#include "scene/menuScene.hpp"
#include "scene/sceneType.hpp"
#include "systems/RenderConcept.hpp"
#include <optional>
#include <string>

namespace rtype::client {
RoomListScene::RoomListScene(clientDependancy &dep) : _dependency(dep) {}
void RoomListScene::onEnter(rtk::EngineContext &context) {
  _dependency.networkClient.sendListRoom();

  _logoTextureId =
      _dependency.window
          .loadTexture("assets/menu_assets/texture/R-Type_logo.png")
          .getHandle();

  _roomTextureId = _dependency.window
                       .loadTexture("assets/menu_assets/texture/rooms_text.png")
                       .getHandle();

  _refreshTextureId =
      _dependency.window
          .loadTexture("assets/menu_assets/texture/refresh_text.png")
          .getHandle();

  _createRoomTextureId =
      _dependency.window
          .loadTexture("assets/menu_assets/texture/create_room.png")
          .getHandle();

  constexpr float screenWidth = 1280.0f;
  constexpr float gapX = 150.0f;
  constexpr float sharedY = 150.0f;

  _createRoomEntity = context.registry.spawn_entity();
  component::Transform createRoomTransform{};
  createRoomTransform.scale = {0.15f, 0.15f};
  createRoomTransform.rotation = 0.f;

  float createRoomActualWidth = 2064.0f * 0.15f;
  createRoomTransform.position.x = 309.225f;
  createRoomTransform.position.y = sharedY;

  rtk::ecs::concepts::SpriteData createRoomSprite{};
  createRoomSprite.size = {2064.0f / 2.0f, 512.0f / 2.0f};
  createRoomSprite.origin = {0.f, 0.f};
  createRoomSprite.textureRect = {0, 0, 2064, 512};
  createRoomSprite.color = {255, 255, 255, 255};
  createRoomSprite.textureId = _createRoomTextureId;

  context.registry.get_components<component::Transform>().insert_at(
      _createRoomEntity, createRoomTransform);
  context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
      _createRoomEntity, createRoomSprite);
  _entities.push_back(_createRoomEntity);

  const auto roomEntity = context.registry.spawn_entity();
  component::Transform roomTransform{};
  roomTransform.scale = {0.15f, 0.15f};
  roomTransform.rotation = 0.f;

  float roomActualWidth = 1651.0f * 0.15f;
  roomTransform.position.x = 516.175f;
  roomTransform.position.y = 80.0f;

  rtk::ecs::concepts::SpriteData roomSprite{};
  roomSprite.size = {1651.0f, 512.0f};
  roomSprite.origin = {0.f, 0.f};
  roomSprite.textureRect = {0, 0, 1651, 512};
  roomSprite.color = {255, 255, 255, 255};
  roomSprite.textureId = _roomTextureId;

  context.registry.get_components<component::Transform>().insert_at(
      roomEntity, roomTransform);
  context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
      roomEntity, roomSprite);
  _entities.push_back(roomEntity);

  _refreshEntity = context.registry.spawn_entity();
  component::Transform refreshTransform{};
  refreshTransform.scale = {0.15f, 0.15f};
  refreshTransform.rotation = 0.f;

  refreshTransform.position.x = 404.225f + 309.6f + gapX;
  refreshTransform.position.y = sharedY - 10.0f;

  rtk::ecs::concepts::SpriteData refreshSprite{};
  refreshSprite.size = {400.0f, 512.0f};
  refreshSprite.origin = {0.f, 0.f};
  refreshSprite.textureRect = {0, 0, 413, 512};
  refreshSprite.color = {255, 255, 255, 255};
  refreshSprite.textureId = _refreshTextureId;

  context.registry.get_components<component::Transform>().insert_at(
      _refreshEntity, refreshTransform);
  context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
      _refreshEntity, refreshSprite);
  _entities.push_back(_refreshEntity);

  const auto logoEntity = context.registry.spawn_entity();

  component::Transform logoTransform{};
  logoTransform.position = {50.f, 50.f};
  logoTransform.rotation = 0.f;
  logoTransform.scale = {0.4f, 0.4f};

  rtk::ecs::concepts::SpriteData logoSprite{};
  logoSprite.size = {1280.f, 720.f};
  logoSprite.origin = {0.f, 0.f};
  logoSprite.textureRect = {0, 0, 1280, 720};
  logoSprite.color = {255, 255, 255, 255};
  logoSprite.textureId = _logoTextureId;

  context.registry.get_components<component::Transform>().insert_at(
      logoEntity, logoTransform);
  context.registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
      logoEntity, logoSprite);
  _entities.push_back(logoEntity);

  ButtonInit buttonInits[2] = {
      {&_refreshEntity, _refreshTextureId, 242.f, 65.f, 1600, 430},
      {&_createRoomEntity, _createRoomTextureId, 377.f, 65.f, 2470, 426},
  };
  _previousActionMask = 0;
}

void RoomListScene::updateSelectionVisuals(rtk::EngineContext &context) {
  auto &transforms = context.registry.get_components<component::Transform>();
  auto &sprites =
      context.registry.get_components<rtk::ecs::concepts::SpriteData>();

  std::size_t buttons[2] = {_createRoomEntity, _refreshEntity};

  for (int i = 0; i < 2; ++i) {
    auto *transform = transforms.get(buttons[i]);
    auto *sprite = sprites.get(buttons[i]);

    if (!transform || !sprite)
      continue;

    const bool isSelected = (i == _selectedOption);
    const float baseScale = 0.15f;
    const float scaleFactor = isSelected ? 1.08f : 1.0f;

    transform->scale = {baseScale * scaleFactor, baseScale * scaleFactor};
    sprite->color = isSelected
                        ? rtk::ecs::concepts::ColorRGBA8{255, 255, 255, 255}
                        : rtk::ecs::concepts::ColorRGBA8{120, 120, 120, 180};
  }
}

std::optional<SceneType>
RoomListScene::onUpdate(rtk::EngineContext &context,
                        const rtk::InputState &inputState, float dt) {
  const bool upPressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Up) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::Up);

  const bool downPressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Down) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::Down);

  const bool confirmPressed =
      rtype::Input::hasAction(inputState.actionMask, rtype::Action::Confirm) &&
      !rtype::Input::hasAction(_previousActionMask, rtype::Action::Confirm);

  _previousActionMask = inputState.actionMask;

  if (upPressed) {
    _selectedOption = (_selectedOption - 1 +
                       (_dependency.game.getAvailableRooms().size() + 2)) %
                      (2 + _dependency.game.getAvailableRooms().size());
  } else if (downPressed) {
    _selectedOption = (_selectedOption + 1) %
                      (2 + _dependency.game.getAvailableRooms().size());
  }

  if (confirmPressed) {
    if (_selectedOption == 0) {
      _dependency.networkClient.sendCreateRoom(
          "Room : " +
          std::to_string(_dependency.game.getAvailableRooms().size() + 1));
    } else if (_selectedOption == 1) {
      _dependency.networkClient.sendListRoom();
    } else if (_selectedOption >= 2) {
      _dependency.networkClient.sendJoinRoom(
          _dependency.game.getAvailableRooms()[(_selectedOption - 2)].roomId);
    }
  }
  if (_dependency.game.getCurrentRoomId().has_value()) {
    return std::nullopt;
  }
  updateSelectionVisuals(context);
  return std::nullopt;
}

void RoomListScene::onExit(rtk::EngineContext &context) {
  for (const auto entity : _entities) {
    context.registry.kill_entity(entity);
  }
  context.registry.flush();
  _entities.clear();
}

}; // namespace rtype::client