# Networking {#networking}

[TOC]

This page explains how the network code is organized and how the world is
replicated from the server to the clients. The exact byte layout of every
packet is given in @ref protocol.

## Layers

\dot "Network stack on the server and on the client" width=100%
digraph netstack {
  rankdir=TB;
  node [shape=box, style="rounded,filled", fillcolor="#fbf3e4", color="#8a5a00"];
  edge [color="#555555"];
  subgraph cluster_s {
    label="Server"; style="rounded";
    sgame [label="rtype::server::Game\nsimulation", fillcolor="#eef3fb", color="#4a6fa5"];
    srv   [label="rtype::server::Server\ngame messages, queues", fillcolor="#eef3fb", color="#4a6fa5"];
    ns    [label="NetworkServer\nhandshake, connections, Ping, Disconnect"];
    conn  [label="Connection + PacketTracker\nsequence and acks"];
    frame [label="PacketHeader, MessageParser,\nPacketReader, PacketWriter"];
    udp   [label="UdpTransport\nAsio UDP socket"];
    sgame -> srv -> ns; ns -> conn; ns -> frame; ns -> udp;
  }
  subgraph cluster_c {
    label="Client"; style="rounded";
    cgame [label="rtype::client::Game\nreplication", fillcolor="#eef3fb", color="#4a6fa5"];
    nc    [label="rtype::client::NetworkClient\nhandshake, inputs, decoding,\nAsio UDP socket"];
    cframe [label="PacketHeader, MessageParser,\nPacketTracker"];
    cgame -> nc -> cframe;
  }
}
\enddot

**Text description of the diagram.** On the server, the game talks to
rtype::server::Server, which decodes and encodes game messages. Below it,
NetworkServer runs the handshake, keeps one Connection per client, answers
`Ping` and handles `Disconnect`. Each Connection owns a PacketTracker for
sequence numbers and acknowledgements. Framing is done by PacketHeader and
MessageParser on top of the bounds checked PacketReader and PacketWriter.
UdpTransport owns the socket. On the client, rtype::client::NetworkClient
combines the handshake, the socket and the decoding in one class, and reuses
the same framing and tracking classes.

## Network library

The network library in `lib/Network/src` is independent from the game. It
deals with bytes, connections and message framing, never with entities.

| Class | Role |
| ----- | ---- |
| PacketReader | Reads big-endian integers and byte ranges. Every read checks the remaining size first and fails instead of overflowing. |
| PacketWriter | Appends big-endian integers. Refuses to grow past MAX_PAYLOAD_SIZE (548 bytes). |
| HandshakeHeader | Header of `Hello`, `Welcome` and `Rejected`. Checks the magic number and the type. |
| PacketHeader | 19 byte header of every connected packet: connection id, sequence, ack, ack bits, priority, reliable id. |
| MessageHeader | 4 byte header of every message: type and payload size. |
| MessageParser | Splits a packet body into messages. Rejects the whole packet if any size is inconsistent. |
| PacketTracker | Numbers outgoing packets, classifies incoming ones as new, duplicate, out of order or too old, queues outgoing packets by priority and resends the unacknowledged ones. |
| Connection | Connection id, client endpoint and tracker of one client. |
| UdpTransport | Asynchronous IPv4 UDP socket with a receive loop and a send queue. |
| NetworkServer | Owns the network thread, the transport and the connection table. |
| NetworkMetrics | Byte, packet, loss and round trip time counters. |

## Threading model

NetworkServer::start() launches one thread that runs the Asio `io_context`.
Every receive handler, send handler and callback runs on that thread. The game
loop never touches the socket directly:

- **Receiving.** NetworkServer calls the callbacks set with
  NetworkServer::setOnClientConnected(), NetworkServer::setOnClientDisconnected()
  and NetworkServer::setOnMessage(). rtype::server::Server decodes the message.
  Connection changes and room requests go into queues protected by a mutex,
  which the main thread drains at the start of each frame with
  rtype::server::Server::pollConnectionEvent() and
  rtype::server::Server::pollRoomRequest(). Inputs and entity queries go to the
  callbacks set with rtype::server::Server::setInputHandler() and
  rtype::server::Server::setQueryEntityHandler(), which hand them to the room of
  the client (see @ref room_threads).
- **Sending.** NetworkServer::send() can be called from any thread. It posts a
  task to the network thread, which only queues the messages. They leave when
  NetworkServer::flushAll() runs at the end of a room tick (or when the flush
  timer fires), grouped in as few datagrams as possible, see
  @ref networking_batching.

