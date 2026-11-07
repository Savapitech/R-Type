#include "Test.hpp"

#include <Registry.hpp>
#include <Scheduler.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "EngineContext.hpp"
#include "GameEngine.hpp"
#include "components/NetworkEntity.hpp"
#include "components/Transform.hpp"
#include "components/Velocity.hpp"
#include "events/busEvents.hpp"
#include "factoryDataDriven/factoryDataDriven.hpp"
#include "inputAction/InputAction.hpp"
#include "inputAction/inputMapping.hpp"
#include "sceneManager/sceneManager.hpp"
#include "systems/InputSystem.hpp"

using json = nlohmann::json;
using Key = rtk::ecs::concepts::Key;
using MouseButton = rtk::ecs::concepts::MouseButton;
using GamepadButton = rtk::ecs::concepts::GamepadButton;
using GamepadAxis = rtk::ecs::concepts::GamepadAxis;
using rtk::Input::InputBinding;
using rtk::Input::InputMapping;

// busEvent

namespace {

struct Hit {
  int damage = 0;
};
struct Heal {
  int amount = 0;
};

} // namespace

TEST(EventBus, SubscriberReceivesTheEvent) {
  busEvent bus;
  int received = 0;
  bus.subscribe<Hit>([&](const Hit &hit) { received = hit.damage; });

  bus.publish(Hit{12});
  CHECK_EQ(received, 12);
}

TEST(EventBus, EverySubscriberIsCalledInOrder) {
  busEvent bus;
  std::vector<int> calls;
  bus.subscribe<Hit>([&](const Hit &) { calls.push_back(1); });
  bus.subscribe<Hit>([&](const Hit &) { calls.push_back(2); });
  bus.subscribe<Hit>([&](const Hit &) { calls.push_back(3); });

  bus.publish(Hit{});
  CHECK(calls == (std::vector<int>{1, 2, 3}));
}

TEST(EventBus, EventTypesAreSeparated) {
  busEvent bus;
  int hits = 0;
  int heals = 0;
  bus.subscribe<Hit>([&](const Hit &) { ++hits; });
  bus.subscribe<Heal>([&](const Heal &) { ++heals; });

  bus.publish(Heal{5});
  bus.publish(Heal{5});
  CHECK_EQ(hits, 0);
  CHECK_EQ(heals, 2);
}

TEST(EventBus, PublishWithoutSubscriberDoesNothing) {
  busEvent bus;

  CHECK_NOTHROW(bus.publish(Hit{1}));
}

TEST(EventBus, EachPublishCallsAgain) {
  busEvent bus;
  int total = 0;
  bus.subscribe<Hit>([&](const Hit &hit) { total += hit.damage; });

  for (int i = 1; i <= 4; ++i)
    bus.publish(Hit{i});
  CHECK_EQ(total, 10);
}

// EntityFactory

namespace {

struct Factory {
  rtk::ecs::Registry registry;
  rtk::factory::EntityFactory factory;

  Factory() {
    registry.register_component<component::Transform>();
    registry.register_component<component::Velocity>();
    registry.register_component<component::NetworkEntity>();
    factory.registerBuilder("Transform", component::transformFromJson);
    factory.registerBuilder("Velocity", component::velocityFromJson);
  }

  static json ship() {
    return json::parse(R"({"components": {
        "Transform": {"x": 10, "y": 20},
        "Velocity": {"x": 5, "y": 0},
        "Unknown": {"value": 1}}})");
  }
};

/// Text file in the temporary directory, removed at the end of the test.
struct TempFile {
  std::filesystem::path path;

  TempFile(const std::string &name, const std::string &content) {
    path = std::filesystem::temp_directory_path() / ("rtype_test_" + name);
    std::ofstream(path) << content;
  }
  ~TempFile() {
    std::error_code error;
    std::filesystem::remove(path, error);
  }
};

} // namespace

