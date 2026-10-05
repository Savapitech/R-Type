#include "sceneConcept.hpp"
#include "menuScene.hpp"
#include "gameScene.hpp"

namespace rtype::client
{
    static_assert(Scene<MenuScene>);
    static_assert(Scene<GameScene>);
}