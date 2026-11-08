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

Now the ship moves in the same frame at any latency, and not one of the 88 runs
ended with a disconnection, even with 50 percent of the packets lost, or with
four players sharing 5 KB/s. A player alone on a 5 KB/s link, the test of the
subject, can move and shoot: the missile appears 88 ms after the key is
released, against 810 ms before our last fixes, and the game is rated
playable. The remaining problems are the other players and the enemies, which
are seen late and in steps (they are not interpolated), and four players on a
link under 20 KB/s, which is below what the game needs. They are listed in the
section *Limits and known problems*.

How each condition is rated, after the changes:

<!-- BEGIN summary -->
| Condition | Verdict | Ship seen after (ms) | Update wait p99 (ms) | Updates received | Prediction gap p95 (px) | Disconnections |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| no degradation | good | 0 | 51 | 100 % | 4.4 | 0 |
| RTT 50 ms | good | 0 | 51 | 100 % | 2.8 | 0 |
| RTT 100 ms | good | 0 | 51 | 100 % | 3.5 | 0 |
| RTT 150 ms | playable | 0 | 51 | 100 % | 3.5 | 0 |
| RTT 200 ms | playable | 0 | 51 | 100 % | 3.5 | 0 |
| RTT 300 ms | degraded | 0 | 51 | 100 % | 2.8 | 0 |
| RTT 500 ms | degraded | 0 | 51 | 100 % | 3.2 | 0 |
| 2 % loss | good | 0 | 100 | 98 % | 2.6 | 0 |
| 5 % loss | playable | 0 | 100 | 95 % | 4.5 | 0 |
| 10 % loss | playable | 0 | 113 | 91 % | 2.6 | 0 |
| 20 % loss | degraded | 0 | 169 | 79 % | 0.6 | 0 |
| 30 % loss | degraded | 0 | 200 | 69 % | 5.0 | 0 |
| 50 % loss | degraded | 0 | 356 | 49 % | 7.0 | 0 |
| 5 % loss, RTT 100 ms | playable | 0 | 101 | 96 % | 4.3 | 0 |
| 10 % loss, RTT 150 ms | playable | 0 | 107 | 90 % | 4.3 | 0 |
| 2 % duplicated | good | 0 | 51 | 100 % | 4.4 | 0 |
| 10 % duplicated | good | 0 | 51 | 100 % | 4.5 | 0 |
| 25 % reordered, RTT 50 ms | good | 0 | 76 | 100 % | 4.3 | 0 |
| 1 % corrupted | good | 0 | 75 | 99 % | 4.5 | 0 |
| RTT 50 ms, jitter 40 ms | playable | 0 | 108 | 100 % | 5.4 | 0 |
| 4G profile | good | 0 | 79 | 99 % | 3.5 | 0 |
| bad Wi-Fi profile | playable | 0 | 111 | 95 % | 4.4 | 0 |
| congested mobile profile | playable | 0 | 102 | 97 % | 4.4 | 0 |
| 40 KB/s link, 1 player | good | 0 | 51 | 100 % | 2.8 | 0 |
| 20 KB/s link, 1 player | good | 0 | 53 | 100 % | 3.5 | 0 |
| 10 KB/s link, 1 player | good | 0 | 60 | 100 % | 3.5 | 0 |
| 5 KB/s link, 1 player | playable | 0 | 120 | 95 % | 4.7 | 0 |
| 10 KB/s shared, 2 players | degraded | 0 | 183 | 82 % | 3.5 | 0 |
| 5 KB/s shared, 2 players | degraded | 0 | 518 | 31 % | 40.5 | 0 |
| 50 KB/s shared, 4 players | good | 0 | 56 | 100 % | 2.8 | 0 |
| 20 KB/s shared, 4 players | degraded | 0 | 180 | 66 % | 5.1 | 0 |
| 10 KB/s shared, 4 players | degraded | 0 | 515 | 30 % | 11.9 | 0 |
| 5 KB/s shared, 4 players | degraded | - | 1796 | 17 % | 28.8 | 0 |
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
congested mobile link, and rooms of one, two and four players.

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
| none | 35 | 34 | 0 | 0 | 13 |
| RTT 20 ms | 35 | 34 | 0 | 0 | 12 |
| RTT 50 ms | 85 | 83 | 0 | 0 | 12 |
| RTT 100 ms | 135 | 134 | 0 | 0 | 12 |
| RTT 150 ms | 184 | 185 | 0 | 0 | 15 |
| RTT 200 ms | 234 | 234 | 0 | 0 | 8 |
| RTT 300 ms | 335 | 334 | 0 | 0 | 10 |
| RTT 500 ms | 535 | 535 | 0 | 0 | 10 |
<!-- END latency -->

