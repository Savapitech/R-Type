#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "ISystem.hpp"
#include "Registry.hpp"
#include "components/Particle.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "systems/RenderConcept.hpp"
#include "utils/vec2.hpp"

namespace rtk::systems {
/**
 * @addtogroup api_engine
 * @{
 */

class ParticleManager;

/**
 * @brief Helper generator object exposing particle generation with 4
 * parameters.
 */
class ParticleGenerator {
public:
  /**
   * @brief Constructs the generator tied to a manager.
   * @param manager Reference to the parent particle manager.
   */
  explicit ParticleGenerator(ParticleManager &manager) : _manager(manager) {}

  /**
   * @brief Emits a burst of particles.
   * @param color Particle base color (RGBA).
   * @param density Number of particles to create.
   * @param radius Dispersion radius from origin.
   * @param position Center position of emission.
   */
  void operator()(const rtk::ecs::concepts::ColorRGBA8 &color,
                  std::size_t density, float radius,
                  const utils::vec2 &position);

  /**
   * @brief Generates a burst of particles.
   * @param color Particle base color (RGBA).
   * @param density Number of particles to create.
   * @param radius Dispersion radius from origin.
   * @param position Center position of emission.
   */
  void generate(const rtk::ecs::concepts::ColorRGBA8 &color,
                std::size_t density, float radius, const utils::vec2 &position);

  /**
   * @brief Alias to generate particles.
   * @param color Particle base color (RGBA).
   * @param density Number of particles to create.
   * @param radius Dispersion radius from origin.
   * @param position Center position of emission.
   */
  void emit(const rtk::ecs::concepts::ColorRGBA8 &color, std::size_t density,
            float radius, const utils::vec2 &position);

private:
  ParticleManager &_manager;
};

/**
 * @brief Manages particle lifecycles, fading, animations, and deferred
 * generation.
 */
class ParticleManager final : public rtk::ecs::ISystem {
public:
  /**
   * @brief Pending emission burst request to be processed on the next update.
   */
  struct EmissionRequest {
    rtk::ecs::concepts::ColorRGBA8 color; ///< Base RGBA color of the particles.
    std::size_t density;                  ///< Number of particles in the burst.
    float radius;         ///< Spatial dispersion radius in pixels.
    utils::vec2 position; ///< Center emission coordinates.
  };

  /**
   * @brief Constructs the particle manager.
   * @param reg Registry where particle entities will be allocated.
   */
  explicit ParticleManager(rtk::ecs::Registry &reg)
      : _registry(reg), _distAngle(0.0f, 6.28318530718f),
        _distUnit(0.0f, 1.0f) {
    std::random_device rd;
    _rng.seed(rd());
  }

  ~ParticleManager() override = default;

  /**
   * @brief Initializes the particle manager when the scheduler starts.
   * @param reg ECS entity registry.
   */
  void onStart(rtk::ecs::Registry &reg) override { (void)reg; }

  /**
   * @brief Enqueues a burst of particles to be emitted on the next update.
   *
   * This method performs no allocations or mutations on the ECS registry,
   * making it strictly safe to call from any system loop or event callback.
   *
   * @param color Base RGBA color of the particles.
   * @param density Number of particles in the burst.
   * @param radius Spatial radius in pixels around the center position.
   * @param position Center emission coordinates.
   */
  void generate(const rtk::ecs::concepts::ColorRGBA8 &color,
                std::size_t density, float radius,
                const utils::vec2 &position) {
    _pendingEmissions.push_back({color, density, radius, position});
  }

  /**
   * @brief Alias to generate a burst of particles.
   * @param color Base RGBA color of the particles.
   * @param density Number of particles in the burst.
   * @param radius Spatial radius in pixels around the center position.
   * @param position Center emission coordinates.
   */
  void emit(const rtk::ecs::concepts::ColorRGBA8 &color, std::size_t density,
            float radius, const utils::vec2 &position) {
    generate(color, density, radius, position);
  }

