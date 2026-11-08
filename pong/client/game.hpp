#pragma once

#include <asio.hpp>
#include <memory>
#include <string>

#include "GameEngine.hpp"
#include "audio/DynamicLibrary.hpp"
#include "audio/IAudioEngine.hpp"
#include "collision/collisionBuffer.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "inputAction/inputMapping.hpp"
#include "sceneManager/sceneManager.hpp"

#include "protocol/PongProtocol.hpp"
#include "rtkWindowAdapter.hpp"
#include "scene/gameScene.hpp"
#include "scene/menuScene.hpp"
#include "scene/sceneType.hpp"
#include "systems/ParticleManager.hpp"

namespace pong::systems {
class PongBallSystem;
class PongPaddleSystem;
}

namespace pong::client {

/**
 * @brief Main Pong game client implementing the rtk::Game concept.
 */
class Game {
public:
  /**
   * @brief Opens the 1280x720 window and connects to server if available.
   * @param host Server hostname or IP (default: 127.0.0.1).
   * @param port Server port (default: 4242).
   */
  explicit Game(std::string host = "127.0.0.1", std::string port = "4242");
  ~Game();

  /**
   * @brief Registers components, builders, systems, and connects to server.
   * @param context Engine services.
   */
  void onLoad(rtk::EngineContext &context);

  /**
   * @brief Checks if the game window should keep running.
   * @return True if running.
   */
  [[nodiscard]] bool shouldContinue() const;

  /**
   * @brief Updates network synchronization, scenes, and handles inputs.
   * @param context Engine services.
   * @param dt Elapsed frame time in seconds.
   */
  void onUpdate(rtk::EngineContext &context, float dt);

  /**
   * @brief Tears down resources, socket, and scenes.
   * @param context Engine services.
   */
  void onUnload(rtk::EngineContext &context);

private:
  void registerComponents(rtk::EngineContext &context);
  void registerBuilders(rtk::EngineContext &context);
  void loadBlueprints(rtk::EngineContext &context);
  void createBindings();
  void addSystems(rtk::EngineContext &context);

  void pollServer(rtk::EngineContext &context);
  void sendClientInput();
  void applySnapshot(rtk::EngineContext &context,
                     const pong::protocol::ServerSnapshotMsg &snap);

  void loadAudioPlugin(const std::string &libPath);
  void switchAudioPlugin();

  std::string _host;
  std::string _port;
  asio::io_context _ioContext;
  std::unique_ptr<asio::ip::udp::socket> _socket;
  asio::ip::udp::endpoint _serverEndpoint;
  bool _connectedToServer = false;
  std::uint8_t _playerSlot = 0; // 0 = Left (P1), 1 = Right (P2)
  float _reconnectTimer = 0.0f;
  std::uint8_t _latestScoreP1 = 0;
  std::uint8_t _latestScoreP2 = 0;

  rtk::RenderWindow _window;
  RtkWindowAdapter _adapter;
  rtk::InputState _inputState{};
  rtk::collision::CollisionBuffer _collisionBuffer;
  rtk::Input::InputMapping _inputMapping;

  rtk::SceneManager<PongSceneType, rtk::RenderWindow, rtk::InputState,
                    PongMenuScene, PongGameScene>
      _sceneManager;

  pong::systems::PongBallSystem *_ballSystem = nullptr;
  pong::systems::PongPaddleSystem *_paddleSystem = nullptr;
  rtk::systems::ParticleManager *_particleManager = nullptr;
  float _lastBallX = 635.0f;
  float _lastBallY = 355.0f;
  bool _hasLastBallPos = false;
  bool _matchStarted = false;
  bool _shouldQuit = false;

  rtk::audio::DynamicLibrary _audioLib{};
  rtk::audio::IAudioEngine *_audioEngine = nullptr;
  DestroyAudioEngineFn _destroyAudioFn = nullptr;
  std::size_t _activePluginIndex = 0;
  std::size_t _bounceSoundId = static_cast<std::size_t>(-1);
  std::size_t _scoreSoundId = static_cast<std::size_t>(-1);
  std::uint32_t _previousActionMask = 0;
};

} // namespace pong::client
