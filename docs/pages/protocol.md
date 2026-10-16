# Network protocol specification {#protocol}

[TOC]

| Item | Value |
| ---- | ----- |
| Protocol version | 1 |
| Transport | UDP over IPv4 |
| Default server port | 4242 |
| Byte order | Big-endian (network order) |
| Architecture | Authoritative server |

## 1. Introduction

This document specifies the binary protocol spoken between the R-Type server
and its clients. It is complete enough to write a new client, a bot or a
network analysis tool without reading the source code.

The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT** and **MAY**
are to be read as described in RFC 2119.

The server is authoritative. A client sends only the actions of its player and
displays the world described by the server. A client MUST NOT assume that an
entity exists, moves or disappears unless the server said so.

## 2. Conventions

### 2.1 Data types

| Type | Size | Encoding |
| ---- | ---: | -------- |
| `u8` | 1 byte | Unsigned integer. |
| `u16` | 2 bytes | Unsigned integer, big-endian. |
| `u32` | 4 bytes | Unsigned integer, big-endian. |
| `pos16` | 2 bytes | Position, see @ref protocol_position "section 9.1". |
| `rot16` | 2 bytes | Rotation, see @ref protocol_rotation "section 9.2". |

Structures are packed: there is no padding and no alignment between fields.
Offsets in the tables below are counted in bytes from the start of the
structure.

### 2.2 Size limits

| Limit | Value | Meaning |
| ----- | ----: | ------- |
| Maximum datagram | 548 bytes | 576 byte minimum IPv4 reassembly size, minus 20 bytes of IPv4 header and 8 bytes of UDP header. |
| Packet header | 16 bytes | Start of every connected packet. |
| Maximum message area | 532 bytes | Room for message headers and payloads in one packet. |
| Maximum payload of one message | 528 bytes | Message area minus one 4 byte message header. |

A sender MUST NOT send a datagram larger than 548 bytes. A receiver MUST
discard any datagram larger than 548 bytes. Keeping datagrams under this size
avoids IP fragmentation on every network path.

## 3. Datagram kinds

There are two kinds of datagrams:

| Kind | Used | Starts with |
| ---- | ---- | ----------- |
| Handshake datagram | Before the connection exists | A 6 byte handshake header (section 4) |
| Connected packet | After `Welcome` | A 16 byte packet header (section 5) |

The receiver tells them apart by the sender address, not by content:

- **Server.** A datagram coming from an address that has no connection is
  parsed as a handshake datagram. A datagram from an address that has a
  connection is parsed as a connected packet.
- **Client.** Datagrams received before `Welcome` are parsed as handshake
  datagrams, the following ones as connected packets. A client MUST ignore
  datagrams that do not come from the server address and port.

## 4. Handshake

### 4.1 Handshake header

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `magic` | `u32` | MUST be `0x52545950` ("RTYP" in ASCII). |
| 4 | 1 | `version` | `u8` | Protocol version, MUST be `1`. |
| 5 | 1 | `type` | `u8` | Handshake type, see below. |

| `type` | Name | Direction | Total size |
| -----: | ---- | --------- | ---------: |
| 1 | `Hello` | Client to server | 6 bytes |
| 2 | `Welcome` | Server to client | 10 bytes |
| 3 | `Rejected` | Server to client | 6 bytes |

Any other `type` value is invalid.

### 4.2 Hello

Sent by a client to open a connection. It is the 6 byte handshake header
alone, with `type` set to 1.

The client SHOULD resend `Hello` every second until it receives `Welcome` or
`Rejected`, and SHOULD give up after 10 seconds.

On receiving `Hello`, the server:

1. MUST silently drop the datagram if the magic number, the version or the
   type is invalid.
2. Otherwise creates a connection bound to the sender address, with a new
   connection id, and answers with `Welcome`.

### 4.3 Welcome

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 6 | header | | Handshake header with `type` set to 2. |
| 6 | 4 | `connectionId` | `u32` | Id of the new connection, never 0. |

