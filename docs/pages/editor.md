# Level and entity editor {#editor}

[TOC]

`tools/editor` is a desktop editor written in Python (PySide6) that produces the
JSON files read by the game: stages, entity blueprints and the textures they
use. It needs no C++ build, the files it writes are picked up the next time the
game is built (assets are embedded at build time).

## Run it

```sh
python3 -m venv .venv && . .venv/bin/activate
pip install -r tools/editor/requirements.txt
python3 -m tools.editor            # edits this repository
python3 -m tools.editor path/to/project   # any folder that contains assets/
```

`tools/editor/run.sh` starts the editor from the repository's `.venv`. On
Linux without root access (for example on WSL), it also uses the system
libraries extracted in `.venv/syslibs` and forces the X11 (xcb) Qt platform; on
macOS the native (cocoa) platform is used. On Windows, use `tools\editor\run.bat`
instead: it creates the `.venv` and installs the requirements on the first run.

`python3 -m tools.editor --selftest` validates every JSON file under `assets/`
(syntax, missing textures, unknown blueprints in stages) and smoke-tests the
editors on a temporary copy of the assets.

## What it contains

| Tab | Opens | Purpose |
| --- | ----- | ------- |
| Level editor | `assets/stages/stageN.json` | Place terrain and timed spawns by drag and drop |
| Blueprint editor | `assets/descriptionEntity/*.json`, `assets/powerUp/*.json` | Edit the components of an entity, with a live preview |
| Sprite editor | `*.png` | Slice a spritesheet, set up animations, draw pixel art |
| Text editor | any other file | JSON syntax highlighting, validation, find and replace |

Every structured tab has undo and redo, Ctrl+S saves, and a **Raw JSON** button
opens the same file in the text editor. The **Stages** dock lists
`stages.json`: drag stages to reorder them and the `nextStage` links of
`stages.json` and of every stage file follow the order.

### Level editor

- The palette lists every blueprint of `assets/blueprints.json`, grouped as
  terrain, enemies, power-ups and others. Drag one onto the canvas, or double
  click it.
- A **terrain** blueprint is inserted in the `segments` list at the dropped
  position, drag segments horizontally to reorder them. `spacing` and
  `symmetric` are edited in the inspector.
- Anything else becomes an entry of `spawns`. The canvas shares one coordinate
  system with the terrain, so an enemy dropped above a piece of terrain appears
  when that piece enters the screen. The ruler on top shows the time in seconds.
- **Preview** scrolls the camera at `scrollSpeed` and plays the spawns with the
  velocity and wave movement of their blueprint.
- Wheel zooms, middle mouse pans, Delete removes, Ctrl+D duplicates, the grid
  snap can be turned off in the toolbar.

### Sprite editor

The *Spritesheet* tab selects a region with the mouse, computes the animation
frames the same way as the engine's animation system, plays them, and keeps a
list of animator clips (`IDLE`, `UP`, ...). **Apply** merges the result
(`sprite.texture`, `sprite.size`, `sprite.textureRect`, and optionally
`animation` and `animator`) into the chosen blueprint, **New blueprint from
this** creates one. The *Pixel art* tab is a small painter (pencil, eraser,
fill, picker, line, rectangle, mirror modes) that saves the PNG in place. Use
**File > New image** to create a new texture, then **Add frame** to extend it
into a spritesheet.

## File formats

### `assets/blueprints.json`

The list of blueprints loaded by both the client and the server, through
rtk::factory::EntityFactory::loadBlueprintManifest(). The name is hashed to get
the archetype id, so it must be the same on both sides, which is why a single
shared file is used.

```json
{ "blueprints": [ { "name": "enemyBidos", "file": "assets/descriptionEntity/enemyBidos.json" } ] }
```

Adding a blueprint to the game is therefore adding a file and one line to this
manifest. The editor does both.

### Stage files

```json
{
  "name": "Stage 4",
  "nextStage": "",
  "duration": 0.0,
  "startX": 0.0,
  "scrollSpeed": 50.0,
  "randomSpawns": false,
  "segments": [ { "blueprint": "low_ground", "symmetric": false, "spacing": 512.0 } ],
  "spawns": [
    { "blueprint": "enemyBidos", "time": 3.5, "x": 1300.0, "y": 300.0 },
    { "blueprint": "enemyRedShip", "time": 8.0, "x": 1300.0, "y": 300.0,
      "wave": { "amplitude": 80.0, "frequency": 2.0 } }
  ]
}
```

| Key | Meaning |
| --- | ------- |
| `segments` | Terrain laid out one after the other, as before |
| `segments[].offsetX` | Optional, `0` by default. Shifts this entity from the current position without moving the next segments (place it with `spacing: 0` to pin several entities on the same spot) |
| `segments[].y` | Optional. Vertical position overriding the blueprint one. Keep it at `50` or more unless the sprite should be flipped: the client flips every entity created above `y = 50` |
| `spawns` | Optional. Entities created by the server once the stage timer reaches `time` seconds, at `(x, y)` in screen pixels (the screen is 1280x720, so `x = 1300` is just off the right edge) |
| `spawns[].wave` | Optional, only for blueprints with a `waveMovement` component: overrides its `amplitude` and `frequency`, the base height is `y` |
| `randomSpawns` | Optional, `true` by default. Set it to `false` to turn off the random Bydos and Mecha spawner for this stage |
| `enemies` | Optional, `stage1` by default. Name of the enemy pool the random spawner picks from (`stage1`, `stage2`, …). The pools are the vectors of `rtk::systems::EnemySpawnerSystem`; an unknown name falls back to `stage1` with a warning |
| `boss` | Optional, `dobkeratops` by default. `dobkeratops` spawns that boss when a `BossRoom` entity reaches the left edge and the scrolling stops. `parts` means the boss is already in the stage: every entity with a `bossPart` component, the stage ends once they are all destroyed, and if `duration` is set and runs out first, the boss gets away and the game is lost (stage 3 warship thrusters) |
| `scrollSpeed` | Optional, used by the editor to convert between time and distance, the terrain blueprints move at `-50` px/s |
| `duration` | Optional, `0` means no time limit |

A stage ends when its `duration` is reached, or when all the terrain has left the
screen and every scripted spawn has been played. A stage without terrain, with
`randomSpawns` set to `false`, ends when its last scripted enemy is gone. With the
random spawner on and no terrain, set a `duration`.

The loading code is `rtype::server::GameInstance::loadStage()` and
`rtype::server::GameInstance::spawnDueStageEntities()`.