The "seen" delay of 0 ms means that the predicted ship has moved in the same loop
iteration as the key press. On screen the movement appears at the next
rendered frame, at most 16 ms later at 60 FPS. The true state of the game is
still confirmed after the round trip time: what changes is that the player does
not wait for it.

### Packet loss

![Line chart. The horizontal axis is the percentage of packets lost in each direction, from 0 to 50. The vertical axis is the wait between two world updates in milliseconds. After the changes the 99th percentile grows from 51 ms without loss to 113 ms at 10 percent loss, 200 ms at 30 percent and 356 ms at 50 percent. The longest wait reaches 700 ms at 50 percent loss. The curves before and after the changes are close.](netbench/snapshot_gap_vs_loss.svg)

**Text description of the figure.** A world update should arrive every 50 ms. The
99th percentile of the wait is 100 ms with 2 percent loss, which is one lost
update, 113 ms with 10 percent, 200 ms with 30 percent and 356 ms with 50 percent.
The longest wait is 700 ms with 50 percent loss. The curves before and after the
changes are close, a little higher after at 30 and 50 percent loss (see below).

<!-- BEGIN loss -->
| Condition | Wait p99 before (ms) | Wait p99 after (ms) | Longest before (ms) | Longest after (ms) | Updates received | Creation repaired p95 (ms) | Creation repaired max (ms) | Server confirmation before (ms) | Server confirmation after (ms) | Disconnections |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| none | 51 | 51 | 51 | 51 | 100 % | - | 0 | 35 | 34 | 0 |
| 1 % loss | 51 | 81 | 101 | 150 | 99 % | 16 | 16 | 35 | 34 | 0 |
| 2 % loss | 57 | 100 | 150 | 151 | 98 % | 17 | 17 | 35 | 34 | 0 |
| 5 % loss | 88 | 100 | 150 | 151 | 95 % | 32 | 69 | 34 | 34 | 0 |
| 10 % loss | 100 | 113 | 200 | 201 | 91 % | 64 | 204 | 33 | 41 | 0 |
| 20 % loss | 131 | 169 | 250 | 349 | 79 % | 112 | 185 | 59 | 22 | 0 |
| 30 % loss | 175 | 200 | 300 | 300 | 69 % | 158 | 429 | 58 | 80 | 0 |
| 50 % loss | 269 | 356 | 450 | 700 | 49 % | 326 | 716 | 81 | 96 | 0 |
| 5 % loss, RTT 100 ms | 94 | 101 | 150 | 151 | 96 % | 144 | 199 | 160 | 135 | 0 |
| 10 % loss, RTT 150 ms | 100 | 107 | 151 | 199 | 90 % | 304 | 556 | 183 | 169 | 0 |
<!-- END loss -->

No client was disconnected, even with 50 percent of the packets lost in each
direction, which means that only one packet out of four makes a round trip. Lost
creations are repaired in 16 to 326 ms (95th percentile) and at most 716 ms, by the
resends of the reliable packets and by `QueryEntity`. The game is rated good up
to 2 percent loss, playable up to 10 percent, and degraded from 20 percent, with
49 percent of the world updates still received at 50 percent loss.

At 30 and 50 percent loss the wait is a little longer after the changes (200 ms
against 175 ms, 356 ms against 269 ms). The server reads the missing
acknowledgements as the sign of a queue and slows the snapshots down, which does
not help when the packets are lost by the network and not queued. The player does
not feel it on his own ship, which is predicted, and the game stays connected.

