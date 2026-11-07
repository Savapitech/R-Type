from __future__ import annotations

import copy

from . import schema

ENEMY_MASK = ["PlayerProjectile", "Player", "Environment"]


def _comp(name: str, **overrides) -> dict:
    data = schema.default_component(name)
    for key, value in overrides.items():
        data[key] = value
    return data


def _base(scale: float = 1.0, layer: int = 0) -> dict:
    return {
        "transform": _comp("transform", scale={"x": scale, "y": scale}),
        "sprite": _comp("sprite", layer=layer),
    }


def make(kind: str) -> dict:
    if kind == "enemy":
        comps = _base(2.0)
        comps["collider"] = _comp("collider", width=28.0, height=28.0, layer="Enemy",
                                  mask=list(ENEMY_MASK))
        comps["velocity"] = _comp("velocity", x=-200.0)
        comps["animation"] = _comp("animation", totalFrames=2, columns=2,
                                   frameDuration=0.2)
        comps["enemy"] = _comp("enemy", health=1.0, fireRate=2.5)
        comps["deathEffect"] = _comp("deathEffect")
        comps["networkEntity"] = {}
    elif kind == "terrain":
        comps = _base(2.0, layer=-1)
        comps["collider"] = _comp("collider", width=128.0, height=16.0,
                                  layer="Environment",
                                  mask=["Player", "Enemy", "PlayerProjectile",
                                        "EnemyProjectile"])
        comps["velocity"] = _comp("velocity", x=-50.0)
        comps["networkEntity"] = {}
    elif kind == "powerup":
        comps = _base()
        comps["collider"] = _comp("collider", layer="PowerUp", mask=["Player"])
        comps["powerup"] = _comp("powerup")
        comps["velocity"] = _comp("velocity", x=-100.0)
        comps["networkEntity"] = {}
    elif kind == "effect":
        comps = _base(2.0, layer=10)
        comps["animation"] = _comp("animation", totalFrames=6, columns=6,
                                   frameDuration=0.05, loop=False)
        comps["destroyAnimEnd"] = {}
    else:
        comps = {"transform": _comp("transform"), "networkEntity": {}}
    return {"components": copy.deepcopy(comps)}


TEMPLATES = {
    "Enemy": "enemy",
    "Terrain": "terrain",
    "Power-up": "powerup",
    "Visual effect": "effect",
    "Empty": "empty",
}
