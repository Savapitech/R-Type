# Network protocol specification {#protocol}

[TOC]

| Item | Value |
| ---- | ----- |
| Protocol version | 3 |
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
| Packet header | 19 bytes | Start of every connected packet. |
| Maximum message area | 529 bytes | Room for message headers and payloads in one packet. |
| Maximum payload of one message | 525 bytes | Message area minus one 4 byte message header. |

A sender MUST NOT send a datagram larger than 548 bytes. A receiver MUST
discard any datagram larger than 548 bytes. Keeping datagrams under this size
avoids IP fragmentation on every network path.

A server also bounds the state it keeps for untrusted peers:

| Limit | Value | Meaning |
| ----- | ----: | ------- |
| Connections | 4 | Simultaneous connections. Connections are identified by the full address (IP and port), so several players can share one public IP. |
| Inactivity timeout | 10 s | A connection that sends no valid packet for this long is closed. |
| Queued packets | 256 per priority | Packets waiting to be sent to one peer. |
| Unacknowledged packets | 512 | Reliable and Critical packets waiting for an ack. |
| Queued events | 1024 | Decoded messages waiting for the game loop. Further ones are dropped. |

## 3. Datagram kinds

There are two kinds of datagrams:

| Kind | Used | Starts with |
| ---- | ---- | ----------- |
| Handshake datagram | Before the connection exists | A 6 byte handshake header (section 4) |
| Connected packet | After `Welcome` | A 19 byte packet header (section 5) |

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
| 4 | 1 | `version` | `u8` | Protocol version, MUST be `3`. |
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

The 6 byte handshake header alone, with `type` set to 3. The server sends it
when it already holds 4 connections. The client MUST stop
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
| 16 | 1 | `priority` | `u8` | Bits 0 to 6: 0 critical, 1 reliable, 2 unreliable, other values make the packet invalid. Bit 7 (`0x80`): the body is compressed, see section 5.5. |
| 17 | 2 | `reliableId` | `u16` | Identifies a priority 0 or 1 packet, kept by its resends. 0 for priority 2. |

The header is followed by the body: zero or more messages (section 5.3) that
fill the rest of the datagram exactly, or a compressed body (section 5.5).

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

`ack` and `ackBits` describe the state above at the time of sending. A client
MUST fill them in every packet it sends. The server uses them to retransmit
its priority 0 and 1 packets: a priority 0 packet still unacknowledged after 10
resends counts as a loss, and after 3 losses the server closes the connection
as too unstable. A client that never acknowledges is therefore disconnected
after a few seconds. A client with nothing to send SHOULD still send a packet
without message (header only) so that its acknowledgements reach the server.

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

A sender SHOULD put in one packet all the messages of the same priority it has
to send at the same time, up to 529 bytes, instead of sending one packet per
message. Messages of different priorities cannot share a packet, because a
packet has a single `priority` and a single `reliableId`. A batch of priority 0
or 1 messages is acknowledged and resent as a whole.

### 5.4 Server checks

The server MUST drop a connected packet if:

- it is shorter than 19 bytes or its `priority` is greater than 2,
- `connectionId` does not match an open connection,
- the connection with that id is bound to another address,
- its sequence number is a duplicate or too old, or its `reliableId` was already delivered.
- its body is flagged as compressed and cannot be decompressed within 529 bytes (section 5.5).

The client MUST drop a connected packet whose `connectionId` is not its own.

### 5.5 Compressed bodies

When bit 7 of `priority` is set, everything after the 19 byte header is a
single compressed frame instead of messages. The frame is a zstd frame
(RFC 8878) with these properties:

- the format is "magicless": the 4 byte magic number is absent,
- no checksum, no content size and no dictionary id are present,
- it is compressed with the dictionary of the protocol version, 4096 bytes,
  published in `lib/Network/src/protocol/zstd_dict.bin`.

Once decompressed, the bytes are read as in section 5.3. The sender MUST set the
flag only if the frame is strictly smaller than the uncompressed body, and
compresses a packet once: a resend carries the same bytes with the same flag.

The receiver MUST decompress only after the packet passed the checks of section
5.4 and MUST limit the output to 529 bytes. It MUST drop the packet if the
frame is empty, invalid, or expands to more than 529 bytes.

## 6. Message catalog

Types `0x01` to `0x7F` are sent by clients, types `0x80` to `0xFF` by the
server.

