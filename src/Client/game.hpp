#pragma once

#include <array>
#include <chrono>
#include <font/font.hpp>
#include <unordered_set>

#include "GameEngine.hpp"
#include "collision/collisionBuffer.hpp"
#include "inputAction/InputAction.hpp"
#include "inputAction/inputMapping.hpp"
#include "scene/clientDependency.hpp"

#include "factoryDataDriven/factoryDataDriven.hpp"
#include "scene/roomListScene.hpp"
#include "sceneManager/sceneManager.hpp"

#include "rtkAudiManagerAdapter.hpp"
#include "rtkWindowAdapter.hpp"

#include "ClientMetrics.hpp"
#include "NetworkClient.hpp"
#include "Protocol/RoomPayload.hpp"
#include "metrics/Metrics.hpp"
#include "scene/gameScene.hpp"
#include "scene/introScene.hpp"
#include "scene/menuScene.hpp"
#include "scene/networkConnectionScene.hpp"
#include "scene/roomLobbyScene.hpp"
#include "scene/settingScene.hpp"

#include "events/GameEvents.hpp"

namespace rtk::systems {
class CollisionFlashSystem;
class HudSystem;
} // namespace rtk::systems

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Client side game: window, inputs, scenes and replication of the
 * server world.
 */
class Game {
public:
  /**
   * @brief Opens the 1280x720 window and creates the menu scene.
   * @param host Server hostname or IP address.
   * @param port Server UDP port.
   */
  explicit Game(std::string host = "127.0.0.1", std::string port = "4242");
  ~Game() = default;

  /**
   * @brief Registers components, loads blueprints, binds inputs, adds the
   * systems and starts the network client.
   * @param context Engine services.
   */
  void onLoad(rtk::EngineContext &context);

  /**
   * @brief Tells the engine whether to keep running.
   * @return False once the window must close.
   */
  bool shouldContinue() const;

  /**
   * @brief Applies network events and replication messages, then updates the
   * active scene.
   * @param context Engine services.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void onUpdate(rtk::EngineContext &context, float dt);

  /**
   * @brief Destroys replicated entities, exits the scene and stops the
   * network client.
   * @param context Engine services.
   */
  void onUnload(rtk::EngineContext &context);

  /**
   * @brief Returns network statistics prepared for display.
   * @return Current network statistics.
   */
  const ClientNetworkStats &getNetworkStats() const;

  /**
   * @brief Returns the available rooms vector.
   * @return Current available rooms.
   */
  const std::vector<protocol::RoomInfoPayload> &getAvailableRooms() const;

  /**
   * @brief Returns the available rooms vector.
   * @return Current available rooms.
   */
  const std::optional<std::uint32_t> &getCurrentRoomId() const;

  /**
   * @brief Returns the players currently present in the room.
   * @return List of players with their connection IDs and ready states.
   */
  const std::vector<protocol::RoomPlayerState> &getRoomPlayers() const;

  /**
   * @brief Returns whether the local player is ready.
   * @return True if the server confirmed the player's ready state.
   */
  bool isLocalReady() const;

  /**
   * @brief Returns whether a ready-state request is pending.
   * @return True while waiting for a room state update.
   */
  bool isReadyRequestPending() const;

  /**
   * @brief Returns whether the server has started the current room.
   * @return True after a valid GameStarting message.
   */
  bool isGameStarting() const;

  /**
   * @brief Requests a ready-state change for the local player.
   * @param ready Desired ready state.
   * @return True if a request was initiated.
   */
  bool requestReady(bool ready);

  /**
   * @brief Requests to leave the current room.
   * @return True if a leave request was initiated.
   */
  bool requestLeaveRoom();

  /**
   * @brief Returns the font loaded in the window context.
   * @return return the loaded font.
   */
  const rtk::Font &getFont() const;

