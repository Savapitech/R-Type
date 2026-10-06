#include "sceneManager/sceneConcept.hpp"
#include "menuScene.hpp"
#include "gameScene.hpp"

namespace rtype::client
{
    static_assert(rtk::Scene<MenuScene>);
    static_assert(rtk::Scene<GameScene>);
}