| Type | Name | Direction | Payload size | Section |
| ---: | ---- | --------- | -----------: | ------- |
| `0x0002` | `Input` | Client to server | 9 | 7.1 |
| `0x0003` | `Ping` | Client to server | 4 | 7.2 |
| `0x0004` | `Disconnect` | Client to server | 1 | 7.3 |
| `0x0013` | `QueryEntity` | Client to server | 4 | 7.4 |
| `0x0025` | `ChatSend` | Client to server | 1 + n | 7.5 |
| `0x0026` | `SetUsername` | Client to server | n | 7.6 |
| `0x0027` | `GetScoreboard` | Client to server | 0 | 7.7 |
| `0x0083` | `Snapshot` | Server to client | 14 + 11 n | 8.1 |
| `0x0084` | `CreateEntity` | Server to client | 16 | 8.2 |
| `0x0085` | `DestroyEntity` | Server to client | 9 | 8.3 |
| `0x0086` | `Pong` | Server to client | 4 | 8.4 |
| `0x0096` | `ChatMessage` | Server to client | 9 + n | 8.5 |
| `0x0097` | `Scoreboard` | Server to client | n | 8.6 |
| `0x0098` | `ScoreUpdate` | Server to client | 4 | 8.7 |
| `0x0099` | `PowerUpTaken` | Server to client | 3 | 8.8 |

A receiver MUST ignore messages whose type it does not know, and keep
processing the other messages of the packet. Type `0` is invalid and is never
sent.

## 7. Client to server messages

### 7.1 Input (0x0002)

Actions held by the player at a given client tick.

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `inputSequence` | `u32` | Starts at 1, increases by 1 for each `Input` sent. |
| 4 | 4 | `clientTick` | `u32` | Client tick, see below. |
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

`actions` is the full state of the controls, not a change, and `0x00` means
that nothing is held. `clientTick` counts ticks of 1/60 second on the client
clock, from any origin. It follows the clock and not the number of rendered
frames, because the server uses the difference between two ticks as a duration
(section 10.1).

The server keeps applying the last actions it received until it receives other
ones, so a client does not have to send one `Input` per frame. A client SHOULD
send an `Input`:

- as soon as the actions change, and again in each of the two following frames,
  so that the loss of one packet does not delay the change,
- then, while the actions stay the same, every 100 ms. This repairs a change that
  was lost anyway, and tells the server that the client is alive.

A player who holds the same keys sends about 10 `Input` per second instead of
60. Each repetition has its own `inputSequence`.

The server:

- MUST drop the message if the payload is not exactly 9 bytes.
- MUST drop it if `inputSequence` is not newer than the last accepted one for
  this connection. "Newer" is computed as a signed 32-bit difference, so the
  counter MAY wrap around.
- Sets the ship velocity from the direction bits. If two opposite directions
  are held, Down and Right win. Diagonal speed is scaled by 0.7071 so that it
  matches the straight speed. The client uses the same rule to predict its ship
  (section 10.1).
- Starts a new "run" of actions when they differ from the previous ones, and
  keeps the run when they are the same (a repetition does not restart it).
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

### 7.5 ChatSend (0x0025)

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 1 | `length` | `u8` | Text length `n`, 1 to 64. |
| 1 | n | `text` | bytes | Message, not null terminated. |

Sent reliably. The text is made only of the characters drawn by the game font:
`a-z`, `A-Z`, `0-9`, space and `! , . / ; ? _`, and it is not only spaces. The
server MUST drop the message if the text is invalid, if bytes follow it, or if
the sender is not in a room. It also drops a message sent less than 250 ms
after the previous accepted one of the same client. Otherwise it sends a
`ChatMessage` to every player of the sender's room, the sender included.

### 7.6 SetUsername (0x0026)

The payload is the username itself, 1 to 16 bytes among `a-z`, `A-Z`, `0-9`
and `_`, without length prefix nor terminator. Sent reliably by a client
started with `--username`, right after `Welcome`. At the end of each round, the
server adds the score of the connection to the stats saved under this name in
its `scores.json` file. A client that never sends it plays without saving its
scores. The server MUST drop the message if the name is invalid.

A player earns 100 points for each enemy destroyed by one of its projectiles.

### 7.7 GetScoreboard (0x0027)

Empty payload, sent reliably. The server answers with a `Scoreboard`.

## 8. Server to client messages

### 8.1 Snapshot (0x0083)

Positions of the replicated entities at a given server tick.

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `serverTick` | `u32` | Server tick of the states. |
| 4 | 2 | `entityCount` | `u16` | Number of entity states that follow. |
| 6 | 4 | `ackedInputTick` | `u32` | Client tick that the ship of the receiver accounts for, 0 if the server has no `Input` of this client yet. |
| 10 | 4 | `ownNetworkId` | `u32` | Network id of the ship of the receiver, 0 if it has none (spectator, dead). |
| 14 | 11 x `entityCount` | states | | Entity states, see below. |