  /**
   * @brief Checks if a specific key is pressed in the window.
   * @param key Candidate key.
   * @return True if currently pressed.
   */
  [[nodiscard]] bool isKeyPressed(rtk::ecs::concepts::Key key) const {
    return _adapter.isKeyPressed(key);
  }

  /**
   * @brief Returns the first key currently pressed, if any.
   * @return Pressed key or nullopt.
   */
  [[nodiscard]] std::optional<rtk::ecs::concepts::Key> getAnyKeyPressed() const {
    return _adapter.getAnyKeyPressed();
  }

  /**
   * @brief Returns the mutable client input mapping.
   * @return Reference to the input mapping.
   */
  [[nodiscard]] rtk::Input::InputMapping &getInputMapping() noexcept {
    return _inputMapping;
  }

  /**
   * @brief Returns the const client input mapping.
   * @return Const reference to the input mapping.
   */
  [[nodiscard]] const rtk::Input::InputMapping &getInputMapping() const noexcept {
    return _inputMapping;
  }

  /**
   * @brief Resets input bindings to default controls.
   */
  void resetDefaultBindings() {
    createBindings();
  }

private:
  void registerBuilders(rtk::EngineContext &context);
  void registerComponents(rtk::EngineContext &context);
  void createBindings();
  void subscribeEvents(rtk::EngineContext &context, uint32_t shootSoundId);
  void addSystems(rtk::EngineContext &context);
  void loadEntityBluePrint(rtk::EngineContext &context);
  void handleReplication(rtk::EngineContext &context);
  void createReplicatedEntity(rtk::EngineContext &context,
                              const protocol::CreateEntityPayload &payload);
  void destroyReplicatedEntity(rtk::EngineContext &context,
                               const protocol::DestroyEntityPayload &payload);
  void applySnapshot(rtk::EngineContext &context,
                     const protocol::SnapshotPayload &payload);
  void updateNetworkStats();
  /**
   * @brief Destroys every replicated entity, the server does not send
   * DestroyEntity to a player who leaves the room.
   * @param context Engine services.
   */
  void clearNetworkEntities(rtk::EngineContext &context);

  std::unordered_map<protocol::NetworkEntityId, std::size_t> _networkEntities;
  std::unordered_set<std::uint32_t> _pendingEntityQueries;
  std::unordered_set<std::uint32_t> _destroyedEntities;
  std::array<std::uint16_t, 4> _playerArchetypes{};
  std::uint32_t _lastServerTick = 0;
  rtk::RenderWindow _window;
  RtkWindowAdapter _adapter;
  RtkAudioAdapter _adapterAudioManager;
  rtk::InputState _inputState{};
  std::uint32_t _previousActionMask = 0;
  rtk::collision::CollisionBuffer _collisionBuffer;
  rtk::Input::InputMapping _inputMapping;
  rtype::client::NetworkClient _networkClient;
  clientDependancy _dependency;
  rtk::SceneManager<SceneType, clientDependancy, rtk::InputState, IntroScene,
                    MenuScene, NetworkConnectionScene, RoomListScene,
                    RoomLobbyScene, GameScene, SettingScene>
      _sceneManager;

  rtk::Font _fontRtype;
  rtk::systems::CollisionFlashSystem *_collisionFlashSystem = nullptr;
  rtk::systems::HudSystem *_hudSystem = nullptr;
  bool _shouldQuit = false;
  std::string _host = "127.0.0.1";
  std::string _port = "4242";
  ClientNetworkStats _networkStats{};
  NetworkMetrics _previousNetworkMetrics{};
  std::chrono::steady_clock::time_point _lastNetworkMetricsTime{};
  bool _networkMetricsInitialized = false;
  std::vector<protocol::RoomInfoPayload> _availableRooms;
  std::optional<std::uint32_t> _currentRoomId;
  std::vector<protocol::RoomPlayerState> _roomPlayers;
  bool _localReady = false;
  bool _readyRequestPending = false;
  bool _gameStarting = false;
};

/** @} */
} // namespace rtype::client
