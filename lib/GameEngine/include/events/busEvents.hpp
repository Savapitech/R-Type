#pragma once
#include <vector>
#include <functional>
#include <iostream> 
#include <unordered_map>
#include <string>
#include <typeindex>
#include <algorithm>

class busEvent
{
    private:
        std::unordered_map<std::type_index, std::vector<std::function<void(const void *event)>>> _handlers;
    public:
        template <typename E>
        void subscribe(std::function<void(const E &event)> func) {
            auto it = _handlers.find(typeid(E));
            if (it != _handlers.end()) {
                it->second.push_back([fct = std::move(func)](const void *event) { fct(*static_cast<const E *>(event)); });
                return;
            }
            std::vector<std::function<void(const void *event)>> functionList;
            functionList.push_back([fct = std::move(func)](const void *event) { fct(*static_cast<const E *>(event)); });
            _handlers.insert({typeid(E), functionList});
        }
        template <typename E>
        void publish(const E &event) {
            auto it = _handlers.find(typeid(E));
            if (it == _handlers.end()){
                return;
            }
            auto &handler = it->second;
            for (const auto &h : handler) {
                h(&event);
            }

        }           
};