The "server confirmation" columns are noisy: with few samples, one lost snapshot
or input moves the median by 50 ms, in both directions (20 percent loss: 59 then 22
ms, 30 percent loss: 58 then 80 ms). We found no systematic change. In any case
the player does not wait for it any more.

### Duplication, reordering, corruption, jitter

<!-- BEGIN conditions -->
| Condition | Delay before (ms) | Delay after, confirmed (ms) | Delay after, seen (ms) | Update wait p99 after (ms) | Prediction gap p95 (px) | Disconnections |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 2 % duplicated | 34 | 34 | 0 | 51 | 4.4 | 0 |
| 10 % duplicated | 35 | 34 | 0 | 51 | 4.5 | 0 |
| 25 % reordered, RTT 50 ms | 85 | 83 | 0 | 76 | 4.3 | 0 |
| 1 % corrupted | 34 | 34 | 0 | 75 | 4.5 | 0 |
| RTT 50 ms, jitter 40 ms | 75 | 72 | 0 | 108 | 5.4 | 0 |
| 4G: RTT 60 ms, jitter, 1 % loss | 78 | 76 | 0 | 79 | 3.5 | 0 |
| bad Wi-Fi: 5 % loss, jitter | 88 | 73 | 0 | 111 | 4.4 | 0 |
| mobile: RTT 120 ms, 3 % loss, 400 kbit/s | 501 | 177 | 0 | 102 | 4.4 | 0 |
<!-- END conditions -->

Duplicated packets, reordered packets and 1 percent of corrupted packets have no
visible effect. The tracker drops the duplicates, the corrupted datagrams fail a
length check and are dropped like lost ones, and a reordered snapshot is
ignored by the prediction when it is older than the last one used. The three
profiles are playable: the congested mobile link (RTT 120 ms, 3 percent loss,
400 kbit/s shared by four players) went from 501 ms to 177 ms of confirmation
delay, thanks to the smaller snapshots and to the snapshot rate adaptation. A
network with 40 ms of jitter, which reorders packets, is rated playable.

### Accuracy of the prediction

![Line chart. The horizontal axis lists ten network conditions. The vertical axis is the distance in pixels between the predicted ship and the position given by the server. The median is below 1 pixel everywhere. The 95th percentile is at most 7 pixels and the largest gap reaches 30 pixels.](netbench/prediction_error.svg)

**Text description of the figure.** Under ten conditions, from no degradation to a
congested mobile link, the median distance between the predicted ship and the
position it should have according to the server is at most 0.8 pixel. The 95th
percentile is at most 7 pixels. The largest distance is 30 pixels. The screen is
1280 pixels wide.

Over the 88 runs, the prediction was compared with the server 18 835 times. It
corrected the ship smoothly 3 968 times (21 percent) and never had to move it at
once. The largest gap of all was 81 pixels, with two players sharing a 5 KB/s
link. A player sees these corrections as a ship that slides slightly, not as a
jump.

### Bandwidth

<!-- BEGIN bandwidth -->
| Case | Down before (KB/s) | Down after (KB/s) | Up before (KB/s) | Up after (KB/s) | Up packets/s before | Up packets/s after | Down packets/s after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 players, no limit | 7.7 | 3.4 | 1.90 | 0.45 | 61 | 15 | 27 |
| 1 player, no limit | 5.9 | 2.1 | 1.90 | 0.38 | 61 | 13 | 25 |
<!-- END bandwidth -->

The figures count the bytes of the game protocol. Each datagram adds 28 bytes of
IPv4 and UDP headers on the wire. Per client, with four players, that makes about
5.0 KB/s on the wire in total now (4.1 down, 0.9 up) against 12.4 KB/s before
(8.9 down, 3.6 up), 60 percent less. With one player it is 3.5 KB/s.

Both directions went down, for two different reasons:

- **Up.** The client sends 13 to 15 packets per second instead of 61, because an
  input is now sent only when the keys change and every 100 ms otherwise.
