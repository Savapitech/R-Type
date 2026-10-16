#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "EngineContext.hpp"
#include "graphical/renderWindow.hpp"
#include "sceneManager/sceneConcept.hpp"

#include "components/AABBCollider.hpp"
#include "components/Controllable.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"

#include "systems/AnimationSystem.hpp"
#include "systems/AnimatorSystem.hpp"
#include "systems/RenderConcept.hpp"

#include "sceneType.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief In game scene, which shows the scrolling starfield.
 */
class GameScene {
public:
  /**
   * @brief Creates the scene.
   * @param window Window of the client.
   */
  explicit GameScene(rtk::RenderWindow &window);
  ~GameScene() = default;

  /// Identifier of this scene.
  static constexpr SceneType Type = SceneType::Game;

  /**
   * @brief Creates the two parallax starfield layers.
   * @param context Engine services.
   */
  void onEnter(rtk::EngineContext &context);

  /**
   * @brief Does nothing, the game stays in this scene.
   * @param context Engine services.
   * @param inputState Input state of the frame.
   * @param dt Elapsed time since the last frame, in seconds.
   * @return The next scene, or std::nullopt to stay.
   */
  std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                    const rtk::InputState &inputState,
                                    float dt);

  /**
   * @brief Destroys the entities created by the scene.
   * @param context Engine services.
   */
  void onExit(rtk::EngineContext &context);

