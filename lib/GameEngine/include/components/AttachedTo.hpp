#pragma once
#include "Registry.hpp"
#include <cstddef>
#include <iostream>
#include <nlohmann/json.hpp>
namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Keeps an entity at a fixed offs.
 */
struct AttachedTo {
  std::size_t targetEntity{0}; ///< Entity followed.
  float offsetX{0.0f};         ///< Horizontal offset from the target.
  float offsetY{0.0f};         ///< Vertical offset from the target.
};

/**
 * @brief Add an AttachedTo to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional `targetEntity`, `offsetX` and
 * `offsetY` fields.
 */

inline void attachedToFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                               const nlohmann::json &data) {
  component::AttachedTo attached{};

  attached.targetEntity =
      data.value("targetEntity", static_cast<std::size_t>(0));
  attached.offsetX = data.value("offsetX", 0.0f);
  attached.offsetY = data.value("offsetY", 0.0f);

  reg.get_components<component::AttachedTo>().insert_at(entity, attached);
}

/** @} */
} // namespace component
