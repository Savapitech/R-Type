# Coding conventions {#coding_conventions}

[TOC]

These rules keep the code base consistent. When in doubt, follow the code
around you.

## Formatting

- Formatting is done by clang-format with the LLVM style. Run `make format`
  before committing and do not format by hand.
- Lines are at most 80 characters.
- Headers start with `#pragma once`.

## Naming

| Element | Style | Example |
| ------- | ----- | ------- |
| Classes and structs | PascalCase | `NetworkServer`, `SnapshotPayload` |
| Functions and methods | camelCase | `sendSnapshot()`, `pollInput()` |
| Local variables and parameters | camelCase | `connectionId` |
| Private data members | Underscore then camelCase | `_connections`, `_running` |
| Public data members | camelCase, no prefix | `networkId`, `actionMask` |
| Constants | UPPER_SNAKE_CASE | `MAX_PAYLOAD_SIZE`, `HEADER_SIZE` |
| Enum values | PascalCase | `MessageType::CreateEntity` |
| Namespaces | Lowercase | `rtk`, `rtype::server` |
| JSON builders | Component name then `FromJson` | `transformFromJson()` |

Namespaces show where code belongs: `rtk` for the generic engine, `rtype` for
the game, `rtype::protocol` for the wire format, `rtype::client` and
`rtype::server` for each program.

## Language rules

- C++20. Prefer concepts to constrain templates, as in rtk::Game and
  rtk::Scene.
- Prefer references to pointers. Use pointers only for optional values, such as
  the result of `get_components<T>().get(entity)`, and always check them.
- No owning raw pointers: use values, `std::unique_ptr` or containers.
- Mark systems and leaf classes `final`.
- Mark functions that only read state `const`, and functions whose result must
  be used `[[nodiscard]]`.
- No exceptions on the network path: decoding functions return `bool` and leave
  their output untouched on failure.
- No `using namespace` in headers.

## Engine rules

- **Components hold data only.** No methods except trivial helpers. Each
  component comes with a `xxxFromJson()` builder.
- **Systems hold behavior only.** They read and write components and publish
  events. They do not call each other.
- **Time is `dt`.** Every speed and timer is multiplied by the elapsed time of
  the frame.
- **The engine stays generic.** Nothing under `lib/` may include code from
  `src/`, the graphical library or game assets.
- **Destroy through Dead or `kill_entity()`.** Never erase an entity while
  another system may iterate over it.

## Network rules

- Every value read from a datagram is validated before use: size, range and
  identifiers.
- Payload structs are made only of single bytes and rtype::protocol::BigEndian
  fields, and have a `static_assert` on their size.
- Only the network thread touches a socket. Data crosses threads through mutex
  protected queues.
- The client never decides gameplay outcomes. It sends intentions and applies
  what the server sends.

## Documentation comments

Every public class, struct, function, method, enum value and public data member
has a Doxygen comment. Private members used only by the implementation are not
documented.

- Use `/** ... */` blocks with `@brief`, one short sentence.
- Add one `@param` per parameter and a `@return` when there is a value, each
  one short sentence.
- Use `///<` after a field or an enum value for a one line description.
- Describe each network payload with a table giving the offset, size and
  meaning of every field.
- Do not refer to other documents or files in comments: a comment must be
  understandable on its own.
- Put every declaration in the API reference group of its folder, so that it
  shows up in the right place of the website. Open the group right after each
  `namespace ... {` line and close it before the matching `}`. Declarations
  outside any namespace are wrapped the same way, right after
  `#pragma once`.

| Folder | Group |
| ------ | ----- |
| `lib/GameEngine` | `api_engine` |
| `lib/Network` | `api_network` |
| `src/Common/Protocol` | `api_protocol` |
| `src/Common` (other files) | `api_gameplay` |
| `src/Client` | `api_client` |
| `src/Server` | `api_server` |

```cpp
namespace rtk::systems {
/**
 * @addtogroup api_engine
 * @{
 */

// declarations

/** @} */
} // namespace rtk::systems
```

```cpp
/**
 * @brief Sends a Snapshot message.
 * @param connectionId Destination connection.
 * @param payload Message payload.
 * @return False if the snapshot has too many states or cannot be encoded.
 */
bool sendSnapshot(std::uint32_t connectionId,
                  const protocol::SnapshotPayload &payload);
```

## Files and folders

- Generic engine code goes in `lib/GameEngine/include`, by kind: `components`,
  `systems`, `events`.
- Network library code goes in `lib/Network/src`.
- Code used by both programs goes in `src/Common`.
- One main class per file, named after the class.
- Entity data goes in `assets/descriptionEntity` as JSON, never in C++.