`ackedInputTick` and `ownNetworkId` are different for every receiver. They let a
client predict and correct the movement of its own ship, see section 10.1.

Each entity state:

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `networkId` | `u32` | Entity id, never 0. |
| 4 | 1 | `entityType` | `u8` | Low 8 bits of the archetype id, informative only. |
| 5 | 2 | `positionX` | `pos16` | Horizontal position. |
| 7 | 2 | `positionY` | `pos16` | Vertical position. |
| 9 | 2 | `rotation` | `rot16` | Rotation. |

One `Snapshot` holds at most 46 entity states. When more entities exist, the
server sends several `Snapshot` messages with the same `serverTick`, each in
its own packet.

The client MUST drop the message if:

- `entityCount` is greater than 46,
- the payload size is not exactly `14 + 11 x entityCount`,
- any `networkId` is 0,
- the same `networkId` appears twice.

The client sets the position of every known entity to the received values,
except its own ship, which is predicted (section 10.1).
Snapshots are not deltas: each one carries the full position of the entities it
lists. The server normally sends one every 3 ticks, which is about 20 per second.

A `Snapshot` does not list every entity. The stage scenery (the blueprints
`low_ground`, `medium_ground`, `high_ground`, `ramp`, `reactor` and `gate`) scrolls
at a constant velocity given by its blueprint, so the server lists a piece of
scenery in the first `Snapshot` sent to a client after the entity was created,
and then only every 1.5 to 2.5 seconds, to correct any drift. A client MUST
keep moving an entity that a `Snapshot` does not list with its last known
velocity, and MUST NOT remove it: an entity is removed only by `DestroyEntity`.

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

### 8.5 ChatMessage (0x0096)

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `roomId` | `u32` | Room the message was sent in, never 0. |
| 4 | 4 | `senderId` | `u32` | Connection id of the sender, or 0 for a server notice. |
| 8 | 1 | `length` | `u8` | Text length `n`, 1 to 64. |
| 9 | n | `text` | bytes | Message, with the characters of `ChatSend`. |

Sent reliably, in the lobby of the room as well as during a game. A client
MUST drop the message if it is invalid or if `roomId` is not its current room.

A `senderId` of 0 marks a notice written by the server, not by a player. The
server sends one to the other players of a room when a player leaves it, with
the text `<name> LEFT THE ROOM`, or when a player loses the connection (crash,
timeout or `Disconnect` without `LeaveRoom`), with the text
`<name> LOST CONNECTION`. `<name>` is the username of the player, or `P`
followed by its connection id when it has none. A client SHOULD display these
notices apart from player messages.

### 8.6 Scoreboard (0x0097)

The payload is plain text, without terminator: one line per player, ended by
`\n`, for the 10 best players sorted by best score, for example
`1. bob  BEST 300  TOTAL 300  GAMES 1`. It is `NO SCORE YET` when no score was
saved. Sent reliably, as the answer to `GetScoreboard`. The client displays it
as it is.

### 8.7 ScoreUpdate (0x0098)

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 4 | `score` | `u32` | Points of the receiving player in the current round. |

Sent reliably to a player each time its score changes. The client resets the
score to 0 when it receives `GameStarting`.

### 8.8 PowerUpTaken (0x0099)

| Offset | Size | Field | Type | Description |
| -----: | ---: | ----- | ---- | ----------- |
| 0 | 1 | `kind` | `u8` | `0` heal, `1` speed, `2` shield, `3` spread weapon, `4` plasma weapon. |
| 1 | 2 | `value` | `u16` | Strength of the effect, in tenths: pixels per second for speed, seconds for a weapon, health points for heal and shield. |

Sent reliably to the player whose ship took a power up, and only to it. The
server decides the pickup and applies the effect on its side; the client uses
this message to update its own ship (speed used by the prediction, weapon shown
by the HUD). The power up itself is removed with a `DestroyEntity` of reason
`Collision`, sent to every player. A client MUST drop the message if `kind` is
unknown or if bytes follow the payload.

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
| `enemy_bullet` | `0x7FF2` | Bydos missile |

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
  lp [label="Loop:\nClient: Input when the keys change, and every 100 ms\nServer: Snapshot every 3 ticks (less often on a saturated link),\nCreateEntity and DestroyEntity on events"];
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
`Input` when its keys change and every 100 ms, and the server sends a `Snapshot`
every 3 ticks, plus
`CreateEntity` and `DestroyEntity` when entities appear or disappear. When a
snapshot lists an unknown entity, the client sends `QueryEntity` and the server
answers with `CreateEntity`. The session ends when the client sends
`Disconnect`, and the server then tells the remaining players that the ship is
gone.

