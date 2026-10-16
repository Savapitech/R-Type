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
/**
 * @addtogroup api_engine
 * @{
 */

/// 16-bit identifier of an entity blueprint, shared by client and server.
using ArchetypeID = std::uint16_t;

/**
 * @brief Hashes a blueprint name to its archetype id.
 *
 * Computes the 32-bit FNV-1a hash of the name and folds it to 16 bits with
 * `(hash >> 16) ^ (hash & 0xFFFF)`.
 *
 * @param name Blueprint name.
 * @return The archetype id.
 */
constexpr ArchetypeID hashArchetype(std::string_view name) noexcept {
  std::uint32_t hash = 2166136261u;
  for (char c : name) {
    hash ^= static_cast<std::uint8_t>(c);
    hash *= 16777619u;
  }
  return static_cast<ArchetypeID>((hash >> 16) ^ (hash & 0xFFFF));
}

/**
 * @brief Data driven entity creation.
 */
namespace factory {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Builds entities from JSON blueprints and registered component
 * builders.
 */
class EntityFactory {
public:
  /// 16-bit identifier of an entity blueprint.
  using ArchetypeID = rtk::ArchetypeID;
  /// JSON document type.
  using json = nlohmann::json;
  /// Function that adds one component to an entity from its JSON data.
  using ComponentBuilder =
      std::function<void(rtk::ecs::Registry &, std::size_t, const json &)>;

private:
  std::unordered_map<std::string, ComponentBuilder> _builders;
  std::unordered_map<std::string, json> _blueprints;
  std::unordered_map<ArchetypeID, std::string> _archetypes;

public:
  /**
   * @brief Associates a blueprint component key with a builder.
   * @param name Key used in the `components` object of a blueprint.
   * @param builder Function that adds the component.
   */
  void registerBuilder(const std::string &name, ComponentBuilder builder) {
    _builders[name] = builder;
  }

  /**
   * @brief Stores a blueprint under a name.
   * @param blueprintName Blueprint name, hashed to get its archetype id.
   * @param blueprintJson Blueprint document.
   * @return The archetype id of the blueprint.
   */
  ArchetypeID registerBlueprint(const std::string &blueprintName,
                                const json &blueprintJson) {
    ArchetypeID id = hashArchetype(blueprintName);
    _blueprints[blueprintName] = blueprintJson;
    _archetypes[id] = blueprintName;
    return id;
  }

  /**
   * @brief Loads a blueprint from a JSON file and stores it.
   * @param blueprintName Blueprint name, hashed to get its archetype id.
   * @param filepath Path of the JSON file.
   * @return The archetype id of the blueprint.
   * @throws std::runtime_error If the file cannot be opened.
   */
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

  /**
   * @brief Returns the archetype id of a blueprint name.
   * @param blueprintName Blueprint name.
   * @return The archetype id, whether or not the blueprint is loaded.
   */
  [[nodiscard]] ArchetypeID
  getArchetypeID(const std::string &blueprintName) const {
    return hashArchetype(blueprintName);
  }

  /**
   * @brief Returns the blueprint name of an archetype id.
   * @param archetypeId Archetype id.
   * @return The blueprint name.
   * @throws std::runtime_error If no loaded blueprint has this id.
   */
  [[nodiscard]] const std::string &
  getBlueprintName(ArchetypeID archetypeId) const {
    auto it = _archetypes.find(archetypeId);
    if (it == _archetypes.end()) {
      throw std::runtime_error("ArchetypeID not found: " +
                               std::to_string(archetypeId));
    }
    return it->second;
  }

  /**
   * @brief Tells whether a blueprint with this archetype id is loaded.
   * @param archetypeId Archetype id.
   * @return True if the blueprint is loaded.
   */
  [[nodiscard]] bool hasArchetype(ArchetypeID archetypeId) const {
    return _archetypes.contains(archetypeId);
  }

  /**
   * @brief Tells whether a blueprint with this name is loaded.
   * @param blueprintName Blueprint name.
   * @return True if the blueprint is loaded.
   */
  [[nodiscard]] bool hasBlueprint(const std::string &blueprintName) const {
    return _blueprints.contains(blueprintName);
  }

  /**
   * @brief Adds every component listed in a JSON object, unknown keys are
   * ignored.
   * @param reg Registry owning the entity.
   * @param entity Target entity.
   * @param componentsJson Object mapping builder keys to component data.
   */
  void buildComponents(rtk::ecs::Registry &reg, std::size_t entity,
                       const json &componentsJson) {
    for (const auto &[compName, compData] : componentsJson.items()) {
      if (_builders.contains(compName)) {
        _builders[compName](reg, entity, compData);
      }
    }
  }

  /**
   * @brief Spawns an entity from a blueprint document.
   * @param reg Registry receiving the entity.
   * @param entityJson Document with an optional `components` object.
   * @return The new entity.
   */
  std::size_t createEntity(rtk::ecs::Registry &reg, const json &entityJson) {
    std::size_t entity = reg.spawn_entity();

    if (entityJson.contains("components"))
      buildComponents(reg, entity, entityJson["components"]);
    return entity;
  }

  /**
   * @brief Spawns an entity from a loaded blueprint.
   * @param reg Registry receiving the entity.
   * @param blueprintName Blueprint name.
   * @return The new entity.
   * @throws std::runtime_error If the blueprint is not loaded.
   */
  std::size_t createFromBlueprint(rtk::ecs::Registry &reg,
                                  const std::string &blueprintName) {
    if (!_blueprints.contains(blueprintName))
      throw std::runtime_error("Blueprint not found: " + blueprintName);

    return createEntity(reg, _blueprints.at(blueprintName));
  }

  /**
   * @brief Spawns an entity from a loaded blueprint.
   * @param reg Registry receiving the entity.
   * @param archetypeId Archetype id of the blueprint.
   * @return The new entity.
   * @throws std::runtime_error If the blueprint is not loaded.
   */
  std::size_t createFromBlueprint(rtk::ecs::Registry &reg,
                                  ArchetypeID archetypeId) {
    auto it = _archetypes.find(archetypeId);
    if (it == _archetypes.end())
      throw std::runtime_error("ArchetypeID not found: " +
                               std::to_string(archetypeId));

    return createFromBlueprint(reg, it->second);
  }

  /**
   * @brief Spawns a replicated entity and attaches its NetworkEntity.
   * @param reg Registry receiving the entity.
   * @param archetypeId Archetype id of the blueprint.
   * @param networkId Network id of the entity.
   * @return The new entity.
   * @throws std::runtime_error If the blueprint is not loaded.
   */
  std::size_t createNetwork(rtk::ecs::Registry &reg, ArchetypeID archetypeId,
                            std::uint32_t networkId) {
    std::size_t entity = createFromBlueprint(reg, archetypeId);
    reg.get_components<component::NetworkEntity>().insert_at(
        entity, component::NetworkEntity{networkId, archetypeId});
    return entity;
  }

  /**
   * @brief Spawns a replicated entity and attaches its NetworkEntity.
   * @param reg Registry receiving the entity.
   * @param blueprintName Blueprint name.
   * @param networkId Network id of the entity.
   * @return The new entity.
   * @throws std::runtime_error If the blueprint is not loaded.
   */
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

/** @} */
} // namespace factory

/** @} */
} // namespace rtk