private:
  void createParallaxBackground(rtk::EngineContext &context,
                                const std::string &bp) {
    const float scaledWidth = 720.0f;

    for (int i = 0; i < 3; ++i) {
      auto bg =
          context._entityFactory.createFromBlueprint(context.registry, bp);

      auto *transform =
          context.registry.get_components<component::Transform>().get(bg);
      if (transform) {
        transform->position.x = i * scaledWidth;
      }

      _entities.push_back(bg);
    }
  }

  float createGate(rtk::EngineContext &context, float startX, bool symmetric) {
    auto gateEntity =
        context._entityFactory.createFromBlueprint(context.registry, "gate");

    context.registry.get_components<component::Transform>()
        .get(gateEntity)
        ->position.x = startX;
    _entities.push_back(gateEntity);

    if (symmetric) {
      auto ceilingEntity =
          context._entityFactory.createFromBlueprint(context.registry, "gate");
      auto *transform =
          context.registry.get_components<component::Transform>().get(
              ceilingEntity);

      transform->position.x = startX;
      transform->position.y = 0.0f;
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .get(ceilingEntity)
          ->flags = 1 << 1;
      _entities.push_back(ceilingEntity);
    }
    return startX + (192.0f * 2.0f);
  }

  float createRamp(rtk::EngineContext &context, float startX, bool symmetric) {
    auto rampEntity =
        context._entityFactory.createFromBlueprint(context.registry, "ramp");
    context.registry.get_components<component::Transform>()
        .get(rampEntity)
        ->position.x = startX;
    _entities.push_back(rampEntity);

    if (symmetric) {
      auto ceilingEntity =
          context._entityFactory.createFromBlueprint(context.registry, "ramp");
      auto *transform =
          context.registry.get_components<component::Transform>().get(
              ceilingEntity);
      transform->position.x = startX;
      transform->position.y = 0.0f;
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .get(ceilingEntity)
          ->flags = 1 << 1;
      _entities.push_back(ceilingEntity);
    }
    return startX + (192.0f * 2.0f);
  }

  float createReactor(rtk::EngineContext &context, float startX,
                      bool symmetric) {
    auto reactorEntity =
        context._entityFactory.createFromBlueprint(context.registry, "reactor");
    context.registry.get_components<component::Transform>()
        .get(reactorEntity)
        ->position.x = startX;
    _entities.push_back(reactorEntity);

    if (symmetric) {
      auto ceilingEntity = context._entityFactory.createFromBlueprint(
          context.registry, "reactor");
      auto *transform =
          context.registry.get_components<component::Transform>().get(
              ceilingEntity);
      transform->position.x = startX;
      transform->position.y = 0.0f;
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .get(ceilingEntity)
          ->flags = 1 << 1;
      _entities.push_back(ceilingEntity);
    }
    return startX + (113.0f * 2.0f);
  }

  float createHighGround(rtk::EngineContext &context, float startX,
                         bool symmetric) {
    auto highGroundEntity = context._entityFactory.createFromBlueprint(
        context.registry, "high_ground");
    context.registry.get_components<component::Transform>()
        .get(highGroundEntity)
        ->position.x = startX;
    _entities.push_back(highGroundEntity);

    if (symmetric) {
      auto ceilingEntity = context._entityFactory.createFromBlueprint(
          context.registry, "high_ground");
      auto *transform =
          context.registry.get_components<component::Transform>().get(
              ceilingEntity);
      transform->position.x = startX;
      transform->position.y = 0.0f;
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .get(ceilingEntity)
          ->flags = 1 << 1;
      _entities.push_back(ceilingEntity);
    }
    return startX + (64.0f * 2.0f);
  }

  float createLowGround(rtk::EngineContext &context, float startX,
                        bool symmetric) {
    auto lowGroundEntity = context._entityFactory.createFromBlueprint(
        context.registry, "low_ground");
    context.registry.get_components<component::Transform>()
        .get(lowGroundEntity)
        ->position.x = startX;
    _entities.push_back(lowGroundEntity);

    if (symmetric) {
      auto ceilingEntity = context._entityFactory.createFromBlueprint(
          context.registry, "low_ground");
      auto *transform =
          context.registry.get_components<component::Transform>().get(
              ceilingEntity);
      transform->position.x = startX;
      transform->position.y = 0.0f;
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .get(ceilingEntity)
          ->flags = 1 << 1;
      _entities.push_back(ceilingEntity);
    }
    return startX + (256.0f * 2.0f);
  }

  float createMediumGround(rtk::EngineContext &context, float startX,
                           bool symmetric) {
    auto groundEntity = context._entityFactory.createFromBlueprint(
        context.registry, "medium_ground");
    context.registry.get_components<component::Transform>()
        .get(groundEntity)
        ->position.x = startX;
    _entities.push_back(groundEntity);

    if (symmetric) {
      auto ceilingEntity = context._entityFactory.createFromBlueprint(
          context.registry, "medium_ground");
      auto *transform =
          context.registry.get_components<component::Transform>().get(
              ceilingEntity);
      transform->position.x = startX;
      transform->position.y = 0.0f;
      context.registry.get_components<rtk::ecs::concepts::SpriteData>()
          .get(ceilingEntity)
          ->flags = 1 << 1;
      _entities.push_back(ceilingEntity);
    }
    return startX + (64.0f * 2.0f);
  }

  void createBackground(rtk::EngineContext &context) {
    float currentX = 0.0f;
    currentX = createMediumGround((context), currentX, false);
    currentX = createLowGround((context), currentX, false);
    currentX = createHighGround((context), currentX, false);
    currentX = createMediumGround((context), currentX, false);
    currentX = createLowGround((context), currentX, false);
    currentX = createHighGround((context), currentX, false);
    currentX = createHighGround((context), currentX, false);
    currentX = createMediumGround((context), currentX, false);
    currentX = createLowGround((context), currentX, false);
    currentX = createMediumGround((context), currentX, false);
    currentX = createReactor(context, currentX, true);
    currentX = createLowGround((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createHighGround((context), currentX, true);
    currentX = createLowGround((context), currentX, true);
    currentX = createRamp((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createHighGround((context), currentX, true);
    currentX = createGate((context), currentX, true);
    currentX = createLowGround((context), currentX, true);
    currentX = createMediumGround((context), currentX, true);
    currentX = createHighGround((context), currentX, true);
  }

  void createPlayer(rtk::EngineContext &context) {
    // auto entity =
    // context._entityFactory.createFromBlueprint(context.registry, "player");

    //_entities.push_back(entity);
    //_playerEntity.emplace(entity);
  }
  rtk::RenderWindow &_window;

  std::vector<std::size_t> _entities;
  std::optional<std::size_t> _playerEntity;

  std::vector<std::size_t> bullets;

  std::uint32_t _textureId = 0;
  std::uint32_t _backgroundTextureId = 0;

  std::uint16_t _bulletPressedByTick = 0;
};

/** @} */
} // namespace rtype::client
