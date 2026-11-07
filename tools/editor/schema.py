from __future__ import annotations

import copy
from typing import Any

LAYERS = ["Player", "Enemy", "PlayerProjectile", "EnemyProjectile",
          "Environment", "PowerUp", "BossRoom"]
ANIM_STATES = ["IDLE", "UP", "DOWN", "DIE", "SHOOT"]
POWERUP_TYPES = ["heal", "speed", "shield"]


def field(key: str, type_: str, default: Any = None, **extra: Any) -> dict:
    return {"key": key, "type": type_, "default": default, "label": key, **extra}


def group(key: str, subs: list[tuple[str, str, Any]], **extra: Any) -> dict:
    default = {name: value for name, _, value in subs}
    return field(key, "group", default, sub=subs, **extra)


def vec2(key: str, x: float = 0.0, y: float = 0.0) -> dict:
    return group(key, [("x", "float", x), ("y", "float", y)])


def size2(key: str, w: float = 32.0, h: float = 32.0) -> dict:
    return group(key, [("w", "float", w), ("h", "float", h)])


def rect(key: str) -> dict:
    return group(key, [("x", "int", 0), ("y", "int", 0),
                       ("w", "int", 32), ("h", "int", 32)])


def color(key: str) -> dict:
    return group(key, [("r", "int", 255), ("g", "int", 255),
                       ("b", "int", 255), ("a", "int", 255)], color=True)


CLIP_FIELDS = [
    field("startX", "int", 0), field("startY", "int", 0),
    field("totalFrames", "int", 1), field("frameDuration", "float", 0.1),
    field("loop", "bool", False), field("reverse", "bool", False),
]

COMPONENTS: dict[str, dict] = {
    "transform": {"fields": [vec2("position"), vec2("scale", 1.0, 1.0),
                             field("rotation", "float", 0.0)]},
    "sprite": {"fields": [
        field("texture", "texture", ""), size2("size"), vec2("origin"),
        rect("textureRect"), color("color"), field("layer", "int", 0),
        field("flags", "int", 0, optional=True)]},
    "collider": {"fields": [
        field("width", "float", 32.0), field("height", "float", 32.0),
        field("offsetX", "float", 0.0), field("offsetY", "float", 0.0),
        field("layer", "enum", "Environment", options=LAYERS),
        field("mask", "flags", [], options=LAYERS)]},
    "velocity": {"fields": [field("x", "float", 0.0), field("y", "float", 0.0)]},
    "animation": {"fields": [
        field("frameDuration", "float", 0.1), field("currentFrame", "int", 0),
        field("totalFrames", "int", 1), field("frameWidth", "int", 32),
        field("frameHeight", "int", 32), field("startX", "int", 0),
        field("startY", "int", 0), field("loop", "bool", True),
        field("playing", "bool", True), field("reverse", "bool", False),
        field("columns", "int", 0), field("paddingX", "int", 0),
        field("paddingY", "int", 0)]},
    "animator": {"fields": [
        field("currentState", "enum", "IDLE", options=ANIM_STATES),
        field("clips", "clips", {})]},
    "enemy": {"fields": [
        field("health", "float", 100.0), field("fireRate", "float", 2.0),
        field("blueprintName", "blueprint", "enemy_bullet", optional=True)]},
    "health": {"fields": [field("max", "float", 100.0),
                          field("current", "float", 100.0, optional=True)]},
    "controllable": {"fields": [
        field("speed", "float", 300.0),
        field("isChargingShoot", "bool", False, optional=True)]},
    "parallax": {"fields": [field("speedX", "float", 50.0),
                            field("loopWidth", "float", 1920.0)]},
    "waveMovement": {"fields": [
        field("baseY", "float", 300.0), field("amplitude", "float", 120.0),
        field("frequency", "float", 2.5), field("time", "float", 0.0)]},
    "attachedTo": {"fields": [
        field("targetEntity", "int", 0), field("offsetX", "float", 0.0),
        field("offsetY", "float", 0.0)]},
    "deathEffect": {"fields": [field("blueprintName", "blueprint", "explosion")]},
    "dropPowerUp": {"fields": [
        field("blueprintName", "blueprint", "powerup_speed")]},
    "powerup": {"fields": [field("type", "enum", "speed", options=POWERUP_TYPES),
                           field("value", "float", 20.0)]},
    "bullet": {"fields": [field("damage", "float", 10.0)]},
    "destroyableTile": {"fields": [
        field("explosionDamage", "float", 20.0),
        field("isTriggered", "bool", False)]},
    "enemyEvent": {"fields": [field("entityId", "int", 0, optional=True)]},
    "soundRequest": {"fields": [field("soundId", "int", 0)]},
    "luaScript": {"fields": [field("script", "str", ""),
                             field("time", "float", 0.0, optional=True)]},
    "destroyAnimEnd": {"fields": []},
    "networkEntity": {"fields": []},
    "dead": {"fields": []},
}

_BY_LOWER = {name.lower(): name for name in COMPONENTS}
ALIASES = {"aabbcollider": "collider", "enemyescapedevent": "enemyEvent"}


def canonical(name: str) -> str | None:
    return _BY_LOWER.get(name.lower()) or ALIASES.get(name.lower())


def fields_of(name: str) -> list[dict] | None:
    key = canonical(name)
    return COMPONENTS[key]["fields"] if key else None


def default_value(fld: dict) -> Any:
    return copy.deepcopy(fld["default"])


def default_component(name: str) -> dict:
    key = canonical(name)
    if key is None:
        return {}
    return {f["key"]: default_value(f) for f in COMPONENTS[key]["fields"]
            if not f.get("optional")}
