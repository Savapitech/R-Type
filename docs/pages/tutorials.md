# Tutorials and how-tos {#tutorials}

[TOC]

Short, task oriented recipes. Each one lists the files to touch and the code to
write. Read @ref engine first if the words entity, component or system are new
to you.

## Add a new kind of entity with a blueprint

Goal: add a new enemy without writing C++ for its data.

1. Create `assets/descriptionEntity/enemyScout.json`:

   ```json
   {
     "components": {
       "transform": { "position": { "x": 0, "y": 0 }, "scale": { "x": 2, "y": 2 } },
       "sprite": {
         "texture": "assets/entityTexture/scout.png",
         "size": { "w": 32, "h": 32 },
         "textureRect": { "x": 0, "y": 0, "w": 32, "h": 32 },
         "layer": 0
       },
       "collider": { "width": 32, "height": 32, "layer": "Enemy", "mask": ["Player", "PlayerProjectile"] },
       "velocity": { "x": -320, "y": 0 },
       "enemy": { "health": 50 },
       "networkEntity": {}
     }
   }
   ```

2. Load it on **both** sides, so that both compute the same archetype id. In
   `rtype::server::Game::loadEntityBlueprints()` and in the client
   `loadEntityBluePrint()`:

   ```cpp
   context._entityFactory.loadBlueprintFromPath(
       "enemyScout", "assets/descriptionEntity/enemyScout.json");
   ```

3. Spawn it on the server with a network id, then tell the clients:

   ```cpp
   const auto netId = rtype::server::NetworkIdGenerator::getInstance().getNextId();
   const auto entity = context._entityFactory.createNetwork(context.registry, "enemyScout", netId);
   // set its Transform, then send protocol::createEntityPayload(...) to every player
   ```

The client needs no extra code: it already instantiates any blueprint whose
archetype id it receives in `CreateEntity`.

Available component keys are the ones registered with
rtk::factory::EntityFactory::registerBuilder() in each `Game`, for example
`transform`, `sprite` (client only), `collider`, `velocity`, `animation`,
`animator`, `controllable`, `enemy`, `bullet`, `parallax` and `networkEntity`.

## Add a component

1. Declare a plain struct and its JSON builder, next to the other components:
   `lib/GameEngine/include/components` for generic data, `src/Common/components`
   for gameplay data.

   ```cpp
   namespace component {
   /**
    * @brief Remaining shield points of an entity.
    */
   struct Shield {
     float points{0.0f}; ///< Points left before the shield breaks.
   };

   /**
    * @brief Adds a Shield to an entity from its blueprint data.
    * @param reg Registry owning the entity.
    * @param entity Target entity.
    * @param data JSON object with an optional `points` field.
    */
   inline void shieldFromJson(rtk::ecs::Registry &reg, std::size_t entity,
                              const nlohmann::json &data) {
     reg.get_components<Shield>().insert_at(entity,
                                            Shield{data.value("points", 0.0f)});
   }
   } // namespace component
   ```

2. Register the storage and the builder in `registerComponents()` and
   `registerBuilders()` of every `Game` that uses it:

   ```cpp
   context.registry.register_component<component::Shield>();
   context._entityFactory.registerBuilder("shield", component::shieldFromJson);
   ```

3. Use the `shield` key in any blueprint.

## Add a system

1. Write a `final` class deriving from `rtk::ecs::ISystem`:

   ```cpp
   namespace rtk::systems {
   /**
    * @brief Regenerates shields over time.
    */
   class ShieldSystem final : public rtk::ecs::ISystem {
   public:
     /**
      * @brief Does nothing.
      */
     void onStart(rtk::ecs::Registry &) override {}

     /**
      * @brief Adds 5 points per second to every shield.
      * @param reg Registry holding the shields.
      * @param dt Elapsed time since the last frame, in seconds.
      */
     void update(rtk::ecs::Registry &reg, float dt) override {
       auto &shields = reg.get_components<component::Shield>();
       for (auto entity : reg.view<component::Shield>())
         shields.get(entity)->points += 5.0f * dt;
     }

     /**
      * @brief Does nothing.
      */
     void onStop(rtk::ecs::Registry &) override {}
   };
   } // namespace rtk::systems
   ```

