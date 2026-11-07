# Network benchmark {#network_benchmark}

[TOC]

This page answers one question: **is the game still playable when the network
is bad, and what did we do about it?** The subject asks for real measurements of
bandwidth and of responsiveness in front of lag, and suggests testing a 2 percent
packet drop or duplication, a latency above 150 ms and a very low bandwidth such
as 5 KB/s. All of these are measured here, with `netem`, the tool the subject
recommends for Linux.

The page compares two versions of the game: the one before we worked on latency and the current one. The mechanisms that
make the difference are described in @ref networking_latency, the protocol
changes in @ref protocol (sections 7.1, 8.1, 10.1 and 10.2) and the reasons for
our choices in @ref technical_study (sections 6.5 and 6.6).

## Short answer

Before the changes, the ship of the player reacted one round trip time after a
key press (135 ms with a 100 ms RTT, 535 ms with 500 ms), a 10 KB/s link added
775 ms of delay, and a 5 KB/s link made the game unplayable.

Now the ship moves in the same frame at any latency, the game stays connected
with up to 50 percent packet loss, and links down to 10 KB/s per player work. The
remaining problems are the other players and the enemies, which are seen late
(they are not interpolated), a 5 KB/s link that is still on the edge, and a
bandwidth that is still too high for four players on a link under 20 KB/s. They
are listed in the section *Limits and known problems*.

How each condition is rated, after the changes:

<!-- BEGIN summary -->
| Condition | Verdict | Ship seen after (ms) | Update wait p99 (ms) | Updates received | Prediction gap p95 (px) | Disconnections |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| no degradation | good | 0 | 51 | 100 % | 0.0 | 0 |
| RTT 50 ms | good | 0 | 51 | 100 % | 2.8 | 0 |
| RTT 100 ms | good | 0 | 51 | 100 % | 2.8 | 0 |
| RTT 150 ms | playable | 0 | 51 | 100 % | 2.8 | 0 |
| RTT 200 ms | playable | 0 | 51 | 100 % | 3.2 | 0 |
| RTT 300 ms | degraded | 0 | 51 | 100 % | 2.8 | 0 |
| RTT 500 ms | degraded | 0 | 51 | 100 % | 2.8 | 0 |
| 2 % loss | good | 0 | 69 | 99 % | 2.2 | 0 |
| 5 % loss | good | 0 | 81 | 99 % | 0.6 | 0 |
| 10 % loss | good | 0 | 100 | 97 % | 1.1 | 0 |
| 20 % loss | playable | 0 | 119 | 93 % | 2.8 | 0 |
| 30 % loss | playable | 0 | 150 | 87 % | 0.9 | 0 |
| 50 % loss | degraded | 0 | 231 | 69 % | 10.0 | 0 |
| 5 % loss, RTT 100 ms | good | 0 | 82 | 99 % | 4.3 | 0 |
| 10 % loss, RTT 150 ms | playable | 0 | 100 | 97 % | 4.3 | 0 |
| 2 % duplicated | good | 0 | 51 | 100 % | 2.2 | 0 |
| 10 % duplicated | good | 0 | 51 | 100 % | 0.7 | 0 |
| 25 % reordered, RTT 50 ms | good | 0 | 75 | 100 % | 3.7 | 0 |
| 1 % corrupted | good | 0 | 51 | 100 % | 2.2 | 0 |
| RTT 50 ms, jitter 40 ms | good | 0 | 95 | 100 % | 5.0 | 0 |
| 4G profile | good | 0 | 66 | 99 % | 3.6 | 0 |
| bad Wi-Fi profile | good | 0 | 86 | 99 % | 4.4 | 0 |
| congested mobile profile | playable | 0 | 107 | 93 % | 4.1 | 0 |
| 40 KB/s link, 1 player | good | 0 | 53 | 100 % | 3.5 | 0 |
| 20 KB/s link, 1 player | good | 0 | 55 | 100 % | 3.5 | 0 |
| 10 KB/s link, 1 player | playable | 0 | 151 | 91 % | 4.9 | 0 |
| 5 KB/s link, 1 player | unstable | 0 | 273 | 32 % | 56.2 | 1 |
| 50 KB/s shared, 4 players | good | 0 | 76 | 99 % | 3.6 | 0 |
| 20 KB/s shared, 4 players | degraded | 0 | 361 | 27 % | 17.0 | 0 |
| 10 KB/s shared, 4 players | unstable | 0 | 485 | 40 % | 6.2 | 3 |
| 5 KB/s shared, 4 players | unplayable | - | - | - | - | 0 |
<!-- END summary -->