  /**
   * @brief Alias to spawn particles.
   * @param color Base RGBA color of the particles.
   * @param density Number of particles in the burst.
   * @param radius Spatial radius in pixels around the center position.
   * @param position Center emission coordinates.
   */
  void spawn(const rtk::ecs::concepts::ColorRGBA8 &color, std::size_t density,
             float radius, const utils::vec2 &position) {
    generate(color, density, radius, position);
  }

  /**
   * @brief Returns a generator object wrapping this manager.
   * @return ParticleGenerator instance.
   */
  [[nodiscard]] ParticleGenerator getGenerator() {
    return ParticleGenerator(*this);
  }

  /**
   * @brief Updates active particles, fades alpha/scale, and processes deferred
   * emission requests.
   * @param reg ECS entity registry.
   * @param dt Elapsed frame time in seconds.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    if (!_pendingEmissions.empty()) {
      auto &transforms = reg.get_components<component::Transform>();
      auto &velocities = reg.get_components<component::Velocity>();
      auto &sprites = reg.get_components<rtk::ecs::concepts::SpriteData>();
      auto &particles = reg.get_components<component::Particle>();

      for (const auto &req : _pendingEmissions) {
        for (std::size_t i = 0; i < req.density; ++i) {
          const float angle = _distAngle(_rng);
          const float dist = _distUnit(_rng) * req.radius;
          const float speed =
              _minSpeed + _distUnit(_rng) * (_maxSpeed - _minSpeed);
          const float life =
              _minLifetime + _distUnit(_rng) * (_maxLifetime - _minLifetime);

          const float spawnX = req.position.x + std::cos(angle) * dist;
          const float spawnY = req.position.y + std::sin(angle) * dist;
          const float velX = std::cos(angle) * speed;
          const float velY = std::sin(angle) * speed;

          const std::size_t entity = reg.spawn_entity();

          component::Transform trans{};
          trans.position = {spawnX, spawnY};
          trans.rotation = angle;
          trans.scale = {1.0f, 1.0f};

          component::Velocity vel{velX, velY};

          rtk::ecs::concepts::SpriteData sprite{};
          sprite.position = trans.position;
          sprite.scale = trans.scale;
          sprite.size = _particleSize;
          sprite.origin = {_particleSize.x * 0.5f, _particleSize.y * 0.5f};
          sprite.textureRect = _defaultTextureRect;
          sprite.color = req.color;
          sprite.textureId = _defaultTextureId;
          sprite.layer = _defaultLayer;
          sprite.rotation = 0.0f;
          sprite.flags = 0;

          component::Particle part{};
          part.lifetime = life;
          part.initialLifetime = life;
          part.initialColor = req.color;
          part.fadeAlpha = true;

          transforms.insert_at(entity, trans);
          velocities.insert_at(entity, vel);
          sprites.insert_at(entity, sprite);
          particles.insert_at(entity, part);

          _activeParticles.push_back(entity);
        }
      }
      _pendingEmissions.clear();
    }

    auto &transforms = reg.get_components<component::Transform>();
    auto &velocities = reg.get_components<component::Velocity>();
    auto &sprites = reg.get_components<rtk::ecs::concepts::SpriteData>();
    auto &particles = reg.get_components<component::Particle>();

    for (std::size_t i = 0; i < _activeParticles.size();) {
      const std::size_t entity = _activeParticles[i];
      auto *part = particles.get(entity);
      auto *trans = transforms.get(entity);
      auto *spr = sprites.get(entity);

      if (!part || !trans || !spr) {
        _activeParticles[i] = _activeParticles.back();
        _activeParticles.pop_back();
        continue;
      }

      part->lifetime -= dt;
      if (part->lifetime <= 0.0f) {
        reg.kill_entity(entity);
        _activeParticles[i] = _activeParticles.back();
        _activeParticles.pop_back();
        continue;
      }

      if (auto *vel = velocities.get(entity)) {
        vel->x *= std::pow(0.25f, dt);
        vel->y *= std::pow(0.25f, dt);
      }

      if (part->fadeAlpha && part->initialLifetime > 0.0f) {
        const float ratio =
            std::clamp(part->lifetime / part->initialLifetime, 0.0f, 1.0f);
        spr->color.a = static_cast<uint8_t>(
            static_cast<float>(part->initialColor.a) * ratio);
        trans->scale.x = ratio;
        trans->scale.y = ratio;
      }

      ++i;
    }
  }

  /**
   * @brief Destroys all active particles and cleans up requests when stopped.
   * @param reg ECS entity registry.
   */
  void onStop(rtk::ecs::Registry &reg) override {
    for (auto ent : _activeParticles) {
      reg.kill_entity(ent);
    }
    _activeParticles.clear();
    _pendingEmissions.clear();
  }

