#pragma once

#include <ISystem.hpp>
#include <string>
#include <unordered_set>

#include "Logger/Logger.hpp"
#include "components/LuaScript.hpp"
#include "components/Velocity.hpp"

#include <lua.hpp>

namespace rtk::systems {
/**
 * @addtogroup api_engine
 * @{
 */

/**
 * @brief Executes external Lua scripts to update entity movement and behavior.
 */
class LuaScriptSystem final : public rtk::ecs::ISystem {
public:
  LuaScriptSystem() = default;
  ~LuaScriptSystem() override { closeLua(); }

  /**
   * @brief Initializes the Lua state and runtime environment.
   */
  void onStart(rtk::ecs::Registry &) override {
    LOG_INFO("Load Lua Script System");
    initLua();
  }

  /**
   * @brief Closes the Lua state and releases resources.
   */
  void onStop(rtk::ecs::Registry &) override { closeLua(); }

  /**
   * @brief Calls update_velocity in each entity's Lua script to update its
   * velocity.
   * @param reg ECS Registry.
   * @param dt Frame delta time.
   */
  void update(rtk::ecs::Registry &reg, float dt) override {
    if (!_L)
      return;

    auto view = reg.view<component::LuaScript, component::Velocity>();
    auto &scripts = reg.get_components<component::LuaScript>();
    auto &velocities = reg.get_components<component::Velocity>();

    for (auto entity : view) {
      auto *script = scripts.get(entity);
      auto *velocity = velocities.get(entity);

      if (!script || !velocity || script->scriptPath.empty())
        continue;

      script->time += dt;

      if (!loadScript(script->scriptPath))
        continue;

      lua_getglobal(_L, "update_velocity");
      if (!lua_isfunction(_L, -1)) {
        lua_pop(_L, 1);
        continue;
      }

      lua_pushnumber(_L, static_cast<lua_Number>(script->time));
      lua_pushnumber(_L, static_cast<lua_Number>(dt));
      lua_pushnumber(_L, static_cast<lua_Number>(velocity->x));
      lua_pushnumber(_L, static_cast<lua_Number>(velocity->y));

      if (lua_pcall(_L, 4, 2, 0) != LUA_OK) {
        LOG_WARN("Lua update_velocity execution error: " +
                 std::string(lua_tostring(_L, -1)));
        lua_pop(_L, 1);
        continue;
      }

      if (lua_isnumber(_L, -2) && lua_isnumber(_L, -1)) {
        velocity->x = static_cast<float>(lua_tonumber(_L, -2));
        velocity->y = static_cast<float>(lua_tonumber(_L, -1));
      }
      lua_pop(_L, 2);
    }
  }

private:
  lua_State *_L{nullptr};
  std::unordered_set<std::string> _loadedScripts;

  void initLua() {
    if (_L)
      return;
    _L = luaL_newstate();
    if (_L) {
      luaL_openlibs(_L);
    } else {
      LOG_ERROR("Failed to initialize Lua state");
    }
  }

  void closeLua() {
    if (_L) {
      lua_close(_L);
      _L = nullptr;
    }
    _loadedScripts.clear();
  }

  bool loadScript(const std::string &path) {
    if (_loadedScripts.contains(path))
      return true;

    if (luaL_dofile(_L, path.c_str()) != LUA_OK) {
      LOG_ERROR("Failed to load Lua script " + path + ": " +
                std::string(lua_tostring(_L, -1)));
      lua_pop(_L, 1);
      return false;
    }

    _loadedScripts.insert(path);
    LOG_INFO("Successfully loaded Lua script: " + path);
    return true;
  }
};

/** @} */
} // namespace rtk::systems
