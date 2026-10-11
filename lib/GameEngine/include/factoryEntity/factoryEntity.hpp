#pragma once

#include <concepts>
#include <string>
#include <unordered_map>
#include <exception>

#include <Registry.hpp>

#include "ComponentFromJsonConcept.hpp"

using Json = nlohmann::json;

namespace rtk::ecs
{
    class EntityFactory {
    public:
        EntityFactory() = default;

        template<ComponentFromJson T>
        void register_component(std::string name)
        {
            auto [iterator, inserted] = _builders.emplace(
                std::move(name),
                &buildComponent<T>
            );

            if (!inserted)
                throw std::invalid_argument("Component already register " + iterator->first);
        }

        Entity create(Registry& registry, const Json& definition) const {
            Entity entity = registry.spawn_entity();

            try {
                const auto& components = definition.at("components");

                for (const auto& [name, data] : components.items()) {
                    auto builder = _builders.find(name);

                    if (builder == _builders.end()) {
                        throw std::runtime_error("Composant uknown : " + name);
                    }

                    builder->second(registry, entity, data);
                }
            } catch (...) {
                registry.kill_entity(entity); 
                throw;
            }

            return entity;
        }

    private:
        using Builder = void (*)(Registry&, Entity, const Json&);

        std::unordered_map<std::string, Builder> _builders;

        template<ComponentFromJson T>
        static void buildComponent(Registry& registry, Entity entity, const Json& data) {
            T component = data.get<T>();

            registry.get_components<T>().insert_at(entity, std::move(component));
        }
    };
} // namespace rtk::ecs