#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include <Registry.hpp>
#include <font/font.hpp>
#include <graphical/renderWindow.hpp>
#include <texture/texture.hpp>

#include "rtkWindowAdapter.hpp"
#include "systems/RenderConcept.hpp"

namespace rtype::client {
/**
 * @addtogroup api_client
 * @{
 */

/**
 * @brief Text chat of the current room, drawn in the lower right corner.
 *
 */
class ChatBox {
public:
  /**
   * @brief Creates a closed chat with an empty history.
   * @param window Window used to build the text glyphs.
   * @param keys Adapter queried for the pressed keys.
   */
  ChatBox(rtk::RenderWindow &window, RtkWindowAdapter &keys);

  /**
   * @brief Sets the font of the chat.
   * @param font Font used to draw the messages.
   */
  void setFont(const rtk::Font &font);

  /**
   * @brief Appends a message to the history.
   * @param senderId Connection ID of the sender.
   * @param text Message content.
   * @param fromLocalPlayer True if the local player sent the message.
   */
  void addMessage(std::uint32_t senderId, const std::string &text,
                  bool fromLocalPlayer);

  /**
   * @brief Removes every message and closes the input line.
   */
  void clearHistory();

  /**
   * @brief Reads the keyboard and refreshes the drawn chat.
   *
   * @param registry Registry holding the chat entities.
   * @param dt Elapsed time since the previous frame, in seconds.
   * @param available False hides the chat and closes the input line.
   * @param keepHistoryVisible True to show the history even when closed,
   * false to only show the recent messages while closed.
   * @return The message to send, when Enter validated a non-empty input.
   */
  std::optional<std::string> update(rtk::ecs::Registry &registry, float dt,
                                    bool available, bool keepHistoryVisible);

  /**
   * @brief Tells whether the chat captures the keyboard.
   * @return True while the input line is open or its closing keys are held.
   */
  [[nodiscard]] bool capturesKeyboard() const;

  /**
   * @brief Destroys the drawn entities.
   * @param registry Registry holding the chat entities.
   */
  void destroyEntities(rtk::ecs::Registry &registry);

private:
  using Key = rtk::ecs::concepts::Key;
  using Color = rtk::ecs::concepts::ColorRGBA8;

  /// One drawn line of a message, a long message is wrapped on several.
  struct Line {
    std::string label;   ///< Sender label, empty on continuation lines.
    std::string text;    ///< Part of the message on this line.
    float indent = 0.0f; ///< Offset of the text from the panel border.
    bool fromLocalPlayer = false;
    bool fromServer = false; ///< Notice written by the server.
  };

  /// A message of the history, already wrapped.
  struct Message {
    std::vector<Line> lines;
    float age = 0.0f; ///< Seconds since the message was received.
  };

  static constexpr std::size_t KEY_COUNT =
      static_cast<std::size_t>(Key::F15) + 1;

  void readKeys(float dt, std::optional<std::string> &sent);
  void typeKey(Key key, bool shift);
  void open();
  void close();
  [[nodiscard]] bool isDown(Key key) const;
  [[nodiscard]] bool wasPressed(Key key) const;

  [[nodiscard]] float measure(const std::string &text, float scale) const;
  [[nodiscard]] std::vector<std::string> wrap(const std::string &text,
                                              float width) const;
  [[nodiscard]] std::string buildSignature(bool available,
                                           bool keepHistoryVisible) const;
  void rebuild(rtk::ecs::Registry &registry, bool keepHistoryVisible);
  void addText(rtk::ecs::Registry &registry, const std::string &content,
               float x, float y, const Color &color);
  void addRect(rtk::ecs::Registry &registry, float x, float y, float width,
               float height, const Color &color);

  rtk::RenderWindow &_window;
  RtkWindowAdapter &_keys;
  rtk::Font _font;
  rtk::Texture _whiteTexture;

  std::deque<Message> _history;
  std::string _input;
  bool _open = false;
  bool _releaseLock = false;
  std::array<bool, KEY_COUNT> _keyDown{};
  std::array<bool, KEY_COUNT> _previousKeyDown{};
  float _backspaceHeld = 0.0f;
  float _backspaceRepeat = 0.0f;

  std::vector<std::size_t> _entities;
  std::string _signature;
};

/** @} */
} // namespace rtype::client