The client MUST check that the datagram is exactly 10 bytes long and that
`connectionId` is not 0, and MUST ignore it otherwise. Once accepted, the
client is connected and writes `connectionId` in the header of every packet it
sends.

### 4.4 Rejected

The 6 byte handshake header alone, with `type` set to 3. The client MUST stop
trying to connect. The client MUST ignore a `Rejected` datagram that has extra
bytes.

## 5. Connected packets

### 5.1 Packet header

Every datagram sent on an open connection, in both directions, starts with
this header.

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `connectionId` | `u32` | Id received in `Welcome`. |
| 4 | 4 | `sequence` | `u32` | Sequence number of this packet. |
| 8 | 4 | `ack` | `u32` | Highest sequence number received from the peer, 0 if none. |
| 12 | 4 | `ackBits` | `u32` | Bit `i` set when sequence `ack - 1 - i` was received. |

The header is followed by zero or more messages (section 5.3) that fill the
rest of the datagram exactly.

### 5.2 Sequence numbers and acknowledgements

Each side keeps its own outgoing counter. The first packet sent on a connection
has `sequence` 1, and every following packet increases it by 1.

Each side also remembers the highest sequence received and which of the 32
previous ones were received. When a packet arrives, the receiver classifies its
`sequence`:

| Case | Classification | Action |
| ---- | -------------- | ------ |
| First packet ever received | New | Process. |
| Greater than the highest received | New | Process, becomes the highest. |
| Equal to the highest, or already seen | Duplicate | MUST drop. |
| Lower than the highest by 1 to 32, not seen yet | Out of order | Process. |
| Lower than the highest by more than 32 | Too old | MUST drop. |

`ack` and `ackBits` describe the state above at the time of sending. A sender
MAY use them to measure loss or to retransmit important messages.

### 5.3 Messages

Each message is a 4 byte header followed by its payload.

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 2 | `type` | `u16` | Message type, see section 6. |
| 2 | 2 | `size` | `u16` | Payload size in bytes, header excluded. |
| 4 | `size` | payload | | Message specific content. |

A receiver MUST drop the whole packet if a message header is truncated, if a
`size` is larger than the bytes left in the datagram, or if bytes remain after
the last complete message. A packet MAY contain several messages, and a packet
with no message is valid.

### 5.4 Server checks

The server MUST drop a connected packet if:

- it is shorter than 16 bytes,
- `connectionId` does not match an open connection,
- the connection with that id is bound to another address,
- its sequence number is a duplicate or too old.

The client MUST drop a connected packet whose `connectionId` is not its own.

## 6. Message catalog

Types `0x01` to `0x7F` are sent by clients, types `0x80` to `0xFF` by the
server.

| Type | Name | Direction | Payload size | Section |
| ---: | ---- | --------- | -----------: | ------- |
| `0x0002` | `Input` | Client to server | 9 | 7.1 |
| `0x0003` | `Ping` | Client to server | 4 | 7.2 |
| `0x0004` | `Disconnect` | Client to server | 1 | 7.3 |
| `0x0013` | `QueryEntity` | Client to server | 4 | 7.4 |
| `0x0083` | `Snapshot` | Server to client | 6 + 11 n | 8.1 |
| `0x0084` | `CreateEntity` | Server to client | 16 | 8.2 |
| `0x0085` | `DestroyEntity` | Server to client | 9 | 8.3 |
| `0x0086` | `Pong` | Server to client | 4 | 8.4 |

A receiver MUST ignore messages whose type it does not know, and keep
processing the other messages of the packet. Type `0` is invalid and is never
sent.

## 7. Client to server messages

### 7.1 Input (0x0002)

Actions held by the player during one client frame.

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `inputSequence` | `u32` | Starts at 1, increases by 1 for each `Input` sent. |
| 4 | 4 | `clientTick` | `u32` | Client frame counter. |
| 8 | 1 | `actions` | `u8` | Bit flags, see below. |

| Bit | Value | Action |
| --: | ----: | ------ |
| 0 | `0x01` | Up |
| 1 | `0x02` | Down |
| 2 | `0x04` | Left |
| 3 | `0x08` | Right |
| 4 | `0x10` | Shoot |