  /**
   * @brief Sets the texture handle and UV coordinates used by emitted
   * particles.
   * @param textureId Texture identifier returned by the renderer.
   * @param rect Source rectangle inside the texture.
   */
  void setDefaultTexture(uint32_t textureId,
                         rtk::ecs::concepts::TextureRectU16 rect = {0, 0, 1,
                                                                    1}) {
    _defaultTextureId = textureId;
    _defaultTextureRect = rect;
  }

  /**
   * @brief Sets the base size in pixels for newly emitted particles.
   * @param size Width and height.
   */
  void setParticleSize(utils::vec2 size) noexcept { _particleSize = size; }

  /**
   * @brief Sets the random lifetime bounds for emitted particles.
   * @param minLife Minimum duration in seconds.
   * @param maxLife Maximum duration in seconds.
   */
  void setLifetimeRange(float minLife, float maxLife) noexcept {
    _minLifetime = minLife;
    _maxLifetime = maxLife;
  }

  /**
   * @brief Sets the initial emission speed range in pixels per second.
   * @param minSpeed Minimum velocity magnitude.
   * @param maxSpeed Maximum velocity magnitude.
   */
  void setSpeedRange(float minSpeed, float maxSpeed) noexcept {
    _minSpeed = minSpeed;
    _maxSpeed = maxSpeed;
  }

  /**
   * @brief Sets the rendering layer priority.
   * @param layer Render layer.
   */
  void setLayer(uint32_t layer) noexcept { _defaultLayer = layer; }

private:
  rtk::ecs::Registry &_registry;
  std::mt19937 _rng;
  std::uniform_real_distribution<float> _distAngle;
  std::uniform_real_distribution<float> _distUnit;

  std::vector<EmissionRequest> _pendingEmissions;
  std::vector<std::size_t> _activeParticles;

  uint32_t _defaultTextureId{0};
  rtk::ecs::concepts::TextureRectU16 _defaultTextureRect{0, 0, 1, 1};
  utils::vec2 _particleSize{4.0f, 4.0f};
  float _minLifetime = 0.20f;
  float _maxLifetime = 0.45f;
  float _minSpeed = 60.0f;
  float _maxSpeed = 240.0f;
  uint32_t _defaultLayer = 25;
};

inline void
ParticleGenerator::operator()(const rtk::ecs::concepts::ColorRGBA8 &color,
                              std::size_t density, float radius,
                              const utils::vec2 &position) {
  _manager.generate(color, density, radius, position);
}

inline void
ParticleGenerator::generate(const rtk::ecs::concepts::ColorRGBA8 &color,
                            std::size_t density, float radius,
                            const utils::vec2 &position) {
  _manager.generate(color, density, radius, position);
}

inline void ParticleGenerator::emit(const rtk::ecs::concepts::ColorRGBA8 &color,
                                    std::size_t density, float radius,
                                    const utils::vec2 &position) {
  _manager.generate(color, density, radius, position);
}

/** @} */
} // namespace rtk::systems