The ratings follow fixed rules, applied to the measurements of the current
version:

| Rating | Rule |
| ------ | ---- |
| good | Round trip time up to 100 ms, 99 percent of the world updates arrive within 100 ms of the previous one, and at least 95 percent of them arrive. |
| playable | Round trip time up to 250 ms, wait up to 200 ms, at least 85 percent of the updates. |
| degraded | Anything worse, but the game stays connected. |
| unstable | At least one client was disconnected by the server. |
| unplayable | The game could not even start. |

## How it was measured

### Degrading the network

Every scenario runs the real server and the bots in a private network namespace
(`unshare -rn`, which needs no root and leaves the real network alone). The
loopback of that namespace is degraded with the Linux traffic control tool,
`tc qdisc add dev lo root netem ...`. A packet crosses it once in each direction,
so:

- `delay 50ms` is a round trip time of 100 ms, and every RTT in this page is twice
  the delay,
- `loss 5%` loses 5 percent of the packets in each direction,
- `rate` limits all the traffic of the namespace, so with four players the rate
  is shared by the whole room. The tests with one player give the capacity of
  the link of one player.

The scenarios are the ones of the subject (2 percent drop or duplication,
latency above 150 ms, 5 KB/s) and a wider sweep: RTT from 20 to 500 ms, loss from
1 to 50 percent, 10 percent duplication, reordering, corruption, jitter, links of
5 to 40 KB/s, and three profiles that imitate a 4G connection, a bad Wi-Fi and a
congested mobile link.

### The players

The measurements were made with automated players, bots, that use the real
network code of the game, and the game was also played with the graphical client
under the same conditions. A room of four bots joins one game and plays for 25
seconds, sending inputs 60 times per second like the real client. The game ends
earlier when all the ships are destroyed, which happens after about 15 seconds
because the bots move at random. Each scenario was run twice, 88 runs in all.

The first bot is the probe. It presses Right, then nothing, then Left, then
nothing, for one second each, and measures the **delay before the ship moves**:
the time between the key press and the moment the ship has moved by 2 pixels.
There are two versions of this delay:

- **confirmed**: a snapshot of the server shows that the ship moved. This is what
  the player saw before the changes, and it is still the delay of the true state
  of the game.
- **seen**: the ship drawn by the prediction moved. This is what the player sees
  now.

The probe can also fire: it holds the fire button for 0.4 second, releases it,
and measures the time until the missile appears next to the ship (**missile
visible**).

The other measures are the **update wait** (time between two world updates, which
should be 50 ms), the share of **updates received**, the time taken to **repair a
lost creation** (an entity shown in a snapshot before its `CreateEntity`
arrived), the **prediction gap** (distance in pixels between the predicted ship
and the position it should have according to the server, measured at every
snapshot), the number of **disconnections**, and the bytes and packets sent and
received by the clients.

### The statistics in the client: debug mode

The graphical client computes the same statistics and shows them live. Pressing F3
turns the debug mode on: a panel lists the metrics of the network and the
collision boxes are drawn. The complete list, with the percentiles of the last ten
seconds, is written on the console when F3 is pressed and every five seconds
after.

| Metric of this page | Line of the debug screen |
| ------------------- | ------------------------ |
| Round trip time | RTT: last, smoothed, jitter |
| Packet loss, resends, duplicates, reordering | LOSS |
| Bandwidth, packets per second | DOWN and UP |
| Update wait, updates received | WORLD and UPDATES |
| Delay before the server confirms an input | INPUT |
| Ship seen at once (prediction) | SHIP |
| Prediction gap | PREDICT |
| Repair of a lost creation | CREATIONS |
| Rating of the table below | first line: GOOD, PLAYABLE or DEGRADED |

Each line ends with `[OK]`, `[WARN]` or `[BAD]`, so the state is not given by the
color alone. The rating on the first line uses the same thresholds as the rating
of this page.

### Reading the numbers

Every value of the tables is an average. A scenario has two runs, and for each
run the value is measured by the clients: the delays and the waits are
percentiles (the median, the 95th or the 99th) computed on all the samples of
the run, and the table gives the mean of the two runs. The longest wait is the
largest one of all the runs. The bandwidth is the mean over the time played.

A scenario has about ten key presses, so a percentile is fragile: one lost
snapshot moves a delay by 50 ms. Differences smaller than that are noise. Each
figure is followed by a text that gives the same information.

## Results

### Latency

