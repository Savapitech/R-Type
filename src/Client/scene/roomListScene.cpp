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
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <font/font.hpp>
#include <optional>
#include <string>
#include <vector>

namespace rtype::client {
RoomListScene::RoomListScene(clientDependancy &dep) : _dependency(dep) {}
void RoomListScene::onEnter(rtk::EngineContext &context) {
  _dependency.networkClient.sendListRoom();

  _font = _dependency.game.getFont();
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

  std::vector<size_t> allSelectable;
  allSelectable.push_back(_createRoomEntity);
  allSelectable.push_back(_refreshEntity);
  if (_roomsDynamicEntities.size() > 0) {
    for (uint32_t i = 0U; i < _roomsDynamicEntities.size(); i++) {
      allSelectable.push_back(_roomsDynamicEntities[i]);
    }
  }
  for (int i = 0; i < allSelectable.size(); ++i) {
    auto *transform = transforms.get(allSelectable[i]);
    auto *sprite = sprites.get(allSelectable[i]);

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

  const int optionCount =
      static_cast<int>(_dependency.game.getAvailableRooms().size()) + 2;
  if (_selectedOption >= optionCount) {
    _selectedOption = optionCount - 1;
  }

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
    return SceneType::RoomLobby;
  }
  updateSelectionVisuals(context);
  const auto &availableRooms = _dependency.game.getAvailableRooms();
  const bool sameRooms = std::equal(
      availableRooms.begin(), availableRooms.end(), _displayedRoomIds.begin(),
      _displayedRoomIds.end(),
      [](const auto &room, std::uint32_t id) { return room.roomId == id; });
  if (!sameRooms) {
    _displayedRoomIds.clear();
    for (const auto &room : availableRooms) {
      _displayedRoomIds.push_back(room.roomId);
    }
    for (uint16_t i = 0U; i < _roomsDynamicEntities.size(); i++) {
      context.registry.kill_entity(_roomsDynamicEntities[i]);
    }
    for (uint16_t i = 0U; i < _textDynamicEntites.size(); i++) {
      context.registry.kill_entity(_textDynamicEntites[i]);
    }
    _textDynamicEntites.clear();
    _roomsDynamicEntities.clear();

    float startY = 200.0f;
    for (uint32_t i = 0U; i < _dependency.game.getAvailableRooms().size();
         i++) {
      auto room = _dependency.game.getAvailableRooms()[i];
      std::string roomName = "Room " + std::to_string(room.roomId);
      rtk::Text text = _dependency.window.createText(
          roomName, _font, {260.0f, startY + (i * 70.0f) + 5.0f},
          {255, 255, 255});
      auto entity = context.registry.spawn_entity();
      rtk::ecs::concepts::SpriteData roomIdxSprite{};
      roomIdxSprite.textureId = _roomTextureId;
      roomIdxSprite.scale = {0.10f, 0.10f};
      roomIdxSprite.size = {1651.0f, 512.0f};
      roomIdxSprite.color = {255, 255, 255, 255};
      roomIdxSprite.textureRect = {0, 0, 1651, 512};
      component::Transform roomIdxTransform{};
      roomIdxTransform.position.x = 260.0f + 300.0f;
      roomIdxTransform.position.y = startY + (i * 70.0f);
      roomIdxTransform.scale = {0.10f, 0.10f};
      context.registry.get_components<component::Transform>().insert_at(
          entity, roomIdxTransform);
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .insert_at(entity, roomIdxSprite);
      _roomsDynamicEntities.push_back(entity);
      for (uint16_t k = 0U; k < text._glyphs.size(); k++) {
        auto letterEntity = context.registry.spawn_entity();

        rtk::ecs::concepts::SpriteData textSprite{};
        textSprite.textureId = text.getSprites()[k].textureId;
        textSprite.textureRect.height = text.getSprites()[k].textureRect.height;
        textSprite.textureRect.width = text.getSprites()[k].textureRect.width;
        textSprite.textureRect.x = text.getSprites()[k].textureRect.left;
        textSprite.textureRect.y = text.getSprites()[k].textureRect.top;
        textSprite.color.a = text.getSprites()[k].color.a;
        textSprite.color.b = text.getSprites()[k].color.b;
        textSprite.color.g = text.getSprites()[k].color.g;
        textSprite.color.r = text.getSprites()[k].color.r;
        textSprite.size.x = text.getSprites()[k].size.x;
        textSprite.size.y = text.getSprites()[k].size.y;
        textSprite.origin.x = text.getSprites()[k].origin.x;
        textSprite.origin.y = text.getSprites()[k].origin.y;
        textSprite.scale.x = text.getSprites()[k].scale.x;
        textSprite.scale.y = text.getSprites()[k].scale.y;

        float baseY = startY + (i * 110.0f) - 150.0f;
        component::Transform textTransform{};
        textTransform.position.x = text.getSprites()[k].position.x;
        textTransform.position.y = startY + (i * 70.0f) + 20.0f;
        textTransform.scale.x = text.getSprites()[k].scale.x;
        textTransform.scale.y = text.getSprites()[k].scale.y;
        textTransform.rotation = 0.0f;

        context.registry.get_components<component::Transform>().insert_at(
            letterEntity, textTransform);
        context.registry.get_components<rtk::ecs::concepts::SpriteData>()
            .insert_at(letterEntity, textSprite);
        _textDynamicEntites.push_back(letterEntity);
      }
    }
  }
  return std::nullopt;
}

void RoomListScene::onExit(rtk::EngineContext &context) {
  for (const auto entity : _entities) {
    context.registry.kill_entity(entity);
  }
  for (const auto entity : _roomsDynamicEntities) {
    context.registry.kill_entity(entity);
  }
  for (const auto entity : _textDynamicEntites) {
    context.registry.kill_entity(entity);
  }
  _roomsDynamicEntities.clear();
  _textDynamicEntites.clear();
  context.registry.flush();
  _entities.clear();
}

}; // namespace rtype::client