TEST(EntityFactory, HashOfTheEmptyNameIsTheFoldedFnvBasis) {
  static_assert(rtk::hashArchetype("") == 0x1CD9);
  CHECK_EQ(rtk::hashArchetype(""), 0x1CD9);
}

TEST(EntityFactory, HashIsStableAndDistinguishesNames) {
  constexpr auto player = rtk::hashArchetype("player");
  CHECK_EQ(rtk::hashArchetype("player"), player);
  CHECK_NE(rtk::hashArchetype("player"), rtk::hashArchetype("enemy"));
  CHECK_NE(rtk::hashArchetype("player"), rtk::hashArchetype("Player"));
}

TEST(EntityFactory, RegisterBlueprintReturnsTheHash) {
  Factory f;

  const auto id = f.factory.registerBlueprint("ship", Factory::ship());
  CHECK_EQ(id, rtk::hashArchetype("ship"));
  CHECK_EQ(f.factory.getArchetypeID("ship"), id);
  CHECK(f.factory.hasArchetype(id));
  CHECK(f.factory.hasBlueprint("ship"));
  CHECK_EQ(f.factory.getBlueprintName(id), std::string("ship"));
}

TEST(EntityFactory, UnknownBlueprint) {
  Factory f;

  CHECK_FALSE(f.factory.hasBlueprint("ghost"));
  CHECK_FALSE(f.factory.hasArchetype(rtk::hashArchetype("ghost")));
  CHECK_THROWS(f.factory.getBlueprintName(rtk::hashArchetype("ghost")));
  CHECK_THROWS(f.factory.createFromBlueprint(f.registry, "ghost"));
  CHECK_THROWS(
      f.factory.createFromBlueprint(f.registry, rtk::hashArchetype("ghost")));
}

TEST(EntityFactory, CreateEntityRunsTheBuilders) {
  Factory f;

  const auto entity = f.factory.createEntity(f.registry, Factory::ship());
  const auto *transform =
      f.registry.get_components<component::Transform>().get(entity);
  const auto *velocity =
      f.registry.get_components<component::Velocity>().get(entity);
  CHECK(transform != nullptr);
  CHECK(velocity != nullptr);
  CHECK_EQ(transform->position.x, 10.0f);
  CHECK_EQ(velocity->x, 5.0f);
}

TEST(EntityFactory, EntityWithoutComponentsIsEmpty) {
  Factory f;

  const auto entity = f.factory.createEntity(f.registry, json::object());
  CHECK_FALSE(f.registry.has_component<component::Transform>(entity));
  CHECK_FALSE(f.registry.has_component<component::Velocity>(entity));
}

TEST(EntityFactory, CreatesFromNameAndFromId) {
  Factory f;
  const auto id = f.factory.registerBlueprint("ship", Factory::ship());

  const auto byName = f.factory.createFromBlueprint(f.registry, "ship");
  const auto byId = f.factory.createFromBlueprint(f.registry, id);
  CHECK_NE(byName, byId);
  CHECK(f.registry.has_component<component::Transform>(byName));
  CHECK(f.registry.has_component<component::Transform>(byId));
}

TEST(EntityFactory, CreateNetworkTagsTheEntity) {
  Factory f;
  const auto id = f.factory.registerBlueprint("ship", Factory::ship());

  const auto entity = f.factory.createNetwork(f.registry, id, 4242);
  const auto *network =
      f.registry.get_components<component::NetworkEntity>().get(entity);
  CHECK(network != nullptr);
  CHECK_EQ(network->id, 4242u);
  CHECK_EQ(network->archetypeId, id);
}

TEST(EntityFactory, CreateNetworkByName) {
  Factory f;
  f.factory.registerBlueprint("ship", Factory::ship());

  const auto entity = f.factory.createNetwork(f.registry, "ship", 7);
  const auto *network =
      f.registry.get_components<component::NetworkEntity>().get(entity);
  CHECK_EQ(network->id, 7u);
  CHECK_EQ(network->archetypeId, rtk::hashArchetype("ship"));
}

