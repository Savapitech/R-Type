#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
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

#include "rtkAudioManagerAdapter.hpp"
#include "rtkWindowAdapter.hpp"

#include "ClientMetrics.hpp"
#include "LocalPrediction.hpp"
#include "NetworkClient.hpp"
#include "NetworkDiagnostics.hpp"
#include "Protocol/PowerUpPayload.hpp"
#include "Protocol/RoomPayload.hpp"
#include "chat/ChatBox.hpp"
#include "metrics/Metrics.hpp"
#include "scene/gameOverScene.hpp"
#include "scene/gameScene.hpp"
#include "scene/introScene.hpp"
#include "scene/menuScene.hpp"
#include "scene/networkConnectionScene.hpp"
#include "scene/roomLobbyScene.hpp"
#include "scene/scoreboardScene.hpp"
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
   * @param username Name the stats are saved under, empty to not save them.
   */
  explicit Game(std::string host = "127.0.0.1", std::string port = "4242",
                std::string username = "");
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
   * @brief Sets whether the current room game has started.
   * @param starting Game starting status.
   */
  void setGameStarting(bool starting);

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
   * @brief Returns the last scoreboard received, as plain text lines.
   * @return Scoreboard text, empty until the server answered.
   */
  const std::string &getScoreboardText() const { return _scoreboardText; }

  /**
   * @brief Clears the last scoreboard and asks the server for a new one.
   * @return False if not connected: nothing was sent.
   */
  bool requestScoreboard();

  /**
   * @brief Returns the score of the local player in the current round.
   * @return Points earned since the round started.
   */
  std::uint32_t getRoundScore() const { return _roundScore; }

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
    if (_inputState.textInputActive)
      return false;
    return _adapter.isKeyPressed(key);
  }

  /**
   * @brief Returns the first key currently pressed, if any.
   * @return Pressed key or nullopt.
   */
  [[nodiscard]] std::optional<rtk::ecs::concepts::Key>
  getAnyKeyPressed() const {
    if (_inputState.textInputActive)
      return std::nullopt;
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
  [[nodiscard]] const rtk::Input::InputMapping &
  getInputMapping() const noexcept {
    return _inputMapping;
  }

  /**
   * @brief Resets input bindings to default controls.
   */
  void resetDefaultBindings() { createBindings(); }

  /**
   * @brief Plays the UI / shoot selection sound effect.
   */
  void playSelectSound() {
    if (_shootSoundId != 0) {
      _adapterAudioManager.playSound(_shootSoundId);
    }
  }

  /**
   * @brief Plays a sound by its identifier.
   */
  void playSound(size_t soundId) { _adapterAudioManager.playSound(soundId); }

  /**
   * @brief Stops a specific dedicated sound by its identifier.
   */
  void stopSound(size_t soundId) { _adapterAudioManager.stopSound(soundId); }

  /**
   * @brief Checks if a specific sound is currently playing.
   */
  [[nodiscard]] bool isSoundPlaying(size_t soundId) const {
    return _adapterAudioManager.isSoundPlaying(soundId);
  }

  /**
   * @brief Configures looping for a specific sound.
   */
  void setSoundLooping(size_t soundId, bool loop) {
    _adapterAudioManager.setSoundLooping(soundId, loop);
  }

  /**
   * @brief Sets volume of a specific sound.
   */
  void setSoundVolume(size_t soundId, float volume) {
    _adapterAudioManager.setSoundVolume(soundId, volume);
  }

  /**
   * @brief Starts dedicated background music (looping by default).
   */
  void playMusic(const std::string &path, bool loop = true) {
    _adapterAudioManager.playMusic(path, loop);
  }

  /**
   * @brief Stops current background music immediately.
   */
  void stopMusic() { _adapterAudioManager.stopMusic(); }

  /**
   * @brief Pauses background music.
   */
  void pauseMusic() { _adapterAudioManager.pauseMusic(); }

  /**
   * @brief Resumes paused background music.
   */
  void resumeMusic() { _adapterAudioManager.resumeMusic(); }

  /**
   * @brief Checks if background music is currently playing.
   */
  [[nodiscard]] bool isMusicPlaying() const {
    return _adapterAudioManager.isMusicPlaying();
  }

  /**
   * @brief Stops all active sounds and music.
   */
  void stopAllSounds() { _adapterAudioManager.stopAllSounds(); }

  /**
   * @brief Sets whether all sound output is muted.
   */
  void setMuted(bool muted) { _adapterAudioManager.setMuted(muted); }

  /**
   * @brief Returns whether audio is currently muted.
   */
  [[nodiscard]] bool isMuted() const noexcept {
    return _adapterAudioManager.isMuted();
  }

  /**
   * @brief Toggles mute state of all audio output.
   * @return New mute state (true if muted).
   */
  bool toggleMute() { return _adapterAudioManager.toggleMute(); }

  /**
   * @brief Returns the music volume, from 0.0 to 1.0.
   */
  [[nodiscard]] float getMusicVolume() const noexcept {
    return _adapterAudioManager.getMusicVolume();
  }

  /**
   * @brief Returns the sound effects volume, from 0.0 to 1.0.
   */
  [[nodiscard]] float getEffectsVolume() const noexcept {
    return _adapterAudioManager.getEffectsVolume();
  }

  /**
   * @brief Lowers the music volume by a quarter, wrapping from 0 to 100%.
   */
  void cycleMusicVolume() {
    _adapterAudioManager.setMusicVolume(
        nextVolumeStep(_adapterAudioManager.getMusicVolume()));
  }

  /**
   * @brief Lowers the effects volume by a quarter, wrapping from 0 to 100%.
   */
  void cycleEffectsVolume() {
    _adapterAudioManager.setEffectsVolume(
        nextVolumeStep(_adapterAudioManager.getEffectsVolume()));
  }

  /**
   * @brief Destroys every replicated entity, the server does not send
   * DestroyEntity to a player who leaves the room.
   * @param context Engine services.
   */
  void clearNetworkEntities(rtk::EngineContext &context);

private:
  static float nextVolumeStep(float volume) {
    constexpr float STEP = 0.25f;
    if (volume <= 0.001f)
      return 1.0f;
    return std::max(0.0f, std::floor((volume - 0.001f) / STEP) * STEP);
  }

  void registerBuilders(rtk::EngineContext &context);
  void registerComponents(rtk::EngineContext &context);
  void createBindings();
  void subscribeEvents(rtk::EngineContext &context, uint32_t shootSoundId);
  void addSystems(rtk::EngineContext &context);
  void loadEntityBluePrint(rtk::EngineContext &context);
  void updateNetworkStats();
  void updateChat(rtk::EngineContext &context, float dt);
  /**
   * @brief Applies a power up the server gave to the local ship.
   * @param context Engine services.
   * @param payload Kind and strength of the power up.
   */
  void applyPowerUp(rtk::EngineContext &context,
                    const protocol::PowerUpTakenPayload &payload);

  std::unordered_map<protocol::NetworkEntityId, std::size_t> _networkEntities;
  std::unordered_set<std::uint32_t> _pendingEntityQueries;
  std::unordered_set<std::uint32_t> _destroyedEntities;
  std::array<std::uint16_t, 4> _playerArchetypes{};
  std::uint32_t _lastServerTick = 0;
  rtk::RenderWindow _window;
  RtkWindowAdapter _adapter;
  RtkAudioAdapter _adapterAudioManager;
  std::uint32_t _shootSoundId = 0;
  rtk::InputState _inputState{};
  std::uint32_t _previousActionMask = 0;
  rtk::collision::CollisionBuffer _collisionBuffer;
  rtk::Input::InputMapping _inputMapping;
  rtype::client::NetworkClient _networkClient;
  /// Predicted ship of the local player, moved by PlayerSystem and corrected
  /// by every Snapshot.
  rtype::client::LocalPrediction _prediction;
  rtype::client::NetworkDiagnostics _networkDiagnostics;
  ChatBox _chat;
  clientDependancy _dependency;
  rtk::SceneManager<SceneType, clientDependancy, rtk::InputState, IntroScene,
                    MenuScene, NetworkConnectionScene, RoomListScene,
                    RoomLobbyScene, GameScene, SettingScene, GameOverScene,
                    ScoreboardScene>
      _sceneManager;

  rtk::Font _fontRtype;
  rtk::systems::CollisionFlashSystem *_collisionFlashSystem = nullptr;
  rtk::systems::HudSystem *_hudSystem = nullptr;
  bool _shouldQuit = false;
  bool _previousMuteKey = false;
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
  std::string _username;
  std::string _scoreboardText;
  std::uint32_t _roundScore = 0;
};

/** @} */
} // namespace rtype::client
