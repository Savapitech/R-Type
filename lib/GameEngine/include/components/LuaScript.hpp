#pragma once

#include <Registry.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace component {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief References an external Lua script that controls an entity's trajectory and behavior.
 */
struct LuaScript {
  std::string scriptPath; ///< Path to the .lua script file.
  float time{0.0f};       ///< Cumulative time passed to the Lua script.
};

/**
 * @brief Adds LuaScript component to an entity from its blueprint data.
 * @param reg Registry owning the entity.
 * @param entity Target entity.
 * @param data JSON object containing "script" path and optional "time".
 */
inline void luaScriptFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                             const nlohmann::json &data) {
  component::LuaScript script{};
  script.scriptPath = data.value("script", "");
  script.time = data.value("time", 0.0f);

  reg.get_components<component::LuaScript>().insert_at(entity, script);
}

/** @} */
} // namespace component
