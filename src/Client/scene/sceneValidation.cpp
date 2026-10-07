/**
 * @file sceneValidation.cpp
 * @brief Validation pn comilation time.
 *
 * This file cast every Scene implemented and try to
 * template them with the scene concept rtk::Scene
 *
 * SceneManager already template the scene but it will
 * be less easier to read the compilation error in case
 * of SceneManager failure
 *
 * @code
 * static_assert(rtk::Scene<MenuScene, SceneType, rtk::InputState>);
 * @endcode
 */

#include "sceneManager/sceneConcept.hpp"
#include "menuScene.hpp"
#include "gameScene.hpp"
#include "sceneType.hpp"

namespace rtype::client
{
    static_assert(rtk::SceneIdentifier<SceneType>);
    static_assert(rtk::Scene<MenuScene, SceneType, rtk::InputState>);
    static_assert(rtk::Scene<GameScene, SceneType, rtk::InputState>);
}