TEST(EntityFactory, LaterBuilderReplacesTheFirst) {
  Factory f;
  f.factory.registerBuilder("Velocity", [](rtk::ecs::Registry &reg,
                                           std::size_t entity, const json &) {
    reg.get_components<component::Velocity>().insert_at(
        entity, component::Velocity{1, 2});
  });

  const auto entity = f.factory.createEntity(f.registry, Factory::ship());
  CHECK_EQ(f.registry.get_components<component::Velocity>().get(entity)->x,
           1.0f);
}

TEST(EntityFactory, LoadsABlueprintFile) {
  Factory f;
  TempFile file("blueprint.json", Factory::ship().dump());

  const auto id = f.factory.loadBlueprintFromPath("ship", file.path.string());
  CHECK_EQ(id, rtk::hashArchetype("ship"));
  const auto entity = f.factory.createFromBlueprint(f.registry, id);
  CHECK(f.registry.has_component<component::Velocity>(entity));
}

TEST(EntityFactory, MissingFileThrows) {
  Factory f;

  CHECK_THROWS(f.factory.loadBlueprintFromPath(
      "ship", "/nonexistent/rtype_blueprint.json"));
  CHECK_THROWS(
      f.factory.loadBlueprintManifest("/nonexistent/rtype_manifest.json"));
}

TEST(EntityFactory, LoadsAManifest) {
  Factory f;
  TempFile blueprint("manifest_ship.json", Factory::ship().dump());
  json manifest;
  manifest["blueprints"] =
      json::array({{{"name", "ship"}, {"file", blueprint.path.string()}},
                   {{"name", "copy"}, {"file", blueprint.path.string()}}});
  TempFile file("manifest.json", manifest.dump());

  f.factory.loadBlueprintManifest(file.path.string());
  CHECK(f.factory.hasBlueprint("ship"));
  CHECK(f.factory.hasBlueprint("copy"));
}

TEST(EntityFactory, ManifestWithoutBlueprintsThrows) {
  Factory f;
  TempFile file("bad_manifest.json", R"({"entries": []})");

  CHECK_THROWS(f.factory.loadBlueprintManifest(file.path.string()));
}

// InputMapping

TEST(InputMapping, StartsWithoutBindings) {
  const InputMapping mapping;

  for (const auto &binding : mapping.getBindings())
    CHECK_EQ(binding.type, InputBinding::Type::NoneAction);
  CHECK_EQ(mapping.getPrimaryBoundKey(0), Key::Unknown);
}

TEST(InputMapping, BindKeyUsesTheActionSlots) {
  InputMapping mapping;

  mapping.bindKey(3, Key::Space);
  const auto &binding =
      mapping.getBindings()[3 * InputMapping::BINDINGS_PER_ACTION];
  CHECK_EQ(binding.type, InputBinding::Type::Keyboard);
  CHECK_EQ(binding.key, Key::Space);
  CHECK_EQ(mapping.getPrimaryBoundKey(3), Key::Space);
}

TEST(InputMapping, SeveralBindingsPerAction) {
  InputMapping mapping;

  mapping.bindKey(0, Key::A);
  mapping.bindGamepad(0, GamepadButton::A);
  mapping.bindMouse(0, MouseButton::Left);
  mapping.bindGamepadAxis(0, GamepadAxis::LeftX, -0.5f);
  const auto &bindings = mapping.getBindings();
  CHECK_EQ(bindings[0].type, InputBinding::Type::Keyboard);
  CHECK_EQ(bindings[1].type, InputBinding::Type::GamepadButton);
  CHECK_EQ(bindings[2].type, InputBinding::Type::Mouse);
  CHECK_EQ(bindings[3].type, InputBinding::Type::GamepadAxis);
  CHECK_EQ(bindings[3].axisData.axis, GamepadAxis::LeftX);
  CHECK_EQ(bindings[3].axisData.validationPression, -0.5f);
}

