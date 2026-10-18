#pragma once

#include <array>
#include <unordered_set>

#include "GameEngine.hpp"
#include "collision/collisionBuffer.hpp"
#include "inputAction/InputAction.hpp"
#include "inputAction/inputMapping.hpp"

#include "factoryDataDriven/factoryDataDriven.hpp"
#include "sceneManager/sceneManager.hpp"

#include "rtkAudiManagerAdapter.hpp"
#include "rtkWindowAdapter.hpp"

#include "NetworkClient.hpp"
#include "scene/gameScene.hpp"
#include "scene/introScene.hpp"
#include "scene/menuScene.hpp"
#include "scene/networkConnectionScene.hpp"

namespace rtype::events {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Published when the local player releases the shoot button.
 */
struct PlayerShotEvent {};

/**
 * @brief Published when a quit request is triggered from the menu or game.
 */
struct QuitGameEvent {};

/** @} */
} // namespace rtype::events

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

  std::unordered_map<protocol::NetworkEntityId, std::size_t> _networkEntities;
  std::unordered_set<std::uint32_t> _pendingEntityQueries;
  std::unordered_set<std::uint32_t> _destroyedEntities;
  std::array<std::uint16_t, 4> _playerArchetypes{};
  std::uint32_t _lastServerTick = 0;
  rtk::RenderWindow _window;
  RtkWindowAdapter _adapter;
  RtkAudioAdapter _adapterAudioManager;
  rtk::InputState _inputState{};
  rtk::collision::CollisionBuffer _collisionBuffer;
  rtk::Input::InputMapping _inputMapping;
  rtype::client::NetworkClient _networkClient;

  rtk::SceneManager<SceneType, rtk::RenderWindow, rtk::InputState, IntroScene,
                    MenuScene, NetworkConnectionScene, GameScene>
      _sceneManager;

  rtk::Font _fontRtype;
  bool _shouldQuit = false;
  std::string _host = "127.0.0.1";
  std::string _port = "4242";
};

/** @} */
} // namespace rtype::client
