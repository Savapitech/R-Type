#include "GameInstance.hpp"
#include "EngineContext.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>


namespace rtype::server {
  GameInstance::GameInstance(std::uint32_t roomId)
    : _roomId(roomId)
  {}

  std::uint32_t GameInstance::getRoomId() const {
    return _roomId;
  }

  std::uint32_t GameInstance::getServerTick() const {
    return _serverTick;
  }

  void GameInstance::addPlayer(std::uint32_t connectionId, std::size_t entity) {
    _players.insert_or_assign(connectionId, entity);
  }

  bool GameInstance::removePlayer(std::uint32_t connectionId) {
    return _players.erase(connectionId) > 0;
  }

  void GameInstance::incrementServerTick() {
    _serverTick++;
  }

  bool GameInstance::hasPlayer(std::uint32_t connectionId) const {
    return _players.find(connectionId) != _players.end();
  }

  std::size_t GameInstance::getPlayerCount() const {
    return _players.size();
  }

  const std::unordered_map<std::uint32_t, std::size_t> &GameInstance::getPlayers() const {
    return _players;
  }

  bool GameInstance::isActive() const {
    return _isActive;
  }

  void GameInstance::setGameActive(bool active) {
    _isActive = active;
  }

  void GameInstance::resetGameState() {
    _isActive = false;
    _serverTick = 0;
    _players.clear();
  }

  rtk::EngineContext &GameInstance::getContext() {
    return _context;
  }

  void GameInstance::start() {
    if (_systemsStarted)
      return;
    
    _scheduler.start(_registry) ;
    _systemsStarted = true;
  }
  
  void GameInstance::update(float dt) {
    if (!_systemsStarted)
      return;

    _scheduler.run(_registry, dt);
    _registry.flush();
  }
  
  void GameInstance::stop() {
    if (!_systemsStarted)
      return;

    _scheduler.stop(_registry);
    _systemsStarted = false;
  }

} //namespace rtype::server
