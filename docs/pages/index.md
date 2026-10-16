# R-Type developer documentation {#mainpage}

R-Type is a networked, multiplayer remake of the classic horizontal shoot'em up.
Up to four players connect to an authoritative server and fight waves of Bydos
together. The game runs on a small game engine of our own, written in C++20.

This website is the developer documentation of the project. It explains how the
code is organized, how the parts talk to each other, why each technology was
chosen, and how to extend the game. The API reference generated from the source
code is available from the navigation tree on the left.

## Where to start

| If you want to | Read |
| -------------- | ---- |
| Build and run the game | @ref getting_started |
| Understand the big picture | @ref architecture |
| Learn how the engine works | @ref engine |
| Learn how the network layer works | @ref networking |
| Write a new client or a network tool | @ref protocol |
| Add content or features step by step | @ref tutorials |
| Understand our technical choices | @ref technical_study |
| Know how the project handles accessibility | @ref accessibility |
| Contribute to the project | @ref contributing and @ref coding_conventions |
| Browse the code reference | @ref api_libraries and @ref api_game |

## Project at a glance

| Item | Value |
| ---- | ----- |
| Language | C++20 |
| Build system | CMake 3.20 or newer |
| Package manager | vcpkg (manifest mode) |
| Programs | `r-type_server`, `r-type_client` |
| Networking | UDP, custom binary protocol, Asio |
| Rendering | rtk graphical, a Vulkan 2D sprite renderer |
| Architecture | Entity Component System, event bus, scene manager |
| License | GNU General Public License v3.0 |

## Searching this documentation

Use the search field at the top of the navigation bar to find a class, a
function, a message type or a guide. The navigation tree on the left lists the
guides first, then the **API reference**, organized as follows:

- **Libraries**
  - **Game engine**: everything under `lib/GameEngine`.
  - **Network library**: everything under `lib/Network`.
- **R-Type game**
  - **Protocol payloads**: message types and payload layouts.
  - **Shared gameplay**: components and systems used by both programs.
  - **Client** and **Server**: code of each program.

The flat lists of namespaces, classes and source files follow.

## Guides

- @subpage getting_started
- @subpage architecture
- @subpage engine
- @subpage networking
- @subpage protocol
- @subpage tutorials
- @subpage technical_study
- @subpage accessibility
- @subpage contributing
- @subpage coding_conventions
