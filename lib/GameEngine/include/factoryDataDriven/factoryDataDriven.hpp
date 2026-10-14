#pragma once

#include "components/NetworkEntity.hpp"
#include <Registry.hpp>
#include <cstdint>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rtk {

using ArchetypeID = std::uint16_t;

/**
 * @brief Computes 16-bit hash.
 */
constexpr ArchetypeID hashArchetype(std::string_view name) noexcept {
  std::uint32_t hash = 2166136261u;
  for (char c : name) {
    hash ^= static_cast<std::uint8_t>(c);
    hash *= 16777619u;
  }
  return static_cast<ArchetypeID>((hash >> 16) ^ (hash & 0xFFFF));
}

namespace factory {

class EntityFactory {
public:
  using ArchetypeID = rtk::ArchetypeID;
  using json = nlohmann::json;
  using ComponentBuilder =
      std::function<void(rtk::ecs::Registry &, std::size_t, const json &)>;

private:
  std::unordered_map<std::string, ComponentBuilder> _builders;
  std::unordered_map<std::string, json> _blueprints;
  std::unordered_map<ArchetypeID, std::string> _archetypes;

public:
  void registerBuilder(const std::string &name, ComponentBuilder builder) {
    _builders[name] = builder;
  }

  ArchetypeID registerBlueprint(const std::string &blueprintName,
                                const json &blueprintJson) {
    ArchetypeID id = hashArchetype(blueprintName);
    _blueprints[blueprintName] = blueprintJson;
    _archetypes[id] = blueprintName;
    return id;
  }

  ArchetypeID loadBlueprintFromPath(const std::string &blueprintName,
                                    const std::string &filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open blueprint file: " + filepath);
    }

    json blueprintJson;
    file >> blueprintJson;
    std::cout << blueprintName << " Hash ID : "
              << registerBlueprint(blueprintName, blueprintJson) << std::endl;
    return registerBlueprint(blueprintName, blueprintJson);
  }

  [[nodiscard]] ArchetypeID
  getArchetypeID(const std::string &blueprintName) const {
    return hashArchetype(blueprintName);
  }

  [[nodiscard]] const std::string &
  getBlueprintName(ArchetypeID archetypeId) const {
    auto it = _archetypes.find(archetypeId);
    if (it == _archetypes.end()) {
      throw std::runtime_error("ArchetypeID not found: " +
                               std::to_string(archetypeId));
    }
    return it->second;
  }

  [[nodiscard]] bool hasArchetype(ArchetypeID archetypeId) const {
    return _archetypes.contains(archetypeId);
  }

  [[nodiscard]] bool hasBlueprint(const std::string &blueprintName) const {
    return _blueprints.contains(blueprintName);
  }

  void buildComponents(rtk::ecs::Registry &reg, std::size_t entity,
                       const json &componentsJson) {
    for (const auto &[compName, compData] : componentsJson.items()) {
      if (_builders.contains(compName)) {
        _builders[compName](reg, entity, compData);
      }
    }
  }

  std::size_t createEntity(rtk::ecs::Registry &reg, const json &entityJson) {
    std::size_t entity = reg.spawn_entity();

    if (entityJson.contains("components"))
      buildComponents(reg, entity, entityJson["components"]);
    return entity;
  }

  std::size_t createFromBlueprint(rtk::ecs::Registry &reg,
                                  const std::string &blueprintName) {
    if (!_blueprints.contains(blueprintName))
      throw std::runtime_error("Blueprint not found: " + blueprintName);

    return createEntity(reg, _blueprints.at(blueprintName));
  }

  std::size_t createFromBlueprint(rtk::ecs::Registry &reg,
                                  ArchetypeID archetypeId) {
    auto it = _archetypes.find(archetypeId);
    if (it == _archetypes.end())
      throw std::runtime_error("ArchetypeID not found: " +
                               std::to_string(archetypeId));

    return createFromBlueprint(reg, it->second);
  }

  std::size_t createNetwork(rtk::ecs::Registry &reg, ArchetypeID archetypeId,
                            std::uint32_t networkId) {
    std::size_t entity = createFromBlueprint(reg, archetypeId);
    reg.get_components<component::NetworkEntity>().insert_at(
        entity, component::NetworkEntity{networkId, archetypeId});
    return entity;
  }

  std::size_t createNetwork(rtk::ecs::Registry &reg,
                            const std::string &blueprintName,
                            std::uint32_t networkId) {
    ArchetypeID archetypeId = getArchetypeID(blueprintName);
    std::size_t entity = createFromBlueprint(reg, blueprintName);
    reg.get_components<component::NetworkEntity>().insert_at(
        entity, component::NetworkEntity{networkId, archetypeId});
    return entity;
  }
};

} // namespace factory
} // namespace rtk