#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "graphical/renderWindow.hpp"
#include "sceneManager/sceneConcept.hpp"
#include "sceneType.hpp"

namespace pong::client {

/**
 * @brief Main menu title screen for Pong.
 */
class PongMenuScene {
public:
  static constexpr PongSceneType Type = PongSceneType::Menu;

  /**
   * @brief Constructs the menu scene.
   * @param window Target render window.
   */
  explicit PongMenuScene(rtk::RenderWindow &window);
  ~PongMenuScene() = default;

  /**
   * @brief Spawns title and start prompt sprites.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Polls inputs to switch to Game scene on Space or Enter.
   * @param context Engine services.
   * @param input Current frame input state.
   * @param dt Elapsed frame time.
   * @return Next scene identifier or nullopt.
   */
  std::optional<PongSceneType> onUpdate(rtk::EngineContext &context,
                                        const rtk::InputState &input, float dt);

  /**
   * @brief Destroys all menu entities.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

  void setMatchStarted(bool started) noexcept { _matchStarted = started; }
  [[nodiscard]] bool isMatchStarted() const noexcept { return _matchStarted; }

private:
  rtk::RenderWindow &_window;
  std::vector<std::size_t> _entities;
  float _blinkTimer = 0.0f;
  std::size_t _promptEntity = 0;
  bool _matchStarted = false;
};

} // namespace pong::client
