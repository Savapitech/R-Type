#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "EngineContext.hpp"
#include "inputAction/InputAction.hpp"
#include "scene/clientDependency.hpp"
#include "scene/sceneType.hpp"

namespace rtype::client {

/**
 * @brief Page showing the scores of the best players, as sent by the server.
 *
 * Enter asks the server again, Escape goes back to the menu.
 */
class ScoreboardScene {
public:
  /**
   * @brief Creates the scene.
   * @param dep Shared client dependencies.
   */
  explicit ScoreboardScene(clientDependancy &dep) : _dependency(dep) {}

  /// Identifier of this scene.
  static constexpr SceneType Type = SceneType::Scoreboard;

  /**
   * @brief Asks the server for the scoreboard.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Connects if needed, shows the answer, handles Enter and Escape.
   * @param context Engine services.
   * @param inputState Current input state.
   * @param dt Elapsed time since the previous frame, in seconds.
   * @return SceneType::Menu on Escape, std::nullopt otherwise.
   */
  std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                    const rtk::InputState &inputState,
                                    float dt);

  /**
   * @brief Destroys the text of the page.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

private:
  clientDependancy &_dependency;
  std::vector<std::size_t> _entities;
  std::string _shownText;
  bool _requested = false;
  float _timer = 0.0f;
  std::uint32_t _previousActionMask = 0;
};

} // namespace rtype::client