![Line chart. The horizontal axis is the round trip time added by the network, from 0 to 500 ms. The vertical axis is the delay before the ship moves. The delay before the changes and the delay confirmed by the server both follow the round trip time, 535 ms at 500 ms. The delay of the ship seen by the player is 0 ms at every round trip time.](netbench/latency_vs_rtt.svg)

**Text description of the figure.** Before the changes, the delay between a key
press and the movement of the ship is the round trip time plus about 35 ms: 85 ms
at 50 ms RTT, 135 ms at 100 ms, 184 ms at 150 ms, 234 ms at 200 ms, 335 ms at 300
ms and 535 ms at 500 ms. The server still confirms the movement after the same
delay. The ship that the player sees now moves in the same frame at every round
trip time.

<!-- BEGIN latency -->
| Added latency | Before (ms) | After, confirmed (ms) | After, seen (ms) | After, seen p95 (ms) | Samples |
| --- | ---: | ---: | ---: | ---: | ---: |
| none | 35 | 32 | 0 | 0 | 12 |
| RTT 20 ms | 35 | 35 | 0 | 0 | 9 |
| RTT 50 ms | 85 | 84 | 0 | 0 | 16 |
| RTT 100 ms | 135 | 133 | 0 | 0 | 10 |
| RTT 150 ms | 184 | 184 | 0 | 0 | 14 |
| RTT 200 ms | 234 | 236 | 0 | 0 | 13 |
| RTT 300 ms | 335 | 335 | 0 | 0 | 13 |
| RTT 500 ms | 535 | 534 | 0 | 0 | 10 |
<!-- END latency -->

The "seen" delay of 0 ms means that the predicted ship has moved in the same loop
iteration as the key press. On screen the movement appears at the next
rendered frame, at most 16 ms later at 60 FPS. The true state of the game is
still confirmed after the round trip time: what changes is that the player does
not wait for it.

### Packet loss

![Line chart. The horizontal axis is the percentage of packets lost in each direction, from 0 to 50. The vertical axis is the wait between two world updates in milliseconds. The 99th percentile grows from 51 ms without loss to 231 ms at 50 percent loss. The longest wait grows from 51 ms to 450 ms. The curves before and after the changes are close.](netbench/snapshot_gap_vs_loss.svg)

**Text description of the figure.** A world update should arrive every 50 ms. The
99th percentile of the wait stays at 100 ms with 10 percent loss, which is one
lost update, then grows to 150 ms with 30 percent and 231 ms with 50 percent. The
longest wait is 450 ms with 50 percent loss. The curves before and after the
changes are close because the way snapshots are sent did not change.

<!-- BEGIN loss -->
| Condition | Wait p99 before (ms) | Wait p99 after (ms) | Longest before (ms) | Longest after (ms) | Updates received | Creation repaired p95 (ms) | Creation repaired max (ms) | Server confirmation before (ms) | Server confirmation after (ms) | Disconnections |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| none | 51 | 51 | 51 | 51 | 100 % | - | 0 | 35 | 32 | 0 |
| 1 % loss | 51 | 51 | 101 | 101 | 100 % | 52 | 86 | 35 | 34 | 0 |
| 2 % loss | 57 | 69 | 150 | 101 | 99 % | 17 | 17 | 35 | 35 | 0 |
| 5 % loss | 88 | 81 | 150 | 150 | 99 % | 38 | 101 | 34 | 60 | 0 |
| 10 % loss | 100 | 100 | 200 | 151 | 97 % | 47 | 118 | 33 | 33 | 0 |
| 20 % loss | 131 | 119 | 250 | 199 | 93 % | 124 | 273 | 59 | 35 | 0 |
| 30 % loss | 175 | 150 | 300 | 251 | 87 % | 127 | 302 | 58 | 104 | 0 |
| 50 % loss | 269 | 231 | 450 | 450 | 69 % | 205 | 472 | 81 | 59 | 0 |
| 5 % loss, RTT 100 ms | 94 | 82 | 150 | 149 | 99 % | 131 | 194 | 160 | 133 | 0 |
| 10 % loss, RTT 150 ms | 100 | 100 | 151 | 200 | 97 % | 131 | 167 | 183 | 185 | 0 |
<!-- END loss -->

No client was disconnected, even with 50 percent of the packets lost in each
direction, which means that only one packet out of four makes a round trip. Lost
creations are repaired in 17 to 205 ms (95th percentile) and at most 472 ms, by the
resends of the reliable packets and by `QueryEntity`. With up to 10 percent loss
the game is rated good, and with 20 and 30 percent playable.

