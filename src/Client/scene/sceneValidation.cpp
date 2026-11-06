/**
 * @file sceneValidation.cpp
 * @brief Compile time check that every client scene satisfies rtk::Scene.
 *
 * SceneManager already checks its scenes, but a failed check here gives a
 * much shorter compiler error.
 */

#include "gameScene.hpp"
#include "inputAction/InputAction.hpp"
#include "menuScene.hpp"
#include "networkConnectionScene.hpp"
#include "roomListScene.hpp"
#include "sceneManager/sceneConcept.hpp"
#include "sceneType.hpp"
#include "settingScene.hpp"

namespace rtype::client {
static_assert(rtk::SceneIdentifier<SceneType>);
static_assert(rtk::Scene<MenuScene, SceneType, rtk::InputState>);
static_assert(rtk::Scene<GameScene, SceneType, rtk::InputState>);
static_assert(rtk::Scene<RoomListScene, SceneType, rtk::InputState>);
static_assert(rtk::Scene<NetworkConnectionScene, SceneType, rtk::InputState>);
static_assert(rtk::Scene<SettingScene, SceneType, rtk::InputState>);

} // namespace rtype::client