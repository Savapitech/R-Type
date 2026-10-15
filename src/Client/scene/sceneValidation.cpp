/**
 * @file sceneValidation.cpp
 * @brief Compile time check that every client scene satisfies rtk::Scene.
 *
 * SceneManager already checks its scenes, but a failed check here gives a
 * much shorter compiler error.
 */

#include "gameScene.hpp"
#include "menuScene.hpp"
#include "networkConnectionScene.hpp"
#include "sceneManager/sceneConcept.hpp"
#include "sceneType.hpp"

namespace rtype::client {
static_assert(rtk::SceneIdentifier<SceneType>);
static_assert(rtk::Scene<MenuScene, SceneType, rtk::InputState>);
static_assert(rtk::Scene<GameScene, SceneType, rtk::InputState>);
static_assert(rtk::Scene<NetworkConnectionScene, SceneType, rtk::InputState>);

} // namespace rtype::client