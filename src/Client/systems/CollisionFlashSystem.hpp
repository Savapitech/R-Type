#pragma once

#include "collision/collisionBuffer.hpp"
#include "components/AABBCollider.hpp"
#include "systems/RenderConcept.hpp"
#include <ISystem.hpp>
#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace rtk::systems {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Flashes entity sprites red upon collision and smoothly restores their
 * original color.
 */
class CollisionFlashSystem final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Creates the flash system.
   * @param collisionBuffer Buffer holding collisions of the current frame.
   */
  explicit CollisionFlashSystem(
      const rtk::collision::CollisionBuffer &collisionBuffer)
      : _collisionBuffer(collisionBuffer) {}

  /**
   * @brief Creates the flash system with a window parameter for compatibility.
   * @param collisionBuffer Buffer holding collisions of the current frame.
   */
  template <typename WindowType>
  CollisionFlashSystem(const rtk::collision::CollisionBuffer &collisionBuffer,
                       WindowType &)
      : _collisionBuffer(collisionBuffer) {}

  ~CollisionFlashSystem() override = default;

  /**
   * @brief Does nothing.
   */
  void onStart(rtk::ecs::Registry &) override {}

  /**
   * @brief Manually triggers a hit flash on a given entity.
   * @param registry Registry holding entities and components.
   * @param entity Entity to flash.
   */
  void triggerFlash(rtk::ecs::Registry &registry, std::size_t entity) {
    auto &sprites = registry.get_components<rtk::ecs::concepts::SpriteData>();
    triggerFlash(sprites, entity);
  }

  /**
   * @brief Updates active hit flashes and triggers new flashes from recent
   * collisions.
   * @param registry Registry holding entities and components.
   * @param dt Elapsed time since the last frame, in seconds.
   */
  void update(rtk::ecs::Registry &registry, float dt) override {
    auto &sprites = registry.get_components<rtk::ecs::concepts::SpriteData>();
    constexpr float holdDuration = 0.08f;
    constexpr std::uint8_t startGB = 70;

    for (auto it = _activeFlashes.begin(); it != _activeFlashes.end();) {
      auto &flash = it->second;
      flash.elapsed += dt;

      auto *sprite = sprites.get(flash.entity);
      if (!sprite) {
        it = _activeFlashes.erase(it);
        continue;
      }

      if (flash.elapsed >= flash.duration) {
        sprite->color.r = 255;
        sprite->color.g = 255;
        sprite->color.b = 255;
        it = _activeFlashes.erase(it);
      } else if (flash.elapsed <= holdDuration) {
        sprite->color.r = 255;
        sprite->color.g = startGB;
        sprite->color.b = startGB;
        ++it;
      } else {
        const float fadeProgress = std::clamp(
            (flash.elapsed - holdDuration) / (flash.duration - holdDuration),
            0.0f, 1.0f);
        const auto gb = static_cast<std::uint8_t>(startGB + (255.0f - startGB) *
                                                                fadeProgress);
        sprite->color.r = 255;
        sprite->color.g = gb;
        sprite->color.b = gb;
        ++it;
      }
    }

    auto &colliders = registry.get_components<component::AABBCollider>();
    constexpr std::uint32_t targetMask =
        Collider::CollisionLayer::Player | Collider::CollisionLayer::Enemy;

    for (const auto &col : _collisionBuffer.collisions()) {
      const auto *colA = colliders.get(col.firstEntity);
      const auto *colB = colliders.get(col.secondEntity);
      if (!colA || !colB)
        continue;

      if ((colA->layer & targetMask) != 0 && sprites.get(col.firstEntity)) {
        triggerFlash(sprites, col.firstEntity);
      }
      if ((colB->layer & targetMask) != 0 && sprites.get(col.secondEntity)) {
        triggerFlash(sprites, col.secondEntity);
      }
    }
  }

  /**
   * @brief Restores normal color on any remaining flashing sprites.
   * @param registry Registry holding entities and components.
   */
  void onStop(rtk::ecs::Registry &registry) override {
    auto &sprites = registry.get_components<rtk::ecs::concepts::SpriteData>();
    for (const auto &[entity, _] : _activeFlashes) {
      if (auto *sprite = sprites.get(entity)) {
        sprite->color.r = 255;
        sprite->color.g = 255;
        sprite->color.b = 255;
      }
    }
    _activeFlashes.clear();
  }

private:
  struct ActiveFlash {
    std::size_t entity{0};
    float elapsed{0.0f};
    float duration{0.22f};
  };

  void
  triggerFlash(rtk::ecs::SparseArray<rtk::ecs::concepts::SpriteData> &sprites,
               std::size_t entity) {
    auto *sprite = sprites.get(entity);
    if (!sprite)
      return;

    constexpr float flashDuration = 0.22f;
    constexpr std::uint8_t startGB = 70;
    _activeFlashes[entity] = ActiveFlash{entity, 0.0f, flashDuration};
    sprite->color.r = 255;
    sprite->color.g = startGB;
    sprite->color.b = startGB;
  }

  const rtk::collision::CollisionBuffer &_collisionBuffer;
  std::unordered_map<std::size_t, ActiveFlash> _activeFlashes;
};

/** @} */
} // namespace rtk::systems
