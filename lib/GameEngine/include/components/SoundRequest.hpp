#pragma once

#include <cstddef>
#include <cstdint>
#include <nlohmann/json.hpp>

#include <Registry.hpp>

namespace component {
struct SoundRequest {
  size_t soundId;
};

inline void soundRequestFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                                 const nlohmann::json &data) {
  component::SoundRequest soundRequest{};
  soundRequest.soundId = data.value("soundId", static_cast<std::size_t>(0));

  reg.get_components<component::SoundRequest>().insert_at(entity, soundRequest);
}

} // namespace component