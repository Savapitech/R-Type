#pragma once

#include <ISystem.hpp>
#include <algorithm>
#include <cmath>

#include "../components/PongComponents.hpp"
#include "../events/BallBounceEvent.hpp"
#include "../events/ScoreEvent.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "events/busEvents.hpp"

namespace pong::systems {

/**
 * @brief Manages ball physics, bounce deflection, scoring events, and paddle
 * interactions.
 */
class PongBallSystem final : public rtk::ecs::ISystem {
public:
  static constexpr float BALL_SIZE = 10.0f;
  static constexpr float PADDLE_WIDTH = 12.0f;
  static constexpr float PADDLE_HEIGHT = 64.0f;
  static constexpr float TOP_BOUND = 8.0f;
  static constexpr float BOTTOM_BOUND = 712.0f - BALL_SIZE;

  /**
   * @brief Constructs the ball system.
   * @param eventBus Engine event bus used to publish match score changes.
   */
  explicit PongBallSystem(busEvent &eventBus) : _eventBus(eventBus) {}
  ~PongBallSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {}

  void setEnabled(bool enabled) noexcept { _enabled = enabled; }
  [[nodiscard]] bool isEnabled() const noexcept { return _enabled; }

  /**
   * @brief Updates ball movement, bounces, and scoring.
   * @param reg Entity registry.
   * @param dt Delta time in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    if (!_enabled)
      return;
    dt = std::min(dt, 0.05f);
    auto &transforms = reg.get_components<::component::Transform>();
    auto &velocities = reg.get_components<::component::Velocity>();
    auto &balls = reg.get_components<pong::component::Ball>();
    auto &paddles = reg.get_components<pong::component::Paddle>();

    auto ballView = reg.view<pong::component::Ball, ::component::Transform,
                             ::component::Velocity>();
    auto paddleView =
        reg.view<pong::component::Paddle, ::component::Transform>();

    for (auto ballEnt : ballView) {
      auto *bTrans = transforms.get(ballEnt);
      auto *bVel = velocities.get(ballEnt);
      auto *ball = balls.get(ballEnt);
      if (!bTrans || !bVel || !ball)
        continue;

      if (!ball->inPlay) {
        ball->serveTimer -= dt;
        if (ball->serveTimer <= 0.0f) {
          ball->inPlay = true;
          ball->currentSpeed = ball->baseSpeed;
          bVel->x = ball->serveDirX * ball->currentSpeed;
          bVel->y = ((_scoreP1 + _scoreP2) % 2 == 0) ? -120.0f : 120.0f;
        } else {
          bVel->x = 0.0f;
          bVel->y = 0.0f;
          bTrans->position = {635.0f, 355.0f};
          continue;
        }
      }

      if (bTrans->position.y <= TOP_BOUND) {
        bTrans->position.y = TOP_BOUND;
        bVel->y = std::abs(bVel->y);
        _eventBus.publish(pong::events::BallBounceEvent{
            .position = {bTrans->position.x + BALL_SIZE * 0.5f, TOP_BOUND}});
      } else if (bTrans->position.y >= BOTTOM_BOUND) {
        bTrans->position.y = BOTTOM_BOUND;
        bVel->y = -std::abs(bVel->y);
        _eventBus.publish(pong::events::BallBounceEvent{
            .position = {bTrans->position.x + BALL_SIZE * 0.5f, BOTTOM_BOUND}});
      }

      for (auto padEnt : paddleView) {
        const auto *pTrans = transforms.get(padEnt);
        const auto *paddle = paddles.get(padEnt);
        if (!pTrans || !paddle)
          continue;

        const float padLeft = pTrans->position.x;
        const float padRight = padLeft + PADDLE_WIDTH;
        const float padTop = pTrans->position.y;
        const float padBottom = padTop + PADDLE_HEIGHT;

        const float ballLeft = bTrans->position.x;
        const float ballRight = ballLeft + BALL_SIZE;
        const float ballTop = bTrans->position.y;
        const float ballBottom = ballTop + BALL_SIZE;

        const bool overlapY = (ballBottom >= padTop && ballTop <= padBottom);

        if (paddle->side == pong::component::Paddle::Side::Left &&
            bVel->x < 0.0f) {
          if (ballLeft <= padRight && ballRight >= padLeft && overlapY) {
            ball->currentSpeed = std::min(ball->currentSpeed * 1.05f, 950.0f);
            const float hitRel = ((ballTop + BALL_SIZE * 0.5f) -
                                  (padTop + PADDLE_HEIGHT * 0.5f)) /
                                 (PADDLE_HEIGHT * 0.5f);
            bVel->y = std::clamp(hitRel, -1.0f, 1.0f) * 440.0f;
            const float remSq =
                ball->currentSpeed * ball->currentSpeed - bVel->y * bVel->y;
            bVel->x = std::sqrt(std::max(10000.0f, remSq));
            bTrans->position.x = padRight + 1.0f;
            _eventBus.publish(pong::events::BallBounceEvent{
                .position = {padRight, bTrans->position.y + BALL_SIZE * 0.5f}});
          }
        } else if (paddle->side == pong::component::Paddle::Side::Right &&
                   bVel->x > 0.0f) {
          if (ballRight >= padLeft && ballLeft <= padRight && overlapY) {
            ball->currentSpeed = std::min(ball->currentSpeed * 1.05f, 950.0f);
            const float hitRel = ((ballTop + BALL_SIZE * 0.5f) -
                                  (padTop + PADDLE_HEIGHT * 0.5f)) /
                                 (PADDLE_HEIGHT * 0.5f);
            bVel->y = std::clamp(hitRel, -1.0f, 1.0f) * 440.0f;
            const float remSq =
                ball->currentSpeed * ball->currentSpeed - bVel->y * bVel->y;
            bVel->x = -std::sqrt(std::max(10000.0f, remSq));
            bTrans->position.x = padLeft - BALL_SIZE - 1.0f;
            _eventBus.publish(pong::events::BallBounceEvent{
                .position = {padLeft, bTrans->position.y + BALL_SIZE * 0.5f}});
          }
        }
      }

      if (bTrans->position.x < -15.0f) {
        _scoreP2 = _scoreP2 + 1;
        if (_scoreP2 >= 10) {
          _gameOver = true;
        }
        _eventBus.publish(pong::events::ScoreChangedEvent{
            .scorer = pong::component::Paddle::Side::Right,
            .scoreP1 = _scoreP1,
            .scoreP2 = _scoreP2});
        ball->inPlay = false;
        ball->serveTimer = 0.5f;
        ball->serveDirX = -1.0f;
      } else if (bTrans->position.x > 1285.0f) {
        _scoreP1 = _scoreP1 + 1;
        if (_scoreP1 >= 10) {
          _gameOver = true;
        }
        _eventBus.publish(pong::events::ScoreChangedEvent{
            .scorer = pong::component::Paddle::Side::Left,
            .scoreP1 = _scoreP1,
            .scoreP2 = _scoreP2});
        ball->inPlay = false;
        ball->serveTimer = 0.5f;
        ball->serveDirX = 1.0f;
      }
    }
  }

  void onStop(rtk::ecs::Registry &) override {}

  /**
   * @brief Resets ball position, speed, and restarts scores via event bus.
   * @param reg ECS entity registry.
   */
  void resetMatch(rtk::ecs::Registry &reg) {
    _scoreP1 = 0;
    _scoreP2 = 0;
    _gameOver = false;
    _eventBus.publish(pong::events::ScoreChangedEvent{
        .scorer = pong::component::Paddle::Side::Left,
        .scoreP1 = 0,
        .scoreP2 = 0});

    auto ballView = reg.view<::component::Transform, ::component::Velocity,
                             pong::component::Ball>();
    auto &transforms = reg.get_components<::component::Transform>();
    auto &velocities = reg.get_components<::component::Velocity>();
    auto &balls = reg.get_components<pong::component::Ball>();

    for (auto ballEnt : ballView) {
      if (auto *bt = transforms.get(ballEnt)) {
        bt->position = {635.0f, 355.0f};
      }
      if (auto *bv = velocities.get(ballEnt)) {
        bv->x = 0.0f;
        bv->y = 0.0f;
      }
      if (auto *ball = balls.get(ballEnt)) {
        ball->inPlay = false;
        ball->serveTimer = 0.5f;
        ball->serveDirX = 1.0f;
        ball->currentSpeed = ball->baseSpeed;
      }
    }
  }

  [[nodiscard]] std::uint8_t getScoreP1() const noexcept { return _scoreP1; }
  [[nodiscard]] std::uint8_t getScoreP2() const noexcept { return _scoreP2; }
  [[nodiscard]] bool isGameOver() const noexcept { return _gameOver; }

private:
  busEvent &_eventBus;
  std::uint8_t _scoreP1 = 0;
  std::uint8_t _scoreP2 = 0;
  bool _gameOver = false;
  bool _enabled = true;
};

} // namespace pong::systems