TEST(InputMapping, FifthBindingIsIgnored) {
  InputMapping mapping;

  for (int i = 0; i < 4; ++i)
    mapping.bindKey(1, Key::A);
  mapping.bindKey(1, Key::B);
  for (std::size_t i = 0; i < InputMapping::BINDINGS_PER_ACTION; ++i)
    CHECK_EQ(mapping.getBindings()[InputMapping::BINDINGS_PER_ACTION + i].key,
             Key::A);
  CHECK_EQ(mapping.getBindings()[2 * InputMapping::BINDINGS_PER_ACTION].type,
           InputBinding::Type::NoneAction);
}

TEST(InputMapping, OutOfRangeActionIsIgnored) {
  InputMapping mapping;

  mapping.bindKey(InputMapping::MAX_ACTIONS, Key::A);
  mapping.rebindKey(InputMapping::MAX_ACTIONS, Key::A);
  mapping.clearActionBindings(InputMapping::MAX_ACTIONS);
  for (const auto &binding : mapping.getBindings())
    CHECK_EQ(binding.type, InputBinding::Type::NoneAction);
  CHECK_EQ(mapping.getPrimaryBoundKey(InputMapping::MAX_ACTIONS), Key::Unknown);
}

TEST(InputMapping, ClearRemovesTheBindingsOfOneAction) {
  InputMapping mapping;

  mapping.bindKey(0, Key::A);
  mapping.bindKey(1, Key::B);
  mapping.clearActionBindings(0);
  CHECK_EQ(mapping.getPrimaryBoundKey(0), Key::Unknown);
  CHECK_EQ(mapping.getPrimaryBoundKey(1), Key::B);
}

TEST(InputMapping, RebindReplacesTheKey) {
  InputMapping mapping;

  mapping.bindKey(2, Key::W);
  mapping.bindGamepad(2, GamepadButton::A);
  mapping.rebindKey(2, Key::Z);
  CHECK_EQ(mapping.getPrimaryBoundKey(2), Key::Z);
  CHECK_EQ(
      mapping.getBindings()[2 * InputMapping::BINDINGS_PER_ACTION + 1].type,
      InputBinding::Type::GamepadButton);
}

TEST(InputMapping, RebindKeepsASingleKey) {
  InputMapping mapping;

  mapping.bindKey(2, Key::W);
  mapping.bindKey(2, Key::Up);
  mapping.rebindKey(2, Key::Z);
  int keys = 0;
  for (std::size_t i = 0; i < InputMapping::BINDINGS_PER_ACTION; ++i) {
    const auto &binding =
        mapping.getBindings()[2 * InputMapping::BINDINGS_PER_ACTION + i];
    if (binding.type == InputBinding::Type::Keyboard)
      ++keys;
  }
  CHECK_EQ(keys, 1);
  CHECK_EQ(mapping.getPrimaryBoundKey(2), Key::Z);
}

TEST(InputMapping, RebindAddsAKeyWhenThereIsNone) {
  InputMapping mapping;

  mapping.bindGamepad(4, GamepadButton::B);
  mapping.rebindKey(4, Key::X);
  CHECK_EQ(mapping.getPrimaryBoundKey(4), Key::X);
}

// InputSystem

namespace {

struct FakeWindow {
  std::set<Key> pressed;
  std::set<Key> released;
  std::set<MouseButton> mouse;
  std::set<GamepadButton> buttons;
  float axis = 0.0f;
  bool open = true;

