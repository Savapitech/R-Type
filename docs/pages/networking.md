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
| PacketHeader | 16 byte header of every connected packet: connection id, sequence, ack, ack bits. |
| MessageHeader | 4 byte header of every message: type and payload size. |
| MessageParser | Splits a packet body into messages. Rejects the whole packet if any size is inconsistent. |
| PacketTracker | Numbers outgoing packets and classifies incoming ones as new, duplicate, out of order or too old. |
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
  and NetworkServer::setOnMessage(). rtype::server::Server decodes the message
  and pushes the result into one of three queues, each protected by its own
  mutex: inputs, connection changes and entity queries. The game loop drains
  them at the start of each tick with rtype::server::Server::pollInput(),
  rtype::server::Server::pollConnectionEvent() and
  rtype::server::Server::pollQueryEntity().
- **Sending.** NetworkServer::send() posts a task to the network thread, which
  builds the packet header and hands the datagram to the transport.

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
  each side knows which of the last 33 packets the peer received. They are the
  basis for future selective retransmission.

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
