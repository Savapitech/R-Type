#include "roomLobbyScene.hpp"

#include <string>
#include <utility>
#include <vector>

#include "components/Transform.hpp"
#include "game.hpp"
#include "input/action.hpp"
#include "systems/RenderConcept.hpp"

namespace rtype::client {

namespace {

void addText(
    rtk::EngineContext &context,
    rtk::RenderWindow &window,
    const rtk::Font &font,
    const std::string &content,
    float x,
    float y,
    const rtk::ecs::concepts::ColorRGBA8 &color,
    std::vector<std::size_t> &entities)
{
    const rtk::Text text = window.createText(
        content,
        font,
        {x, y},
        {color.r, color.g, color.b}
    );

    for (const auto &glyph : text.getSprites()) {
        const auto entity = context.registry.spawn_entity();

        rtk::ecs::concepts::SpriteData sprite{};

        sprite.textureId = glyph.textureId;

        sprite.textureRect.x = glyph.textureRect.left;
        sprite.textureRect.y = glyph.textureRect.top;
        sprite.textureRect.width = glyph.textureRect.width;
        sprite.textureRect.height = glyph.textureRect.height;

        sprite.color = {
            glyph.color.r,
            glyph.color.g,
            glyph.color.b,
            glyph.color.a
        };

        sprite.size = {
            glyph.size.x,
            glyph.size.y
        };

        sprite.origin = {
            glyph.origin.x,
            glyph.origin.y
        };

        sprite.scale = {
            glyph.scale.x,
            glyph.scale.y
        };

        component::Transform transform{};

        transform.position = {
            glyph.position.x,
            glyph.position.y
        };

        transform.scale = {
            glyph.scale.x,
            glyph.scale.y
        };

        transform.rotation = 0.0f;

        context.registry
            .get_components<component::Transform>()
            .insert_at(entity, transform);

        context.registry
            .get_components<rtk::ecs::concepts::SpriteData>()
            .insert_at(entity, sprite);

        entities.push_back(entity);
    }
}

} // namespace

RoomLobbyScene::RoomLobbyScene(clientDependancy &dep)
    : _dependency(dep)
{
}

void RoomLobbyScene::onEnter(rtk::EngineContext &context)
{
    _font = _dependency.game.getFont();

    _previousActionMask = 0;
    _leaveRequested = false;
    _displaySignature.clear();

    _entities.clear();
    _playerEntities.clear();

    updatePlayerDisplay(context);
}

std::optional<SceneType> RoomLobbyScene::onUpdate(
    rtk::EngineContext &context,
    const rtk::InputState &inputState,
    float dt)
{
    (void)dt;
    auto &game = _dependency.game;

    if (_leaveRequested) {
        if (!game.getCurrentRoomId().has_value()) {
            return SceneType::RoomList;
        }
        updatePlayerDisplay(context);
        return std::nullopt;
    }
    if (game.isGameStarting()) {
        return SceneType::Game;
    }
    if (!game.getCurrentRoomId().has_value()) {
        return SceneType::RoomList;
    }
    const bool readyPressed =
        rtype::Input::hasAction(
            inputState.actionMask,
            rtype::Action::Ready
        ) &&
        !rtype::Input::hasAction(
            _previousActionMask,
            rtype::Action::Ready
        );

    const bool leavePressed =
        rtype::Input::hasAction(
            inputState.actionMask,
            rtype::Action::LeaveRoom
        ) &&
        !rtype::Input::hasAction(
            _previousActionMask,
            rtype::Action::LeaveRoom
        );

    _previousActionMask = inputState.actionMask;

    if (readyPressed && !game.isReadyRequestPending()) {
        game.requestReady(!game.isLocalReady());
    }

    if (leavePressed) {
        if (game.requestLeaveRoom()) {
            _leaveRequested = true;
        }
    }

    updatePlayerDisplay(context);

    return std::nullopt;
}

void RoomLobbyScene::updatePlayerDisplay(
    rtk::EngineContext &context)
{
    const auto &game = _dependency.game;
    const auto &players = game.getRoomPlayers();
    std::string signature;

    if (game.getCurrentRoomId().has_value()) {
        signature += "Room ";
        signature += std::to_string(
            *game.getCurrentRoomId()
        );
    }
    for (const auto &player : players) {
        signature += "|";
        signature += std::to_string(player.connectionId);
        signature += player.ready ? ":1" : ":0";
    }
    signature += game.isLocalReady() ? "|local:1" : "|local:0";
    signature += game.isReadyRequestPending()
        ? "|pending:1"
        : "|pending:0";
    signature += _leaveRequested
        ? "|leaving:1"
        : "|leaving:0";
    if (signature == _displaySignature) {
        return;
    }
    _displaySignature = std::move(signature);
    for (const auto entity : _playerEntities) {
        context.registry.kill_entity(entity);
    }

    _playerEntities.clear();

    const rtk::ecs::concepts::ColorRGBA8 white{
        255, 255, 255, 255
    };

    const rtk::ecs::concepts::ColorRGBA8 green{
        80, 255, 120, 255
    };

    const rtk::ecs::concepts::ColorRGBA8 red{
        255, 100, 100, 255
    };

    const rtk::ecs::concepts::ColorRGBA8 yellow{
        255, 220, 90, 255
    };
    addText(
        context,
        _dependency.window,
        _font,
        "ROOM LOBBY",
        420.0f,
        70.0f,
        white,
        _playerEntities
    );
    if (game.getCurrentRoomId().has_value()) {
        addText(
            context,
            _dependency.window,
            _font,
            "Room " + std::to_string(
                *game.getCurrentRoomId()
            ),
            420.0f,
            140.0f,
            white,
            _playerEntities
        );
    }
    float playerY = 220.0f;
    for (const auto &player : players) {
        addText(
            context,
            _dependency.window,
            _font,
            "Player " + std::to_string(
                player.connectionId
            ),
            300.0f,
            playerY,
            white,
            _playerEntities
        );

        addText(
            context,
            _dependency.window,
            _font,
            player.ready ? "READY" : "NOT READY",
            750.0f,
            playerY,
            player.ready ? green : red,
            _playerEntities
        );

        playerY += 65.0f;
    }
    addText(
        context,
        _dependency.window,
        _font,
        "ESC TO LEAVE ROOM",
        360.0f,
        600.0f,
        white,
        _playerEntities
    );
    std::string status = "Waiting for players...";
    if (_leaveRequested) {
        status = "Leaving room...";
    } else if (game.isReadyRequestPending()) {
        status = "Updating ready state...";
    } else if (game.isLocalReady()) {
        status = "You are READY";
    } else {
        status = "Press R to get READY";
    }

    addText(
        context,
        _dependency.window,
        _font,
        status,
        360.0f,
        660.0f,
        yellow,
        _playerEntities
    );
}

void RoomLobbyScene::onExit(rtk::EngineContext &context)
{
    for (const auto entity : _entities) {
        context.registry.kill_entity(entity);
    }

    for (const auto entity : _playerEntities) {
        context.registry.kill_entity(entity);
    }

    _entities.clear();
    _playerEntities.clear();

    _displaySignature.clear();
    _previousActionMask = 0;
    _leaveRequested = false;

    context.registry.flush();
}

} // namespace rtype::client