  bool pollEvents(rtk::ecs::concepts::Event &event) {
    event.userData = this;
    event._isKeyPressed = [](void *self, Key key) {
      return static_cast<FakeWindow *>(self)->pressed.contains(key);
    };
    event._isKeyReleased = [](void *self, Key key) {
      return static_cast<FakeWindow *>(self)->released.contains(key);
    };
    event._isMouseButtonPressed = [](void *self, MouseButton button) {
      return static_cast<FakeWindow *>(self)->mouse.contains(button);
    };
    event._isGamepadButtonPressed = [](void *self, int, GamepadButton button) {
      return static_cast<FakeWindow *>(self)->buttons.contains(button);
    };
    event._getGamepadAxis = [](void *self, int, GamepadAxis) {
      return static_cast<FakeWindow *>(self)->axis;
    };
    return open;
  }
  bool beginFrame(rtk::ecs::concepts::ColorRGBA8) { return true; }
  void draw(std::span<const rtk::ecs::concepts::SpriteData>) {}
  void endFrame() {}
};

static_assert(rtk::ecs::concepts::IsRenderWindow<FakeWindow>);

struct InputWorld {
  FakeWindow window;
  rtk::InputState state;
  InputMapping mapping;
  rtk::ecs::Registry registry;

  void run(bool closeOnEscape = true) {
    rtk::systems::InputSystem<FakeWindow> system(std::ref(window), state,
                                                 mapping, closeOnEscape);
    system.update(registry, 0.016f);
  }
};

} // namespace

TEST(InputSystem, PressedKeySetsTheActionBit) {
  InputWorld world;
  world.mapping.bindKey(0, Key::Up);
  world.mapping.bindKey(4, Key::Space);
  world.window.pressed = {Key::Space};

  world.run();
  CHECK_EQ(world.state.actionMask, 1u << 4);
}

TEST(InputSystem, AnyBindingOfTheActionCounts) {
  InputWorld world;
  world.mapping.bindKey(1, Key::Down);
  world.mapping.bindGamepad(1, GamepadButton::A);
  world.mapping.bindMouse(2, MouseButton::Left);
  world.window.buttons = {GamepadButton::A};
  world.window.mouse = {MouseButton::Left};

  world.run();
  CHECK_EQ(world.state.actionMask, (1u << 1) | (1u << 2));
}

TEST(InputSystem, MaskIsRebuiltEveryFrame) {
  InputWorld world;
  world.mapping.bindKey(0, Key::Up);
  world.window.pressed = {Key::Up};
  world.run();

  world.window.pressed.clear();
  world.run();
  CHECK_EQ(world.state.actionMask, 0u);
}

TEST(InputSystem, AxisUsesTheThresholdDirection) {
  InputWorld world;
  world.mapping.bindGamepadAxis(0, GamepadAxis::LeftX, -0.5f);
  world.mapping.bindGamepadAxis(1, GamepadAxis::LeftX, 0.5f);

  world.window.axis = -0.8f;
  world.run();
  CHECK_EQ(world.state.actionMask, 1u << 0);
  world.window.axis = 0.8f;
  world.run();
  CHECK_EQ(world.state.actionMask, 1u << 1);
  world.window.axis = 0.2f;
  world.run();
  CHECK_EQ(world.state.actionMask, 0u);
}

TEST(InputSystem, ClosedWindowRequestsClose) {
  InputWorld world;
  world.window.open = false;

  world.run();
  CHECK(world.state.closeRequested);
}

TEST(InputSystem, EscapeRequestsClose) {
  InputWorld world;
  world.window.pressed = {Key::Escape};

  world.run();
  CHECK(world.state.closeRequested);
}

TEST(InputSystem, EscapeCanBeDisabled) {
  InputWorld world;
  world.window.pressed = {Key::Escape};

  world.run(false);
  CHECK_FALSE(world.state.closeRequested);
}

TEST(InputSystem, CloseRequestIsResetEveryFrame) {
  InputWorld world;
  world.window.open = false;
  world.run();

  world.window.open = true;
  world.run();
  CHECK_FALSE(world.state.closeRequested);
}

TEST(InputSystem, F3AndBToggleDebug) {
  InputWorld world;
  world.window.pressed = {Key::F3};
  world.window.released = {Key::B};

  world.run();
  CHECK(world.state.debug);
  world.run();
  CHECK_FALSE(world.state.debug);
}

TEST(InputSystem, F3AloneDoesNotToggleDebug) {
  InputWorld world;
  world.window.pressed = {Key::F3};

  world.run();
  CHECK_FALSE(world.state.debug);
}

// SceneManager

namespace {

enum class SceneId { Menu, Game, Missing };

struct SceneLog {
  std::vector<std::string> calls;
};

struct Frame {
  std::optional<SceneId> next;
};

struct MenuScene {
  static constexpr SceneId Type = SceneId::Menu;
  SceneLog &log;
  explicit MenuScene(SceneLog &sceneLog) : log(sceneLog) {
    log.calls.push_back("menu:create");
  }
  void onEnter(rtk::EngineContext &) { log.calls.push_back("menu:enter"); }
  std::optional<SceneId> onUpdate(rtk::EngineContext &, const Frame &frame,
                                  float) {
    log.calls.push_back("menu:update");
    return frame.next;
  }
  void onExit(rtk::EngineContext &) { log.calls.push_back("menu:exit"); }
};

struct GameScene {
  static constexpr SceneId Type = SceneId::Game;
  SceneLog &log;
  explicit GameScene(SceneLog &sceneLog) : log(sceneLog) {
    log.calls.push_back("game:create");
  }
  void onEnter(rtk::EngineContext &) { log.calls.push_back("game:enter"); }
  std::optional<SceneId> onUpdate(rtk::EngineContext &, const Frame &frame,
                                  float) {
    log.calls.push_back("game:update");
    return frame.next;
  }
  void onExit(rtk::EngineContext &) { log.calls.push_back("game:exit"); }
};

using Manager =
    rtk::SceneManager<SceneId, SceneLog, Frame, MenuScene, GameScene>;

struct SceneWorld {
  rtk::ecs::Registry registry;
  rtk::ecs::Scheduler scheduler;
  busEvent bus;
  rtk::EngineContext context{registry, scheduler, bus, {}};
  SceneLog log;
  Manager manager{log, std::in_place_type<MenuScene>};
};

} // namespace

TEST(SceneManager, BuildsTheInitialScene) {
  SceneWorld world;

  CHECK(world.log.calls == std::vector<std::string>{"menu:create"});
  CHECK(world.manager.get<MenuScene>() != nullptr);
  CHECK(world.manager.get<GameScene>() == nullptr);
}

TEST(SceneManager, StartEntersTheScene) {
  SceneWorld world;

  world.manager.start(world.context);
  CHECK_EQ(world.log.calls.back(), std::string("menu:enter"));
}

TEST(SceneManager, UpdateWithoutRequestStays) {
  SceneWorld world;
  world.manager.start(world.context);

  world.manager.update(world.context, Frame{}, 0.016f);
  CHECK(world.manager.get<MenuScene>() != nullptr);
  CHECK_EQ(world.log.calls.back(), std::string("menu:update"));
}

TEST(SceneManager, SwitchesToTheRequestedScene) {
  SceneWorld world;
  world.manager.start(world.context);
  world.log.calls.clear();

  world.manager.update(world.context, Frame{SceneId::Game}, 0.016f);
  CHECK(world.manager.get<GameScene>() != nullptr);
  CHECK(world.log.calls ==
        (std::vector<std::string>{"menu:update", "menu:exit", "game:create",
                                  "game:enter"}));
}

TEST(SceneManager, StopExitsTheCurrentScene) {
  SceneWorld world;
  world.manager.start(world.context);
  world.manager.update(world.context, Frame{SceneId::Game}, 0.016f);

  world.manager.stop(world.context);
  CHECK_EQ(world.log.calls.back(), std::string("game:exit"));
}

