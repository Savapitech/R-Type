#pragma once

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Request to play a sound loaded in the audio manager.
 */
struct SoundRequest {
  size_t soundId; ///< Id returned by the audio manager when loading.
};

/**
 * @brief Adds a SoundRequest to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object with an optional `soundId` field.
 */
inline void soundRequestFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                 const nlohmann::json &data) {
  component::SoundRequest soundRequest{};
  soundRequest.soundId = data.value("soundId", static_cast<std::size_t>(0));

  reg.get_components<component::SoundRequest>().insert_at(entity, soundRequest);
}

/** @} */
} // namespace component
