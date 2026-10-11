#pragma once
#include "Logger/Logger.hpp"
#include "components/AABBCollider.hpp"
#include "components/Enemy.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "systems/RenderConcept.hpp"
#include <ISystem.hpp>
#include <random>

namespace rtk::systems {
class EnemySpawnerSystem final : public rtk::ecs::ISystem {
private:
  uint32_t _enemyTextureId;
  float _spawnTimer{0.0f};
  float _spawnInterval{1.5f};

  std::mt19937 _rng;
  std::uniform_real_distribution<float> _distX;
  std::uniform_real_distribution<float> _distY;

public:
  EnemySpawnerSystem(uint32_t enemyTextureId)
      : _enemyTextureId(enemyTextureId), _rng(std::random_device{}()),
        _distX(1920.0f, 2920.0f), _distY(200.0f, 400.0f) {}

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Enemy Spawner System");
  }

  void update(rtk::ecs::Registry &reg, float dt) override {
    _spawnTimer += dt;

    if (_spawnTimer >= _spawnInterval) {
      _spawnTimer = 0.0f;

      auto enemyEntity = reg.spawn_entity();

      component::Transform transform{};
      transform.position = {_distX(_rng), _distY(_rng)};
      transform.rotation = 0.f;
      transform.scale = {1.f, 1.f};

      component::Velocity velocity{-200.0f, 0.0f};

      rtk::ecs::concepts::SpriteData sprite{};
      sprite.size = {64.f, 64.f};
      sprite.origin = {0.f, 0.f};
      sprite.textureRect = {0, 0, 64, 64};
      sprite.color = {255, 255, 255, 255};
      sprite.textureId = _enemyTextureId;

      component::AABBCollider collider{};
      collider.offsetX = 0.f;
      collider.offsetY = 0.f;
      collider.width = 64.f;
      collider.height = 64.f;
      collider.layer = Collider::CollisionLayer::Enemy;
      collider.mask = Collider::CollisionLayer::PlayerProjectile |
                      Collider::CollisionLayer::Player;

      reg.get_components<component::Transform>().insert_at(enemyEntity,
                                                           transform);
      reg.get_components<component::Velocity>().insert_at(enemyEntity,
                                                          velocity);
      reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(
          enemyEntity, sprite);
      reg.get_components<component::AABBCollider>().insert_at(enemyEntity,
                                                              collider);
      reg.get_components<component::Enemy>().insert_at(enemyEntity, {});
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems