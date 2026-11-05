#pragma once

#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>
#include <Registry.hpp>

namespace pong::component {

/**
 * @brief Identifies a paddle entity and which player controls it.
 */
struct Paddle {
  enum class Side : std::uint8_t { Left = 0, Right = 1 };
  Side side = Side::Left;
  float speed = 520.0f;
};

/**
 * @brief Constructs a Paddle component from a JSON blueprint node.
 */
inline void paddleFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                           const nlohmann::json &data) {
  Paddle paddle{};
  const std::string sideStr = data.value("side", "left");
  if (sideStr == "right" || sideStr == "Right") {
    paddle.side = Paddle::Side::Right;
  } else {
    paddle.side = Paddle::Side::Left;
  }
  paddle.speed = data.value("speed", 520.0f);
  reg.get_components<Paddle>().insert_at(entity, paddle);
}

/**
 * @brief Identifies the ball entity.
 */
struct Ball {
  float baseSpeed = 480.0f;
  float currentSpeed = 480.0f;
  float serveTimer = 0.5f;
  float serveDirX = 1.0f;
  bool inPlay = false;
};

/**
 * @brief Constructs a Ball component from a JSON blueprint node.
 */
inline void ballFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                         const nlohmann::json &data) {
  Ball ball{};
  ball.baseSpeed = data.value("baseSpeed", 480.0f);
  ball.currentSpeed = data.value("currentSpeed", ball.baseSpeed);
  ball.serveTimer = data.value("serveTimer", 0.5f);
  ball.serveDirX = data.value("serveDirX", 1.0f);
  ball.inPlay = data.value("inPlay", false);
  reg.get_components<Ball>().insert_at(entity, ball);
}

/**
 * @brief Identifies a score display entity for a player.
 */
struct ScoreDisplay {
  Paddle::Side side = Paddle::Side::Left;
  std::uint8_t score = 0;
};

/**
 * @brief Constructs a ScoreDisplay component from a JSON blueprint node.
 */
inline void scoreDisplayFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                 const nlohmann::json &data) {
  ScoreDisplay scoreDisplay{};
  const std::string sideStr = data.value("side", "left");
  if (sideStr == "right" || sideStr == "Right") {
    scoreDisplay.side = Paddle::Side::Right;
  } else {
    scoreDisplay.side = Paddle::Side::Left;
  }
  scoreDisplay.score = data.value<std::uint8_t>("score", 0);
  reg.get_components<ScoreDisplay>().insert_at(entity, scoreDisplay);
}

} // namespace pong::component
