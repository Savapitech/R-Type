#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "graphical/renderWindow.hpp"
#include "sceneManager/sceneConcept.hpp"
#include "sceneType.hpp"

namespace pong::systems {
class PongBallSystem;
}

namespace pong::client {

/**
 * @brief Active gameplay scene running the Pong match.
 */
class PongGameScene {
public:
  static constexpr PongSceneType Type = PongSceneType::Game;

  /**
   * @brief Constructs the game scene.
   * @param window Target render window.
   */
  explicit PongGameScene(rtk::RenderWindow &window);
  ~PongGameScene() = default;

  /**
   * @brief Spawns paddles, ball, walls, center line, and score displays.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Checks pause, game over state, and restart input.
   * @param context Engine services.
   * @param input Current frame input state.
   * @param dt Elapsed frame time.
   * @return Next scene identifier or nullopt.
   */
  std::optional<PongSceneType> onUpdate(rtk::EngineContext &context,
                                        const rtk::InputState &input, float dt);

  /**
   * @brief Destroys all gameplay entities.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

  /**
   * @brief Links the ball system for score and state queries.
   * @param ballSystem Pointer to the active ball system.
   */
  void setBallSystem(pong::systems::PongBallSystem *ballSystem) {
    _ballSystem = ballSystem;
  }

private:
  rtk::RenderWindow &_window;
  std::vector<std::size_t> _entities;
  pong::systems::PongBallSystem *_ballSystem = nullptr;
};

} // namespace pong::client