Game rules a client should know:

| Rule | Value |
| ---- | ----- |
| Server tick rate | About 60 per second |
| Snapshot rate | One every 3 ticks, down to one every 12 ticks for a client whose link is saturated (section 10.2) |
| Round start | 2 players connected, or 30 seconds after the first player joined |
| Player slots | 4, one ship color per slot |
| Spawn position of slot n (0 to 3) | x = 150, y = 150 + 100 n |
| Return to lobby | When every player has left, or when every ship is destroyed (game over) |
| Game over | Every entity is removed with `DestroyEntity` reason 4, every player becomes not ready, then the client goes back to the room lobby |

### 10.1 Prediction of the local ship

Without prediction, the ship of a player moves only when a `Snapshot` comes
back, one round trip time after the key was pressed. A client SHOULD instead
move its own ship at once and use the `Snapshot` to correct it. The server
gives it what it needs, in every `Snapshot`:

- `ownNetworkId` tells which entity is the ship of the receiver,
- `ackedInputTick` tells which client tick the position of that ship in the
  `Snapshot` accounts for.

The server computes `ackedInputTick` from the current run of actions of the
client: if the run started with the `Input` of client tick `c`, applied at
server tick `s`, then a `Snapshot` sent at server tick `n` has
`ackedInputTick = c + (n - s)`. The ship has been moving with these actions
since then, one client tick per server tick, which assumes that both sides tick
60 times per second. The value is 0 until the first `Input`.

A client that predicts:

1. Every frame, moves its ship by the velocity of the held actions (section
   7.1) times the duration of the frame, and records the movement with the
   client tick of the frame.
2. On every `Snapshot` that contains its ship, forgets the recorded movements
   whose tick is not after `ackedInputTick`, and adds the remaining ones to the
   position of the `Snapshot`. The result is the position the ship should have
   now. If `ackedInputTick` is 0, no movement is forgotten.
3. Moves the displayed ship toward that position. Small errors SHOULD be
   corrected smoothly (the reference client removes half of the error at each
   `Snapshot` and ignores errors under 0.75 pixel), large ones, above 96
   pixels, at once.
4. Ignores a `Snapshot` whose `ackedInputTick` is older than the last one used,
   which happens when packets are reordered.

The server stays the authority: a hit, a power up or a lost packet is corrected
by step 2 and 3. Other entities are not predicted and are set to the position
of the `Snapshot`.

### 10.2 Snapshot rate

A saturated link makes packets wait in a queue, and everything then arrives
late. The server MAY send fewer `Snapshot` to a client whose link is saturated,
and MUST send them again at the normal rate once the link is free. A client
MUST NOT assume a fixed rate: it uses `serverTick` to order the `Snapshot` and
to know their age.

The reference server watches the smallest round trip time of each client over
half a second, measured on every acknowledged packet, four times per second. If
it stays more than 80 ms above the smallest value seen for this client on two
checks in a row, a queue is filling: the server sends one `Snapshot` out of 2, 3
or 4 (down to 5 per second), going straight to the slowest rate if the growth is
above 300 ms. It goes back one
step after 6 seconds without growth. Only `Snapshot` are slowed down:
`CreateEntity`, `DestroyEntity` and every reliable message keep their
priority.

## 11. Examples

All bytes are written in hexadecimal.

### 11.1 Hello

```text
52 54 59 50  03  01
magic        ver type=Hello
```

### 11.2 Welcome with connection id 1

```text
52 54 59 50  03  02         00 00 00 01
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
02            priority     = unreliable
00 00         reliableId   = 0
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

### 11.5 Packet with a compressed body

Connection 1, packet sequence 9, critical packet with `reliableId` 3. The body
holds three `CreateEntity` messages of 20 bytes each (60 bytes), and compresses
to 47 bytes with the protocol dictionary.

```text
00 00 00 01   connectionId = 1
00 00 00 09   sequence     = 9
00 00 00 05   ack          = 5
00 00 00 01   ackBits      = packet 4 received
80            priority     = critical (0) | compressed (0x80)
00 03         reliableId   = 3
00 00 55 01 00 C3 01 06 FF 5F 23 AB 55 98 2C BC
FD D6 2A CC C2 6D DD C2 5A 85 69 2E FE 07 B1 04
04 FC 00 9F 63 ED C9 6E C5 F5 A7 8D E1 C2 10
```

Total size: 66 bytes instead of 79 without compression.