The client follows the same model: rtype::client::NetworkClient pushes
connection events and decoded replication messages into queues read with
rtype::client::NetworkClient::pollEvent() and
rtype::client::NetworkClient::pollReplicationEvent().

Because the loop never waits for the network, the server keeps simulating at
a steady rate whatever the clients do, and a client that crashes or stops
sending does not slow anybody down.

## Connection lifecycle

\dot "Connection sequence between a client and the server" width=100%
digraph handshake {
  rankdir=TB;
  node [shape=box, style="rounded,filled", fillcolor="#eef3fb", color="#4a6fa5"];
  edge [fontsize=10];
  a [label="Client sends Hello\n(every second, up to 10 s)"];
  b [label="Server checks magic, version, type\nand creates a Connection"];
  c [label="Server sends Welcome\nwith the connection id"];
  d [label="Server sends CreateEntity for every\nexisting entity, then spawns the new ship\nand broadcasts its CreateEntity"];
  e [label="Client sends Input every frame,\nserver sends Snapshot every 3 ticks"];
  f [label="Client sends Disconnect\nor stops sending"];
  a -> b -> c -> d -> e -> f;
}
\enddot

**Text description of the diagram.** The client sends a `Hello` handshake every
second. The server validates it, creates a Connection with a new id and answers
with `Welcome`. It then sends a `CreateEntity` for every entity already in the
world, spawns a ship for the new player and broadcasts that ship to everybody.
From then on the client sends its inputs every frame and the server sends
snapshots. The connection ends when the client sends `Disconnect`.

Datagrams are routed by source address. A datagram from an unknown address is
treated as a handshake. A datagram from a known address must start with a
PacketHeader whose connection id matches that address, otherwise it is
dropped.

## Replication model

The server owns the world. Clients only display it.

| Event on the server | Message to clients |
| ------------------- | ------------------ |
| A player joins | `CreateEntity` of every entity to the newcomer, then `CreateEntity` of the new ship to everybody |
| An enemy spawns | `CreateEntity` to everybody |
| A player shoots | `CreateEntity` of the missile to everybody |
| Every 3 ticks (about 20 times per second) | `Snapshot` with the position of every replicated entity |
| An entity is destroyed (hit, killed, out of the screen) | `DestroyEntity` to everybody |
| A player disconnects | `DestroyEntity` of the ship to the other players |

On the client:

- `CreateEntity` instantiates the blueprint whose archetype id is given, with
  the given network id and position, unless that network id already exists.
- `DestroyEntity` removes the entity with that network id.
- `Snapshot` overwrites the position of every known entity. When a snapshot
  mentions an unknown network id, the client sends `QueryEntity` once and the
  server answers with a `CreateEntity` for that entity. This recovers from a
  lost `CreateEntity`.

The client sends only its intentions: an `Input` message with the actions held
during the frame. The server turns them into a velocity and fires a missile when
the shoot button is released. The client never creates or destroys a networked
entity on its own authority.

## Packet batching {#networking_batching}

Every datagram costs a fixed overhead whatever its content: 20 bytes of IPv4
header, 8 bytes of UDP header and the 19 bytes of PacketHeader, that is 47 bytes
before the first message. Most messages of the game are small (a `Pong` is 12
bytes, a `DestroyEntity` a few more), so sending each one alone would spend more
bytes on headers than on data, and would cost one system call per message.

The server therefore does not send a message when the game asks for it. It
appends the message to the outgoing queue of its priority and sends the queues
later, all at once:

1. During a tick, the game calls rtype::server::Server::send() as often as it
   needs. NetworkServer only queues the encoded messages.
2. At the end of the tick, rtype::server::RoomWorker calls
   rtype::server::Server::flushAll(), which runs NetworkServer::flush() for every
   connection on the network thread.
3. PacketTracker::collectOutgoing() takes the queued bodies of a priority one
   after the other and concatenates them in a single packet as long as the result
   fits in `MAX_MESSAGE_SIZE` (529 bytes). The next body starts a new packet.
   A message is never split between two packets.

Nothing waits for a fixed delay: the messages produced during a tick are the
messages sent together, so batching adds no latency. The flush timer
(`FLUSH_INTERVAL`, 5 ms) stays as a safety net for resends, acknowledgements and
messages queued outside a tick. A `Pong` is the exception, it is flushed at once
so that the round trip time stays accurate.