The "server confirmation" columns are noisy: with few samples, one lost snapshot
or input moves the median by 50 ms, in both directions (5 percent loss: 34 then 60
ms, 20 percent loss: 59 then 35 ms). We found no systematic change. In any case
the player does not wait for it any more.

### Duplication, reordering, corruption, jitter

<!-- BEGIN conditions -->
| Condition | Delay before (ms) | Delay after, confirmed (ms) | Delay after, seen (ms) | Update wait p99 after (ms) | Prediction gap p95 (px) | Disconnections |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 2 % duplicated | 34 | 34 | 0 | 51 | 2.2 | 0 |
| 10 % duplicated | 35 | 34 | 0 | 51 | 0.7 | 0 |
| 25 % reordered, RTT 50 ms | 85 | 85 | 0 | 75 | 3.7 | 0 |
| 1 % corrupted | 34 | 35 | 0 | 51 | 2.2 | 0 |
| RTT 50 ms, jitter 40 ms | 75 | 76 | 0 | 95 | 5.0 | 0 |
| 4G: RTT 60 ms, jitter, 1 % loss | 78 | 84 | 0 | 66 | 3.6 | 0 |
| bad Wi-Fi: 5 % loss, jitter | 88 | 77 | 0 | 86 | 4.4 | 0 |
| mobile: RTT 120 ms, 3 % loss, 400 kbit/s | 501 | 227 | 0 | 107 | 4.1 | 0 |
<!-- END conditions -->

Duplicated packets, reordered packets and 1 percent of corrupted packets have no
visible effect. The tracker drops the duplicates, the corrupted datagrams fail a
length check and are dropped like lost ones, and a reordered snapshot is
ignored by the prediction when it is older than the last one used. The three
profiles are playable: the congested mobile link (RTT 120 ms, 3 percent loss,
400 kbit/s shared by four players) went from 501 ms to 227 ms of confirmation
delay, thanks to the snapshot rate adaptation.

### Accuracy of the prediction

![Line chart. The horizontal axis lists ten network conditions. The vertical axis is the distance in pixels between the predicted ship and the position given by the server. The median is below 1 pixel everywhere. The 95th percentile stays under 11 pixels and the largest gap reaches 48 pixels with 50 percent loss.](netbench/prediction_error.svg)

**Text description of the figure.** Under ten conditions, from no degradation to a
congested mobile link, the median distance between the predicted ship and the
position it should have according to the server is below one pixel. The 95th
percentile is at most 10 pixels, with 50 percent loss. The largest distance is 48
pixels, with 50 percent loss. The screen is 1280 pixels wide.

Over the 70 runs, the prediction was compared with the server 15 213 times. It
corrected the ship smoothly 2 388 times (16 percent) and had to move it at once
twice, both on a 5 KB/s link (the largest gap was 140 pixels). A player sees
these corrections as a ship that slides slightly, not as a jump, except in the
two extreme cases.

### Bandwidth

<!-- BEGIN bandwidth -->
| Case | Down before (KB/s) | Down after (KB/s) | Up before (KB/s) | Up after (KB/s) | Up packets/s before | Up packets/s after | Down packets/s after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 players, no limit | 7.7 | 7.5 | 1.90 | 0.66 | 61 | 21 | 41 |
| 1 player, no limit | 5.9 | 6.0 | 1.90 | 0.60 | 61 | 20 | 27 |
<!-- END bandwidth -->

The figures count the bytes of the game protocol. Each datagram adds 28 bytes of
IPv4 and UDP headers on the wire. Per client, with four players, that makes about
9.9 KB/s on the wire in total now (8.6 down, 1.2 up) against 12.4 KB/s before
(8.9 down, 3.6 up), 20 percent less.

The saving is on the way up: the client sends 21 packets per second instead of
61, because an input is now sent only when the keys change and every 50 ms
otherwise. The way down is unchanged, 7.5 KB/s and 41 packets per second per client, of
which a snapshot is most of the volume. Reducing it needs delta snapshots, which
we have not done (see the limits).

### Narrow links

![Line chart. The horizontal axis is the capacity of the link in KB/s, 5, 10, 20 and 40, on a logarithmic scale. The vertical axis is the delay before the ship moves. Before the changes the delay is 775 ms at 10 KB/s and no movement was seen at 5 KB/s. After, the delay confirmed by the server is 64 ms at 10 KB/s and 149 ms at 5 KB/s, and the ship seen by the player moves at once on every link.](netbench/narrow_links.svg)

