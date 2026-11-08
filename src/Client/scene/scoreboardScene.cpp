#include "scoreboardScene.hpp"

#include "game.hpp"
#include "input/action.hpp"
#include "ui/Text.hpp"

namespace rtype::client {

void ScoreboardScene::onEnter(rtk::EngineContext &) {
  _requested = _dependency.game.requestScoreboard();
  _timer = 0.0f;
  _shownText.clear();
  // Keys held when the page opens are ignored until released.
  _previousActionMask = ~std::uint32_t{0};
}

std::optional<SceneType>
ScoreboardScene::onUpdate(rtk::EngineContext &context,
                          const rtk::InputState &inputState, float dt) {
  const auto pressed = [&](rtype::Action action) {
    return rtype::Input::hasAction(inputState.actionMask, action) &&
           !rtype::Input::hasAction(_previousActionMask, action);
  };
  if (pressed(rtype::Action::LeaveRoom))
    return SceneType::Menu;
  if (pressed(rtype::Action::Confirm)) {
    _requested = _dependency.game.requestScoreboard();
    _timer = 0.0f;
  }
  _previousActionMask = inputState.actionMask;

  // Not connected yet: say hello once per second, then ask.
  _timer += dt;
  if (!_requested) {
    if (_dependency.networkClient.isConnected())
      _requested = _dependency.game.requestScoreboard();
    else if (static_cast<int>(_timer) != static_cast<int>(_timer - dt))
      _dependency.networkClient.sendHello();
  }

  std::string body = _dependency.game.getScoreboardText();
  if (body.empty())
    body = _timer > 5.0f ? "SERVER UNREACHABLE" : "LOADING...";
  const std::string text =
      "SCOREBOARD\n\n" + body + "\n\nENTER TO REFRESH   ESC TO GO BACK";
  if (text != _shownText) {
    _shownText = text;
    ui::clearText(context.registry, _entities);
    ui::drawText(context.registry, _dependency.window,
                 _dependency.game.getFont(), text, 120.0f, 60.0f, 0.6f,
                 _entities);
  }
  return std::nullopt;
}

void ScoreboardScene::onExit(rtk::EngineContext &context) {
  ui::clearText(context.registry, _entities);
  context.registry.flush();
}

} // namespace rtype::client
