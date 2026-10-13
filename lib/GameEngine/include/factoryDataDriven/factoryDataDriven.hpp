#pragma once

#include <unordered_map>
#include <string>
#include <functional>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <fstream>
#include <Registry.hpp>

namespace rtk::factory {

class EntityFactory {
public:
    using json = nlohmann::json;
    using ComponentBuilder = std::function<void(rtk::ecs::Registry&, std::size_t, const json&)>;

private:
    std::unordered_map<std::string, ComponentBuilder> _builders;

    std::unordered_map<std::string, json> _blueprints;

public:

    void registerBuilder(const std::string& name, ComponentBuilder builder) {
        _builders[name] = builder;
    }

    void registerBlueprint(const std::string& blueprintName, const json& blueprintJson) {
        _blueprints[blueprintName] = blueprintJson;
    }
    void loadBlueprintFromPath(const std::string& blueprintName, const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open blueprint file: " + filepath);
        }

        json blueprintJson;
        file >> blueprintJson;
        registerBlueprint(blueprintName, blueprintJson);
    }

    void buildComponents(rtk::ecs::Registry& reg, std::size_t entity, const json& componentsJson) {
        for (const auto& [compName, compData] : componentsJson.items()) {
            if (_builders.contains(compName)) {
                _builders[compName](reg, entity, compData);
            }
        }
    }

    std::size_t createEntity(rtk::ecs::Registry& reg, const json& entityJson) {
        std::size_t entity = reg.spawn_entity();

        if (entityJson.contains("components"))
            buildComponents(reg, entity, entityJson["components"]);
        return entity;
    }

    std::size_t createFromBlueprint(rtk::ecs::Registry& reg, const std::string& blueprintName) {
        if (!_blueprints.contains(blueprintName))
            throw std::runtime_error("Blueprint not find: " + blueprintName);

        return createEntity(reg, _blueprints[blueprintName]);
    }
};

} // namespace rtk::factory