Bits 5 to 7 are reserved and MUST be 0. Several bits can be set at once, for
example Up, Right and Shoot give `0x19`.

`actions` is the full state of the controls, not a change: the client SHOULD
send one `Input` per frame, including frames where nothing is held (`0x00`).

The server:

- MUST drop the message if the payload is not exactly 9 bytes.
- MUST drop it if `inputSequence` is not newer than the last accepted one for
  this connection. "Newer" is computed as a signed 32-bit difference, so the
  counter MAY wrap around.
- Sets the ship velocity from the direction bits. If two opposite directions
  are held, Down and Right win. Diagonal speed is scaled by 0.7071 so that it
  matches the straight speed.
- Fires a missile when the Shoot bit goes from set to cleared.

### 7.2 Ping (0x0003)

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `timestamp` | `u32` | Any value chosen by the client, usually a time in milliseconds. |

The payload MUST be exactly 4 bytes. The server answers with a `Pong` that
carries the same `timestamp`. The difference between the reception time of the
`Pong` and `timestamp` gives the round trip time.

### 7.3 Disconnect (0x0004)

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 1 | `reason` | `u8` | Why the client leaves, see below. |

| Value | Reason |
| ----: | ------ |
| 1 | Client quit |
| 2 | Timeout |
| 3 | Protocol error |
| 4 | Server shutdown |

A client SHOULD send `Disconnect` with reason 1 when the player quits. On
receiving it, the server closes the connection at once, removes the player
ship and sends `DestroyEntity` with reason 3 to the other players. The server
does not interpret `reason`.

### 7.4 QueryEntity (0x0013)

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `networkId` | `u32` | Entity the client does not know, never 0. |

A client SHOULD send it once when a `Snapshot` mentions a network id for which
it never received `CreateEntity`. If the entity still exists, the server answers
with a `CreateEntity` describing it. Otherwise it does not answer. The server
MUST drop the message if the payload is not exactly 4 bytes or if `networkId`
is 0.

## 8. Server to client messages

### 8.1 Snapshot (0x0083)

Positions of the replicated entities at a given server tick.

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `serverTick` | `u32` | Server tick of the states. |
| 4 | 2 | `entityCount` | `u16` | Number of entity states that follow. |
| 6 | 11 x `entityCount` | states | | Entity states, see below. |

Each entity state:

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `networkId` | `u32` | Entity id, never 0. |
| 4 | 1 | `entityType` | `u8` | Low 8 bits of the archetype id, informative only. |
| 5 | 2 | `positionX` | `pos16` | Horizontal position. |
| 7 | 2 | `positionY` | `pos16` | Vertical position. |
| 9 | 2 | `rotation` | `rot16` | Rotation. |

One `Snapshot` holds at most 47 entity states. When more entities exist, the
server sends several `Snapshot` messages with the same `serverTick`, each in
its own packet.

The client MUST drop the message if:

- `entityCount` is greater than 47,
- the payload size is not exactly `6 + 11 x entityCount`,
- any `networkId` is 0,
- the same `networkId` appears twice.

The client sets the position of every known entity to the received values.
Snapshots are not deltas: each one carries the full position of the entities it
lists. The server sends one every 3 ticks, which is about 20 per second.

### 8.2 CreateEntity (0x0084)

Asks the client to create the local copy of a server entity.

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `serverTick` | `u32` | Server tick at sending time. |
| 4 | 4 | `networkId` | `u32` | Entity id, never 0. |
| 8 | 2 | `entityType` | `u16` | Archetype id, see @ref protocol_archetypes "section 9.3". |
| 10 | 2 | `positionX` | `pos16` | Horizontal position. |
| 12 | 2 | `positionY` | `pos16` | Vertical position. |
| 14 | 2 | `rotation` | `rot16` | Rotation. |

The client MUST drop the message if the payload is not exactly 16 bytes or if
`networkId` is 0. If an entity with this `networkId` already exists, the client
MUST ignore the message.

The server sends `CreateEntity`:

- to a new player, for every entity already in the world,
- to every player, when a player joins, an enemy spawns or a missile is fired,
- to one player, in answer to `QueryEntity`.

### 8.3 DestroyEntity (0x0085)

Asks the client to remove an entity.

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `serverTick` | `u32` | Server tick at sending time. |
| 4 | 4 | `networkId` | `u32` | Entity id, never 0. |
| 8 | 1 | `reason` | `u8` | Why the entity is removed, see below. |

| Value | Reason |
| ----: | ------ |
| 0 | Unknown |
| 1 | Removed by gameplay (hit, killed, left the playfield) |
| 2 | Out of bounds |
| 3 | Player disconnected |
| 4 | Game ended |

The client MUST drop the message if the payload is not exactly 9 bytes, if
`networkId` is 0 or if `reason` is greater than 4. The client SHOULD remember
recently destroyed ids so that it does not query them again when a late
`Snapshot` still lists them.

### 8.4 Pong (0x0086)

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `timestamp` | `u32` | Copied from the matching `Ping`. |

## 9. Value encodings

### 9.1 Positions {#protocol_position}

Positions are in pixels, with the origin at the top left of the screen, x
growing to the right and y growing downwards. They are encoded as 16-bit fixed
point numbers with a resolution of 0.25 pixel.

```text
encoded = round((position + 8192) * 4)
position = encoded / 4 - 8192
```

The encodable range is -8192 to 8191.75 pixels. A sender MUST clamp values to
this range and MUST encode a non finite value as the encoding of 0.

Example: x = 2000 gives `(2000 + 8192) * 4 = 40768`, sent as `9F 40`.

### 9.2 Rotations {#protocol_rotation}

Rotations are encoded as a fraction of a full turn.

```text
encoded = round(fraction(radians / 2 pi) * 65536) modulo 65536
radians = encoded / 65536 * 2 pi
```

`0` is 0 radians, `16384` is pi / 2, `32768` is pi. Negative angles wrap to the
positive range.

### 9.3 Archetype ids {#protocol_archetypes}

`entityType` identifies the blueprint used to build an entity. It is a 16-bit
hash of the blueprint name, computed the same way by every peer:

1. Compute the 32-bit FNV-1a hash of the name bytes: start from `2166136261`,
   and for each byte, XOR the byte into the hash then multiply by `16777619`
   modulo 2^32.
2. Fold it to 16 bits: `(hash >> 16) XOR (hash AND 0xFFFF)`.

Blueprints currently used by the server:

| Blueprint | Archetype id | Entity |
| --------- | -----------: | ------ |
| `player_blue` | `0xBD3E` | Ship of player slot 1 |
| `player_green` | `0x54EE` | Ship of player slot 2 |
| `player_pink` | `0x8F7A` | Ship of player slot 3 |
| `player_red` | `0xFAD4` | Ship of player slot 4 |
| `bullet` | `0x4BED` | Player missile |
| `enemyBidos` | `0x87FF` | Bydos enemy |
| `low_ground` | `0x7EAF` | Stage tile |
| `medium_ground` | `0xA6A3` | Stage tile |
| `high_ground` | `0x189D` | Stage tile |
| `ramp` | `0xBE70` | Stage tile |
| `reactor` | `0x6C78` | Stage tile |
| `gate` | `0xFD72` | Stage tile |

Stage tiles placed at y = 0 belong to the ceiling and SHOULD be drawn flipped
vertically.

A client that receives an unknown archetype id SHOULD ignore that entity.

## 10. Session flow

\dot "Message sequence of a typical session" width=100%
digraph session {
  rankdir=TB;
  node [shape=box, style="rounded,filled", fillcolor="#eef3fb", color="#4a6fa5", fontsize=10];
  edge [fontsize=9];
  h  [label="Client: Hello (repeated every second)"];
  w  [label="Server: Welcome(connectionId)"];
  ce [label="Server: CreateEntity for each existing entity"];
  sp [label="Server: CreateEntity of the new ship, to every player"];
  lp [label="Loop, about 60 times per second:\nClient: Input\nServer: Snapshot every 3 ticks,\nCreateEntity and DestroyEntity on events"];
  q  [label="Client: QueryEntity for an unknown id\nServer: CreateEntity answer"];
  d  [label="Client: Disconnect\nServer: DestroyEntity(reason 3) to the others"];
  h -> w -> ce -> sp -> lp -> d;
  lp -> q [style=dashed]; q -> lp [style=dashed];
}
\enddot

