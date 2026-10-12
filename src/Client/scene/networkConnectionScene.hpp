#pragma once

#include <iostream>
#include <optional>

#include "EngineContext.hpp"
#include "events/NetworkEvent.hpp"
#include "protocol/HandshakeHeader.hpp"
#include "protocol/PacketWriter.hpp"

#include "scene/sceneType.hpp"


namespace rtype::client {

class NetworkConnectionScene {
private:
    float _timeoutTimer = 0.0f;
    float _retryTimer = 0.0f;
    bool _isConnected = false;

public:
    static constexpr SceneType Type = SceneType::Connection;

    NetworkConnectionScene(rtk::RenderWindow&) {}

    void onEnter(rtk::EngineContext &context) {
        context.eventBus.subscribe<rtype::events::ConnectedToServerEvent>(
            [this](const rtype::events::ConnectedToServerEvent&) {
                LOG_INFO("Handshake success connection");
                _isConnected = true;
            }
        );

        context.eventBus.subscribe<rtype::events::ConnectionRejectedEvent>(
            [](const rtype::events::ConnectionRejectedEvent&) {
                LOG_ERROR("Connection refused by the server");
            }
        );
    }

    std::optional<SceneType> onUpdate(rtk::EngineContext &context, const rtk::InputState &, float dt)
    {
        if (_isConnected) {
            return SceneType::Game;
        }

        _retryTimer += dt;
        if (_retryTimer >= 1.0f) {
            _retryTimer = 0.0f;

            PacketWriter writer;
            HandshakeHeader helloHeader;
            helloHeader.type = HandshakeType::Hello;

            if (helloHeader.serialize(writer)) {
                LOG_INFO("Send handshake hello");

                context.eventBus.publish(rtype::events::SendPacketEvent{writer.getData()});
            }
        }

        _timeoutTimer += dt;
        if (_timeoutTimer >= 10.0f) {
            LOG_ERROR("Timeout: cannot join the server.");
            return SceneType::Menu;
        }

        return std::nullopt;
    }

    void onExit(rtk::EngineContext &) {
    }
};

} // namespace rtype::client