TEST(SceneManager, UnknownSceneThrows) {
  SceneWorld world;
  world.manager.start(world.context);

  CHECK_THROWS(
      world.manager.update(world.context, Frame{SceneId::Missing}, 0.016f));
}

TEST(SceneManager, RequestingTheSameSceneRebuildsIt) {
  SceneWorld world;
  world.manager.start(world.context);
  world.log.calls.clear();

  world.manager.update(world.context, Frame{SceneId::Menu}, 0.016f);
  CHECK(world.log.calls ==
        (std::vector<std::string>{"menu:update", "menu:exit", "menu:create",
                                  "menu:enter"}));
}

// GameEngine

namespace {

struct CountingSystem final : rtk::ecs::ISystem {
  int &starts;
  int &updates;
  int &stops;
  bool fail = false;

  CountingSystem(int &s, int &u, int &e) : starts(s), updates(u), stops(e) {}
  void onStart(rtk::ecs::Registry &) override { ++starts; }
  void update(rtk::ecs::Registry &, float) override {
    ++updates;
    if (fail)
      throw std::runtime_error("system failure");
  }
  void onStop(rtk::ecs::Registry &) override { ++stops; }
};

struct CountedGame {
  int frames = 0;
  int maxFrames = 3;
  int loads = 0;
  int updates = 0;
  int unloads = 0;
  int systemStarts = 0;
  int systemUpdates = 0;
  int systemStops = 0;
  bool failingSystem = false;

  void onLoad(rtk::EngineContext &context) {
    ++loads;
    auto &system = context.scheduler.add<CountingSystem>(
        rtk::ecs::Order::Update, systemStarts, systemUpdates, systemStops);
    system.fail = failingSystem;
  }
  bool shouldContinue() { return frames++ < maxFrames; }
  void onUpdate(rtk::EngineContext &, float dt) {
    ++updates;
    if (dt < 0.0f || dt > 0.25f)
      throw std::runtime_error("bad delta time");
  }
  void onUnload(rtk::EngineContext &) { ++unloads; }
};

static_assert(rtk::Game<CountedGame>);

} // namespace

TEST(GameEngine, RunsUntilTheGameStops) {
  rtk::GameEngine engine;
  CountedGame game;

  CHECK_EQ(engine.run(game), 0);
  CHECK_EQ(game.loads, 1);
  CHECK_EQ(game.updates, 3);
  CHECK_EQ(game.unloads, 1);
}

TEST(GameEngine, DrivesTheScheduler) {
  rtk::GameEngine engine;
  CountedGame game;

  engine.run(game);
  CHECK_EQ(game.systemStarts, 1);
  CHECK_EQ(game.systemUpdates, 3);
  CHECK_EQ(game.systemStops, 1);
}

TEST(GameEngine, StopsSystemsWhenOneThrows) {
  rtk::GameEngine engine;
  CountedGame game;
  game.failingSystem = true;

  CHECK_THROWS(engine.run(game));
  CHECK_EQ(game.systemUpdates, 1);
  CHECK_EQ(game.systemStops, 1);
  CHECK_EQ(game.unloads, 0);
}

TEST(GameEngine, ExposesItsRegistry) {
  rtk::GameEngine engine;

  engine.registry().register_component<component::Velocity>();
  const auto entity = engine.registry().spawn_entity();
  engine.registry().get_components<component::Velocity>().insert_at(
      entity, component::Velocity{1, 1});
  CHECK(engine.registry().has_component<component::Velocity>(entity));
}

TEST(GameEngine, ZeroFramesStillLoadsAndUnloads) {
  rtk::GameEngine engine;
  CountedGame game;
  game.maxFrames = 0;

  engine.run(game);
  CHECK_EQ(game.loads, 1);
  CHECK_EQ(game.updates, 0);
  CHECK_EQ(game.systemStarts, 1);
  CHECK_EQ(game.systemStops, 1);
  CHECK_EQ(game.unloads, 1);
}