**Text description of the figure.** With one player, a link of 20 or 40 KB/s works
the same before and after, 47 and 41 ms. At 10 KB/s the delay before the ship moved
was 775 ms and is now 64 ms. At 5 KB/s the ship never moved in the before
runs, and the server confirms the movement after 149 ms now. The ship seen by the
player moves at once on every link.

<!-- BEGIN links -->
| Link (one player) | Delay before (ms) | Delay after, confirmed (ms) | Delay after, seen (ms) | Updates before | Updates after | Update every, after (ms) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| no limit | 34 | 34 | 0 | 100 % | 100 % | 50 |
| 40 KB/s | 41 | 41 | 0 | 100 % | 100 % | 50 |
| 20 KB/s | 47 | 47 | 0 | 100 % | 100 % | 50 |
| 10 KB/s | 775 | 64 | 0 | 89 % | 91 % | 50 |
| 5 KB/s | no sample | 149 | 0 | 39 % | 32 % | 177 |
<!-- END links -->

With four players, the link is shared by the room:

<!-- BEGIN shared_links -->
| Link shared by the room | Delay before (ms) | Delay after, confirmed (ms) | Delay after, seen (ms) | Updates before | Updates after | Disconnections after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 20 KB/s for 4 players | 663 | 296 | 0 | 45 % | 27 % | 0 |
| 10 KB/s for 4 players | 749 | no sample | 0 | 63 % | 40 % | 3 |
| 5 KB/s for 4 players | no sample | no sample | no sample | 53 % | no game | 0 |
| 50 KB/s for 4 players | 481 | 59 | 0 | 86 % | 99 % | 0 |
<!-- END shared_links -->

The server watches the round trip time of every client. When a queue fills in
front of a link, it sends fewer snapshots to that client (down to 5 per second),
so the queue empties and the delay comes back. That is what makes 10 KB/s per
player work, and the congested mobile profile.

The 5 KB/s link is still on the edge. The ship seen by the player moves at once and
the server confirms after 149 ms, but world updates arrive every 177 ms and one of
the two runs ended with a disconnection. The cause is the burst of `CreateEntity`
sent when the game starts: it fills the queue of a 5 KB/s link before the
adaptation can react, and reliable packets are dropped. Four players on 5 or
10 KB/s in total are below what the game needs: before and after, the clients are
disconnected or the game does not start.

## What was changed

Three changes came out of the first round of measurements:

1. **Prediction with reconciliation of the local ship.** The ship moves at once,
   with the movement of the server, and each snapshot corrects it. This removes the
   round trip time from what the player feels (@ref networking_latency).
2. **Inputs sent when they change.** 21 packets per second instead of 61, with
   two repeats and a 50 ms heartbeat against packet loss.
3. **Snapshot rate adapted to the link.** A client whose round trip time grows by
   more than 60 ms is sent fewer snapshots. The round trip time is now measured on
   every acknowledged packet, because the one used for resends was too rare to see
   a queue (28 ms measured while the real delay was 775 ms).

The protocol version went from 2 to 3 because the `Snapshot` header and the
meaning of `Input` changed.

## Limits and known problems

- **The players are bots.** They move at random, run at a fixed 60 FPS and die
  in about 15 seconds. A human player holds keys longer, and a real client has
  frames of different durations, which makes the prediction gap larger than
  measured here, where it is exactly 0 with no network delay. The graphical
  client uses the same prediction code and compiles, but we did not play it
  under degraded conditions: it needs a screen.
- **Few samples.** About ten key presses per scenario, two runs. The 95th
  percentiles and the comparisons below 50 ms are not reliable.
- **Other players and enemies are seen late and in steps.** Only the ship of the
  local player is predicted. Everything else is the position of the last snapshot,
  without interpolation, so it moves 20 times per second and freezes for the length
  of a lost snapshot (150 ms with 30 percent loss). The next step is to
  interpolate the other entities, at the cost of showing them about 100 ms in the
  past.
- **Missiles leave from the position of the server ship**, which is behind the ship
  that the player sees by about half a round trip time. At 300 pixels per second
  and 100 ms of RTT that is 15 pixels. The server does not compensate the lag.
- **The way down is still 7.5 KB/s per client.** Every snapshot carries the full
  position of every entity. Only the entities that changed, or only the ones that
  cannot be guessed (the enemies move in straight lines), would be needed.
- **The burst of `CreateEntity` at the start** is not paced, so it can overflow a
  very narrow link (see above).
- **The speed of a ship is read from its blueprint.** A speed power up is applied
  by the client and the server with the same rule, but if they ever disagree
  the prediction is corrected at each snapshot, and the ship slides.
