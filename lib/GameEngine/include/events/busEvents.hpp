#pragma once

/**
 * @addtogroup api_engine
 * @{
 */
#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

/**
 * @brief Type indexed publish and subscribe event bus.
 *
 * Handlers run synchronously on the thread that publishes the event.
 */
class busEvent {
private:
  std::unordered_map<std::type_index,
                     std::vector<std::function<void(const void *event)>>>
      _handlers;

public:
  /**
   * @brief Registers a handler for one event type.
   * @tparam E Event type.
   * @param func Function called with every published event of type E.
   */
  template <typename E>
  void subscribe(std::function<void(const E &event)> func) {
    auto it = _handlers.find(typeid(E));
    if (it != _handlers.end()) {
      it->second.push_back([fct = std::move(func)](const void *event) {
        fct(*static_cast<const E *>(event));
      });
      return;
    }
    std::vector<std::function<void(const void *event)>> functionList;
    functionList.push_back([fct = std::move(func)](const void *event) {
      fct(*static_cast<const E *>(event));
    });
    _handlers.insert({typeid(E), functionList});
  }
  /**
   * @brief Calls every handler registered for the event type.
   * @tparam E Event type.
   * @param event Event passed to the handlers.
   */
  template <typename E> void publish(const E &event) {
    auto it = _handlers.find(typeid(E));
    if (it == _handlers.end()) {
      return;
    }
    auto &handler = it->second;
    for (const auto &h : handler) {
      h(&event);
    }
  }
};

/** @} */
