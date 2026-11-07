#pragma once

#include <cstdint>
#include <cstring>

namespace pong::protocol {

static constexpr std::uint32_t PONG_MAGIC = 0x504F4E47; // 'P' 'O' 'N' 'G'

enum class PacketType : std::uint8_t {
  ClientConnect = 0x01,
  ServerAssignSlot = 0x02,
  ClientInput = 0x03,
  ServerSnapshot = 0x04
};

enum class MatchState : std::uint8_t {
  WaitingForPlayers = 0,
  InGame = 1,
  GameOver = 2
};

#pragma pack(push, 1)

struct Header {
  std::uint32_t magic = PONG_MAGIC;
  PacketType type = PacketType::ClientConnect;
};

struct ClientConnectMsg {
  Header header{PONG_MAGIC, PacketType::ClientConnect};
};

struct ServerAssignSlotMsg {
  Header header{PONG_MAGIC, PacketType::ServerAssignSlot};
  std::uint8_t playerSlot = 0; // 0 = Player 1 (Left), 1 = Player 2 (Right), 2 = Spectator
  MatchState matchState = MatchState::WaitingForPlayers;
};

struct ClientInputMsg {
  Header header{PONG_MAGIC, PacketType::ClientInput};
  std::uint8_t playerSlot = 0;
  std::uint8_t inputMask = 0; // bit 0 = Up, bit 1 = Down
};

struct ServerSnapshotMsg {
  Header header{PONG_MAGIC, PacketType::ServerSnapshot};
  float paddleLeftY = 328.0f;
  float paddleRightY = 328.0f;
  float ballX = 635.0f;
  float ballY = 355.0f;
  std::uint8_t scoreP1 = 0;
  std::uint8_t scoreP2 = 0;
  MatchState matchState = MatchState::WaitingForPlayers;
  std::uint8_t gameOver = 0; // 1 if score reached 10
};

#pragma pack(pop)

} // namespace pong::protocol
