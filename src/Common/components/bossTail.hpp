#pragma once

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include <Registry.hpp>

namespace component {
/**
 * @addtogroup api_gameplay
 * @{
 */

/**
 * @brief Marks an entity as a tail.
 */
struct tail {
  std::vector<std::string>
      _segmentsBluePrint; ///< Blueprints of the segment of the tail.
  std::vector<std::size_t> _segments; ///< Segment of the tail.
  float tipX{0.0f};      ///< Tip in position x of the tail in the boss body.
  float tipY{0.0f};      ///< Tip in position y of the tail in the boss body.
  float spacing{0.0f};   ///< Spacing between to segment of the tail.
  float baseAngle{0.0f}; ///< Base angle of the tail.
  float swing{0.0f};     ///< Amplitude of the each segment when the tail swing.
  float speed{0.0f};     ///< Swing speed.
  float phase{0.0f}; ///< The difference of position between a segment and the
                     ///< second segment.
  float time{0.0};   ///< Elapsed time.
};

/**
 * @brief Adds a tail to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with optional fields.
 */
inline void tailFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                         const nlohmann::json &data) {
  component::tail tail{};
  tail._segmentsBluePrint =
      data.value("segmentsBlueprint", std::vector<std::string>{});
  tail.tipX = data.value("tipX", 60.0f);
  tail.tipY = data.value("tipY", 186.0f);
  tail.spacing = data.value("spacing", 33.0f);
  tail.baseAngle = data.value("baseAngle", 3.4f);
  tail.swing = data.value("swing", 0.6f);
  tail.speed = data.value("speed", 2.5f);
  tail.phase = data.value("phase", 0.5f);
  reg.get_components<component::tail>().insert_at(entity, tail);
}

/** @} */
} // namespace component