2. Add it to the scheduler in `addSystems()`, choosing its stage:

   ```cpp
   context.scheduler.add<rtk::systems::ShieldSystem>(rtk::ecs::Order::Update);
   ```

   Constructor arguments follow the stage. Pass references with `std::ref()`.

Always multiply speeds and timers by `dt`, never count frames.

## React to an event

Any struct can be an event. Publish it from a system or the game, and subscribe
where it matters:

```cpp
struct ShieldBrokenEvent { std::size_t entity; };

context.eventBus.subscribe<ShieldBrokenEvent>(
    [&](const ShieldBrokenEvent &event) { /* play a sound, notify clients */ });

eventBus.publish(ShieldBrokenEvent{entity});
```

Handlers run immediately on the publishing thread. Keep them short.

## Add a scene

1. Write a class that satisfies rtk::Scene:

   ```cpp
   class PauseScene {
   public:
     static constexpr SceneType Type = SceneType::Pause;
     explicit PauseScene(rtk::RenderWindow &window);
     void onEnter(rtk::EngineContext &context);
     std::optional<SceneType> onUpdate(rtk::EngineContext &context,
                                       const rtk::InputState &input, float dt);
     void onExit(rtk::EngineContext &context);
   };
   ```

2. Add `Pause` to the `SceneType` enum.
3. Add `PauseScene` to the list of scenes of the `rtk::SceneManager` member of
   the client `Game`.
4. Add a `static_assert(rtk::Scene<PauseScene, SceneType, rtk::InputState>);`
   next to the others, to get a short error message if a method is missing.
5. Return `SceneType::Pause` from another scene `onUpdate()` to switch to it.

## Bind a new action

1. Add a bit to rtype::Action, for example `Pause = 1 << 6`.
2. Bind controls to that bit index in the client `createBindings()`:

   ```cpp
   _inputMapping.bindKey(6, rtk::ecs::concepts::Key::P);
   _inputMapping.bindGamepad(6, rtk::ecs::concepts::GamepadButton::Start);
   ```

3. Test it with `rtype::Input::hasAction(inputState.actionMask, rtype::Action::Pause)`.

Each action accepts up to four controls.

## Add a network message

1. Pick a free type: below `0x80` for client to server, from `0x80` for server
   to client. Add it to rtype::protocol::MessageType.
2. Declare the payload with only `std::uint8_t`, single byte enums,
   rtype::protocol::be16 and rtype::protocol::be32 fields, so that it has no
   padding, and assert its size:

   ```cpp
   /**
    * @brief Payload of a ScoreUpdate message, 8 bytes.
    *
    * | Offset | Size | Field       | Description               |
    * | -----: | ---: | ----------- | ------------------------- |
    * | 0      | 4    | `networkId` | Ship that scored.         |
    * | 4      | 4    | `score`     | New total score.          |
    */
   struct ScoreUpdatePayload {
     be32 networkId; ///< Ship that scored.
     be32 score;     ///< New total score.
   };
   static_assert(sizeof(ScoreUpdatePayload) == 8);
   ```

3. Send it with rtype::protocol::writePayload() and a send helper in
   rtype::server::Server, following rtype::server::Server::sendCreateEntity().
4. Decode it with rtype::protocol::readPayload(), check that the reader is at
   the end (exact size), and validate every field before use.
5. Document it in @ref protocol, with its direction, size, field table and validation
   rules.

## Measure the network

NetworkMetrics counts bytes, packets, duplicates, out of order packets and
smoothed round trip time. To measure latency from a client, send a `Ping` with
the current time in milliseconds and compare it with the `timestamp` of the
`Pong`.