Messages are grouped **by priority**, never across priorities, because a packet
has a single `priority` and a single `reliableId`. A batch of `Critical` or
`Reliable` messages is one packet for the tracker: it is acknowledged as a
whole and, when no ack arrives, resent as a whole with the same `reliableId`,
so the receiver still delivers it once. The wire format does not change: a
receiver already reads every message of a packet in a loop (see section 5.3 of
the @ref protocol).

## Compression {#networking_compression}

After batching, the body of a packet (all its messages) is compressed with zstd
before the PacketHeader is put in front of it. The algorithm and its measured
results are justified in @ref technical_study, section 6.4.

- **What is compressed.** The body only. The PacketHeader stays in clear because
  the receiver needs `connectionId`, `sequence`, `ack` and `reliableId` to
  accept the packet before spending time on its content.
- **When.** Once, when PacketTracker::collectOutgoing() takes the packet out of
  the queue. A resend of a `Critical` or `Reliable` packet sends the same bytes
  again and does not compress a second time.
- **Only if it helps.** The compressed body is used only if it is strictly
  smaller. Otherwise the packet is sent as it is. Tiny bodies such as an
  `Input` or a `Pong` are therefore never compressed.
- **Flag.** Bit 7 of the `priority` byte of the PacketHeader is set when the
  body is compressed. The priority is in bits 0 to 6.
- **Format.** A zstd frame at level 1 without magic number, checksum, content
  size or dictionary id, compressed with a 4 KiB dictionary embedded in both the
  client and the server (`lib/Network/src/protocol/zstd_dict.bin`,
  embedded at build time by `cmake/EmbedDictionary.cmake`, like the assets). See compressBody() and
  decompressBody().
- **On reception.** MessageParser::parseBody() decompresses the body, then reads
  the messages exactly as for an uncompressed packet. It runs after the
  PacketTracker accepted the packet, so duplicates and packets of an unknown
  connection never cost a decompression. The output is capped at
  `MAX_MESSAGE_SIZE` bytes, a larger frame is rejected and the packet dropped.
- **Threads.** zstd contexts are not thread safe, so each thread owns its own.
  The digested dictionaries are shared, they are read only.

To tune it, capture packets with `RTYPE_CAPTURE_FILE=<file>` while playing (or
with `rtype_bot`, a headless client built with `-DRTYPE_BUILD_BOT=ON`), then run
`compression_bench <file> <dictionary>` (built with `-DRTYPE_BUILD_BENCH=ON`).

## Hiding the latency {#networking_latency}

The server is the authority, so what a player sees comes from the server and
travels for half a round trip time, and what the player does travels back for
the other half. Three mechanisms keep the game playable when the network is not
perfect. They are measured in @ref network_benchmark and compared with the
alternatives in @ref technical_study, section 6.5.

### Prediction of the local ship

Until a snapshot comes back, the client does not know that the ship moved. It
used to wait, so the ship reacted one round trip time plus up to 50 ms late.
rtype::client::LocalPrediction removes the wait:

1. Every frame, rtk::systems::PlayerSystem asks it to move the ship of the local
   player with the held actions. The movement is protocol::shipVelocity(), the
   very function the server uses, so both agree. The ship moves in the same
   frame as the key press.
2. Every `Snapshot` carries `ownNetworkId` (which ship is mine) and
   `ackedInputTick` (up to which client tick the server has simulated it). The
   client takes the position of the server, replays the movements it made after
   `ackedInputTick`, and obtains where the ship should be now. It moves the
   displayed ship toward that position, half of the gap at each snapshot.
   Gaps above 96 pixels, which come from a hit or a lost packet, are corrected
   at once.
3. A snapshot older than the last one used is ignored, so reordered packets do
   not pull the ship back.

The server computes `ackedInputTick` in GameInstance::sendSnapshots(). It keeps,
for each player, the client tick `c` and the server tick `s` at which the current
actions started, and answers `c + (now - s)`: the ship has moved with these
actions for as many client ticks as server ticks since. This only works if a
client tick is a duration, so ticks follow the clock
(rtype::client::NetworkClient::inputTick()) and not the number of rendered
frames.

Only the ship of the local player is predicted. Everything else is the position
of the last snapshot. Missiles are created by the server at the position of the
server ship, which is a little behind the ship the player sees.

### Input sending

A snapshot is sent every 50 ms, so an input sent every 16 ms is mostly
redundant, and each one costs 31 bytes of which 19 are the packet header.
rtype::client::NetworkClient::sendInput() is called every frame but sends an
`Input` only:

- when the held actions change, and again in the two following frames, so one
  lost packet does not delay a key press (`INPUT_REDUNDANT_COPIES`),
- every 50 ms while they stay the same (`INPUT_HEARTBEAT`), which repairs a
  change that was lost anyway and tells the server that the client is alive.

The server keeps the last actions it received, so nothing else is needed.

### Snapshot rate

When the link of a client cannot carry the snapshots, a queue grows in front of
the link, and all the packets, the player ship corrections included, arrive
later and later. GameInstance::updateSnapshotRates() runs twice per second and
lowers the rate of such a client.

The signal is the round trip time. PacketTracker::takeMinRttSample() measures
it on every acknowledged packet, Unreliable ones included, which gives about 20
samples per second per client. The round trip time used for resends is measured
on reliable packets only, which are too rare for this: on a saturated test link
it reported 28 ms on the client while the real delay was 775 ms. A packet is acknowledged by the next packet of the client, up to
50 ms later because of the input heartbeat, so the controller uses the smallest
sample of each half second, which ignores that delay.

| Condition | Action |
| --------- | ------ |
| Smallest RTT of the last half second more than 60 ms above the smallest RTT seen for this client | Send one snapshot out of 2, then 3, then 4 (20, 10, 6.7 and 5 per second) |
| More than 300 ms above | Go straight to one out of 4 |
| 6 seconds without growth | Go back one step |

The smallest RTT ever seen is the RTT of an empty link, because it is measured
in the lobby where almost nothing is sent. Only snapshots are slowed down.
Creations, destructions and every reliable message keep their normal rate.

## Reliability

UDP datagrams can be lost, duplicated or reordered. The protocol handles this as
follows:

- **Duplicates and very old packets** are detected with the sequence number of
  the PacketHeader and dropped before their messages are read.
- **Old inputs** are dropped by the server when their input sequence is not
  newer than the last accepted one.
- **Lost snapshots** do not matter: the next one carries the full state.
- **Lost creations** are repaired with `QueryEntity`.
- **Acknowledgements** (`ack` and `ackBits`) are sent in every packet so that
  each side knows which of the last 33 packets the peer received. PacketTracker
  uses them to stop resending packets the peer has received.

### Packet priorities

Every packet carries a `priority` (PacketPriority) and a `reliableId` in its
PacketHeader. The outgoing queues are served from the lowest value to the
highest, so a priority 0 packet always leaves before a priority 1 packet.

| Priority | Name | Behaviour |
| -------: | ---- | --------- |
| 0 | `Critical` | Resent until acknowledged. A packet still unacknowledged after `CRITICAL_RESENDS_BEFORE_LOSS` resends counts as a loss, and when `MAX_CRITICAL_LOSSES` losses are reached the connection is closed because the network is too unstable. One loss is forgiven each time `CRITICAL_LOSS_DECAY` passes without a new one (30 s by default, 0 disables it). |
| 1 | `Reliable` | Resent at most `RELIABLE_MAX_RESENDS` times (20 by default), then ignored. |
| 2 | `Unreliable` | Sent once, never resent. This is the default. |

A resend gets a new `sequence` so it stays inside the acknowledgement window,
but keeps the same `reliableId`: the receiver uses it to deliver a packet only
once when both the original and a resend arrive. When a side has priority 0 or 1
packets to acknowledge and nothing to send, it sends an empty packet carrying the
acks after `ACK_ONLY_DELAY`. The delays and limits are the `constexpr` values of `NetworkConfig.hpp`. The
server sends `CreateEntity` and `DestroyEntity` with priority 0, `QueryEntity`
uses priority 1, and snapshots and inputs use priority 2. On a timeout the server
sends a `Disconnect` message (type 0x04, empty payload) and the client reports
a `Disconnected` event.

## Robustness

Every byte coming from the network is validated before use:

- Datagrams larger than 548 bytes are discarded by the transport.
- Every read is bounds checked by PacketReader.
- A message whose declared size exceeds the bytes left, or a packet with
  trailing bytes, is dropped as a whole.
- Fixed size payloads must have exactly their expected size.
- Network ids must not be 0, enum values must be in range, and snapshot entity
  ids must be unique.
- The server only accepts connected packets whose connection id matches the
  sender address.

A malformed datagram is therefore ignored without side effects. The full list of
rules is in @ref protocol.
