#pragma once

#include <vector>

#include "components/AABBCollider.hpp"

namespace rtk::collision {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Pair of entities whose colliders overlap.
 */
struct Collision {
  std::size_t firstEntity;  ///< Entity with the lower id.
  std::size_t secondEntity; ///< Entity with the higher id.
};

/**
 * @brief Collisions detected during the current frame.
 */
class CollisionBuffer {
public:
  /**
   * @brief Removes every stored collision.
   */
  void clear() { _collisions.clear(); }

  /**
   * @brief Stores a collision.
   * @param collision Collision to store.
   */
  void push(const Collision &collision) { _collisions.push_back(collision); }

  /**
   * @brief Returns the stored collisions.
   * @return Collisions of the current frame.
   */
  const std::vector<Collision> &collisions() const { return _collisions; }

private:
  std::vector<Collision> _collisions;
};

/** @} */
} // namespace rtk::collision
