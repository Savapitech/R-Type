#pragma once
#include "Registry.hpp"
#include <cstddef>
#include <iostream>
#include <nlohmann/json.hpp>
namespace component {

struct AttachedTo {
  std::size_t targetEntity{0};
  float offsetX{0.0f};
  float offsetY{0.0f};
};

inline void attachedToFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                               const nlohmann::json &data) {
  component::AttachedTo attached{};

  attached.targetEntity =
      data.value("targetEntity", static_cast<std::size_t>(0));
  attached.offsetX = data.value("offsetX", 0.0f);
  attached.offsetY = data.value("offsetY", 0.0f);

  reg.get_components<component::AttachedTo>().insert_at(entity, attached);
}
} // namespace component