**Text description of the diagram.** The client sends `Hello` every second
until the server answers with `Welcome`. The server then sends a `CreateEntity`
for every entity already in the world, spawns the ship of the new player and
sends its `CreateEntity` to every player. During the game the client sends an
`Input` every frame and the server sends a `Snapshot` every 3 ticks, plus
`CreateEntity` and `DestroyEntity` when entities appear or disappear. When a
snapshot lists an unknown entity, the client sends `QueryEntity` and the server
answers with `CreateEntity`. The session ends when the client sends
`Disconnect`, and the server then tells the remaining players that the ship is
gone.

Game rules a client should know:

| Rule | Value |
| ---- | ----- |
| Server tick rate | About 60 per second |
| Snapshot rate | One every 3 ticks |
| Round start | 2 players connected, or 30 seconds after the first player joined |
| Player slots | 4, one ship color per slot |
| Spawn position of slot n (0 to 3) | x = 150, y = 150 + 100 n |
| Return to lobby | When every player has left |

## 11. Examples

All bytes are written in hexadecimal.

### 11.1 Hello

```text
52 54 59 50  01  01
magic        ver type=Hello
```

### 11.2 Welcome with connection id 1

```text
52 54 59 50  01  02         00 00 00 01
magic        ver type=Welcome connectionId=1
```

### 11.3 Packet carrying one Input

Connection 1, packet sequence 5, the client last received server packet 120
and the 32 before it. The input is number 42, at client frame 300, with Up and
Shoot held.

```text
00 00 00 01   connectionId = 1
00 00 00 05   sequence     = 5
00 00 00 78   ack          = 120
FF FF FF FF   ackBits      = all 32 previous packets received
00 02         message type = Input
00 09         payload size = 9
00 00 00 2A   inputSequence = 42
00 00 01 2C   clientTick    = 300
11            actions       = Up | Shoot
```

Total size: 29 bytes.

### 11.4 CreateEntity message for a Bydos

Network id 1005 at x = 2000, y = 300, rotation 0, server tick 600. Only the
message is shown, it follows a packet header.

```text
00 84         message type = CreateEntity
00 10         payload size = 16
00 00 02 58   serverTick   = 600
00 00 03 ED   networkId    = 1005
87 FF         entityType   = enemyBidos
9F 40         positionX    = 2000
84 B0         positionY    = 300
00 00         rotation     = 0
```

## 12. Security considerations

Every datagram comes from an untrusted source. Implementations MUST:

- check the remaining length before every read, and never trust a length field
  without comparing it to the bytes actually received,
- reject fixed size payloads whose size differs from the expected one,
- reject enum values out of range and network ids equal to 0,
- bound every count read from the network before allocating memory, such as
  `entityCount` of `Snapshot`,
- discard invalid datagrams silently, without answering, so that the protocol
  cannot be used to amplify traffic.

The protocol provides no encryption and no authentication. Addresses can be
spoofed, and connection ids are allocated sequentially from 1, so they must not
be treated as secrets.

## 13. Known limitations of version 1

These points are known and planned for a future version. A new client must
nevertheless cope with them.

- The server never sends `Rejected` at the moment: an invalid `Hello` is
  dropped and the client times out.
- If `Welcome` is lost, the server already has a connection for the client
  address and drops the following `Hello` datagrams. The client then times out
  and has to reconnect from another port.
- The server has no inactivity timeout: a client that disappears without
  `Disconnect` keeps its connection and its ship.
- `CreateEntity` and `DestroyEntity` are not retransmitted. A lost creation is
  repaired by `QueryEntity`, a lost destruction is not.
- `Welcome` does not tell the client which ship is its own.