- **Down.** A capture of the traffic of the server showed that 98 percent of it was
  snapshots and that 92 percent of their entries were stage scenery (ground
  tiles, ramps, reactors, gates). The scenery scrolls at a constant velocity that
  the client knows, so it is now sent once and then only every 1.5 to 2.5 seconds.
  The average snapshot went from 442 to 105 bytes, and the way down from 7.7 to
  3.4 KB/s per client with four players.

### Shooting

A player on a narrow link could move, but shots seemed not to work: the missile
appears only when the server creates it, and the message came back late. This is
the time between the release of the fire button and the moment the missile is
visible:

<!-- BEGIN shots -->
| One player shooting | Missile visible p50 (ms) | p95 (ms) | Longest (ms) | Shots measured | Shots never seen | Disconnections |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| no degradation | 17 | 17 | 17 | 8 | 0 | 0 |
| RTT 100 ms | 101 | 118 | 118 | 8 | 0 | 0 |
| 10 % loss | 17 | 50 | 50 | 9 | 0 | 0 |
| 40 KB/s link | 17 | 18 | 18 | 9 | 0 | 0 |
| 20 KB/s link | 17 | 18 | 18 | 10 | 0 | 0 |
| 10 KB/s link | 18 | 19 | 19 | 10 | 0 | 0 |
| 5 KB/s link | 88 | 122 | 122 | 10 | 0 | 0 |
<!-- END shots -->

Without degradation the missile is visible in 17 ms, which is one tick of the
server. With 100 ms of RTT it takes one round trip time more, as the missile is
created by the server and not predicted. On a link of 10 KB/s or more it is not
affected, and on a 5 KB/s link it takes 88 ms in the median and 122 ms at most, no shot
was lost. Before the fixes of the resend delay and of the snapshots the same test took
810 ms in the median and up to 1.5 s on a 5 KB/s link.

### Narrow links

![Line chart. The horizontal axis is the capacity of the link in KB/s, 5, 10, 20 and 40, on a logarithmic scale. The vertical axis is the delay before the ship moves. Before the changes the delay is 775 ms at 10 KB/s and no movement was seen at 5 KB/s. After, the delay confirmed by the server is 40 ms at 10 KB/s and 58 ms at 5 KB/s, and the ship seen by the player moves at once on every link.](netbench/narrow_links.svg)

**Text description of the figure.** With one player, a link of 20 or 40 KB/s works
the same before and after, 47 and 41 ms. At 10 KB/s the delay before the ship moved
was 775 ms and is now 40 ms. At 5 KB/s the ship never moved in the before
runs, and the server confirms the movement after 58 ms now. The ship seen by the
player moves at once on every link.

<!-- BEGIN links -->
| Link (one player) | Delay before (ms) | Delay after, confirmed (ms) | Delay after, seen (ms) | Updates before | Updates after | Update every, after (ms) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| no limit | 34 | 34 | 0 | 100 % | 100 % | 50 |
| 40 KB/s | 41 | 35 | 0 | 100 % | 100 % | 50 |
| 20 KB/s | 47 | 38 | 0 | 100 % | 100 % | 50 |
| 10 KB/s | 775 | 40 | 0 | 89 % | 100 % | 50 |
| 5 KB/s | no sample | 58 | 0 | 39 % | 95 % | 50 |
<!-- END links -->

When several players are in the room, they share the link:

<!-- BEGIN shared_links -->
| Link shared by the room | Delay before (ms) | Delay after, confirmed (ms) | Delay after, seen (ms) | Updates before | Updates after | Disconnections after |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 10 KB/s for 2 players | not measured | 35 | 0 | not measured | 82 % | 0 |
| 5 KB/s for 2 players | not measured | 361 | 0 | not measured | 31 % | 0 |
| 20 KB/s for 4 players | 663 | 157 | 0 | 45 % | 66 % | 0 |
| 10 KB/s for 4 players | 749 | 867 | 0 | 63 % | 30 % | 0 |
| 5 KB/s for 4 players | no sample | no sample | no sample | 53 % | 17 % | 0 |
| 50 KB/s for 4 players | 481 | 45 | 0 | 86 % | 100 % | 0 |
<!-- END shared_links -->

