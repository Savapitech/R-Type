#pragma once
#include <ISystem.hpp>

#include "Logger/Logger.hpp"
#include "inputAction/InputAction.hpp"

#include "input/action.hpp"

#include "components/Velocity.hpp"
#include "components/Controllable.hpp"
#include "components/Transform.hpp"
#include "components/Bullet.hpp"

namespace rtk::systems {
class PlayerSystem final : public rtk::ecs::ISystem {
private:
  rtk::InputState &_inputState;
  uint32_t _bulletTextureId;
  static constexpr float SQUARE_2_DIVIDE_BY_2 = 0.7071f;

public:
  PlayerSystem(rtk::InputState &inputState, uint32_t bulletTextureId)
      : _inputState(inputState), _bulletTextureId(bulletTextureId) {}

  ~PlayerSystem() override = default;

  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Player System");
  }

  void update(rtk::ecs::Registry &reg, float ) override {
    auto view = reg.view<component::Velocity, component::Controllable, component::Transform>();

    auto &velocities = reg.get_components<component::Velocity>();
    auto &controllables = reg.get_components<component::Controllable>();
    auto &transforms = reg.get_components<component::Transform>();

    std::vector<component::Transform> bulletsToSpawn;

    for (auto entity : view) {
      auto *velocity = velocities.get(entity);
      auto *controllable = controllables.get(entity);
      auto *playerTransform = transforms.get(entity);

      if (!velocity || !controllable || !playerTransform) continue;

      velocity->x = 0.0f;
      velocity->y = 0.0f;

      if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Up)) {
          velocity->y = -controllable->speed;
      }
      if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Down)) {
          velocity->y = controllable->speed;
      }
      if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Left)) {
          velocity->x = -controllable->speed;
      }
      if (rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Right)) {
          velocity->x = controllable->speed;
      }

      if (velocity->x != 0.0f && velocity->y != 0.0f) {
          velocity->x *= SQUARE_2_DIVIDE_BY_2;
          velocity->y *= SQUARE_2_DIVIDE_BY_2;
      }

      bool isShootPressed = rtype::Input::hasAction(_inputState.actionMask, rtype::Action::Shoot);

      if (isShootPressed) {
          controllable->isChargingShoot = true;
      }
      else if (controllable->isChargingShoot) {
          controllable->isChargingShoot = false;

          component::Transform spawnTransform = {};
          spawnTransform.position = {playerTransform->position.x + 50.0f, playerTransform->position.y};
          spawnTransform.rotation = 0.f;
          spawnTransform.scale = {1.f, 1.f};

          bulletsToSpawn.push_back(spawnTransform);
      }
    }

    for (const auto& spawnTransform : bulletsToSpawn) {
        const auto bulletEntity = reg.spawn_entity();

        rtk::ecs::concepts::SpriteData sprite{};
        sprite.size = {32.f, 16.f};
        sprite.origin = {0.f, 0.f};
        sprite.textureRect = {0, 0, 32, 16};
        sprite.color = {255, 255, 255, 255};
        sprite.textureId = _bulletTextureId; 

        component::AABBCollider collider{};
        collider.offsetX = 0.f;
        collider.offsetY = 0.f;
        collider.width = 32.f;
        collider.height = 16.f;
        collider.layer = Collider::CollisionLayer::PlayerProjectile;
        collider.mask = Collider::CollisionLayer::EnemyProjectile |
                        Collider::CollisionLayer::Enemy;

        reg.get_components<component::Transform>().insert_at(bulletEntity, spawnTransform);
        reg.get_components<rtk::ecs::concepts::SpriteData>().insert_at(bulletEntity, sprite);
        reg.get_components<component::AABBCollider>().insert_at(bulletEntity, collider);
        reg.get_components<component::Velocity>().insert_at(bulletEntity, {800.0f, 0.0f});
        reg.get_components<component::Bullet>().insert_at(bulletEntity, {10.0f});
    }
  }

  void onStop(rtk::ecs::Registry &) override {}
};
} // namespace rtk::systems