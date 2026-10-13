#pragma once

#include "GameEngine.hpp"
#include "collision/collisionBuffer.hpp"
#include "inputAction/InputAction.hpp"
#include "inputAction/inputMapping.hpp"

#include "sceneManager/sceneManager.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"

#include "rtkWindowAdapter.hpp"
#include "rtkAudiManagerAdapter.hpp"

#include "scene/gameScene.hpp"
#include "scene/networkConnectionScene.hpp"
#include "scene/menuScene.hpp"
#include "NetworkClient.hpp"

namespace rtype::events {
    struct PlayerShotEvent {};
}

namespace rtype::client {
class Game {
public:
  Game();
  ~Game() = default;

  void onLoad(rtk::EngineContext &context);
  bool shouldContinue() const;
  void onUpdate(rtk::EngineContext &context, float dt);
  void onUnload(rtk::EngineContext &context);
  
private:
  void registerBuilders(rtk::EngineContext &context);
  void registerComponents(rtk::EngineContext &context);
  void createBindings();
  void subscribeEvents(rtk::EngineContext &context, uint32_t shootSoundId);
  void addSystems(rtk::EngineContext &context);
  void loadEntityBluePrint(rtk::EngineContext &context);
  void handleReplication(rtk::EngineContext &context);
  void createReplicatedEntity(rtk::EngineContext &context, const protocol::CreateEntityPayload &payload);
  void destroyReplicatedEntity(rtk::EngineContext &context, const protocol::DestroyEntityPayload &payload);
  void applySnapshot(rtk::EngineContext &context, const protocol::SnapshotPayload &payload);

  std::unordered_map<protocol::NetworkEntityId,std::size_t> _networkEntities;
  std::uint32_t _lastServerTick = 0;
  rtk::RenderWindow _window;
  RtkWindowAdapter _adapter;
  RtkAudioAdapter _adapterAudioManager;
  rtk::InputState _inputState{};
  rtk::collision::CollisionBuffer _collisionBuffer;
  rtk::Input::InputMapping _inputMapping;
  rtype::client::NetworkClient _networkClient;

  rtk::SceneManager<SceneType, rtk::RenderWindow, rtk::InputState, MenuScene, NetworkConnectionScene,
                    GameScene>
      _sceneManager;
};
} // namespace rtype::client