Two players can share 10 KB/s (82 percent of the updates, 35 ms) and still play on
5 KB/s with long delays (361 ms, 31 percent of the updates). Four players need about
20 KB/s: below that the game degrades but nobody is disconnected, whereas before the
clients were disconnected or the game did not start.

The server watches the round trip time of every client. When a queue fills in
front of a link, it sends fewer snapshots to that client (down to 5 per second),
so the queue empties and the delay comes back. With the smaller snapshots a
single player no longer fills a 5 KB/s link, the 5 KB/s link of one player is
rated playable, and the adaptation mostly protects rooms of several players
and the congested mobile profile.

## What was changed

Several changes came out of the rounds of measurements:

1. **Prediction with reconciliation of the local ship.** The ship moves at once,
   with the movement of the server, and each snapshot corrects it. This removes the
   round trip time from what the player feels (@ref networking_latency).
2. **Inputs sent when they change.** 13 to 15 packets per second instead of 61,
   with two repeats and a 100 ms heartbeat against packet loss.
3. **Scenery sent only now and then.** The snapshot went from 442 to 105 bytes.
4. **Snapshot rate adapted to the link.** A client whose smallest round trip time
   stays more than 80 ms above its normal value for two checks is sent fewer
   snapshots. The round trip time is measured on every acknowledged packet,
   because the one used for resends was too rare to see a queue (28 ms measured
   while the real delay was 775 ms). A first version reacted too fast and slowed
   the snapshots down on jitter and loss (54 percent of the updates received at 40
   ms of jitter): this benchmark showed it, and the second version fixed it.
5. **Resend delay that follows the queue.** A critical packet was sent again
   every 100 ms while it waited in the queue of a saturated link, which made the
   queue longer (@ref networking_latency, *Resends on a saturated link*).

The protocol version went from 2 to 3 because the `Snapshot` header and the
meaning of `Input` changed. A snapshot also no longer lists the stage scenery.

## Limits and known problems

- **The players are bots.** They move at random, run at a fixed 60 FPS and die
  in about 15 seconds. A human player holds keys longer, and a real client has
  frames of different durations, which makes the prediction gap larger than
  measured here, where it is exactly 0 with no network delay. The graphical
  client uses the same prediction code, and shows the same measures live with
  the F3 key (see below).
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
- **The way down is still 3.4 KB/s per client with four players.** Every
  snapshot carries the full position of every entity that is not scenery. Sending
  only what changed, or what cannot be guessed (enemies and missiles move in
  straight lines), would reduce it further.
- **Four players on less than 20 KB/s** do not get a good game, even if nobody is
  disconnected any more.
- **Missiles are not predicted.** They appear when the server creates them, one
  round trip time after the key was released.
- **The speed of a ship is read from its blueprint.** A speed power up is applied
  by the client and the server with the same rule, but if they ever disagree
  the prediction is corrected at each snapshot, and the ship slides.

## Playing under degraded conditions

To feel a condition with the real client, run `tools/netbench/play.sh`. It opens a
shell in a network namespace where the server is already running, and where
the conditions can be changed while the game is running:

```text
tools/netbench/play.sh delay 50ms loss 2%    # start with RTT 100 ms and 2 % loss
bin/r-type_client &                          # one client per call, as many as you want
netem delay 100ms                            # RTT 200 ms
netem loss 10%                               # 10 % of the packets lost in each direction
netem limit 30 rate 40kbit                   # a link of 5 KB/s
netem_off                                    # no degradation
```

The server and every client started from this shell talk through the degraded
loopback of the namespace. `exit` stops the server.

In the client, F3 shows the network metrics on screen: round trip time, packet
loss, bandwidth in both directions, wait between two world updates, delay before
the server confirms an input, gap of the predicted ship, and a verdict rated with
the same thresholds as this page. The complete list, with the percentiles, is
written on the console when F3 is pressed and every five seconds after.

On Windows the subject recommends `clumsy`: the same conditions can be set with it.
