#pragma once

#include <concepts>
#include <nlohmann/json.hpp>

using Json = nlohmann::json;

using Entity = std::size_t;

template <typename T>
concept ComponentFromJson =
    std::default_initializable<T> && requires(const Json &json, T &component) {
      { from_json(json, component) } -> std::same_as<void>;
    };