#include "chat/ChatBox.hpp"

#include <algorithm>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "Protocol/ChatPayload.hpp"
#include "components/Transform.hpp"

namespace rtype::client {

namespace {

constexpr float PANEL_X = 870.0f;
constexpr float PANEL_WIDTH = 400.0f;
constexpr float PANEL_BOTTOM = 712.0f;
constexpr float PADDING = 8.0f;
constexpr float LINE_HEIGHT = 22.0f;
constexpr float TEXT_SCALE = 0.4f;
constexpr float LABEL_GAP = 8.0f;
constexpr std::size_t MAX_VISIBLE_LINES = 8;
constexpr std::size_t MAX_HISTORY = 50;
/// Seconds a message stays visible while the chat is closed in game.
constexpr float MESSAGE_LIFETIME = 10.0f;
constexpr float BACKSPACE_REPEAT_DELAY = 0.4f;
constexpr float BACKSPACE_REPEAT_INTERVAL = 0.05f;
constexpr std::int32_t BACKGROUND_LAYER = 110;
constexpr std::int32_t TEXT_LAYER = 111;

constexpr rtk::ecs::concepts::ColorRGBA8 WHITE{255, 255, 255, 255};
constexpr rtk::ecs::concepts::ColorRGBA8 GREY{150, 150, 170, 255};
constexpr rtk::ecs::concepts::ColorRGBA8 LOCAL_COLOR{255, 220, 90, 255};
constexpr rtk::ecs::concepts::ColorRGBA8 OTHER_COLOR{0, 220, 255, 255};
constexpr rtk::ecs::concepts::ColorRGBA8 SERVER_COLOR{255, 110, 80, 255};
constexpr rtk::ecs::concepts::ColorRGBA8 OPEN_BACKGROUND{10, 12, 24, 190};
constexpr rtk::ecs::concepts::ColorRGBA8 LOBBY_BACKGROUND{10, 12, 24, 120};
constexpr rtk::ecs::concepts::ColorRGBA8 INPUT_BACKGROUND{40, 45, 80, 220};

constexpr rtk::ecs::concepts::Key TEXT_KEYS[] = {
    rtk::ecs::concepts::Key::A,         rtk::ecs::concepts::Key::B,
    rtk::ecs::concepts::Key::C,         rtk::ecs::concepts::Key::D,
    rtk::ecs::concepts::Key::E,         rtk::ecs::concepts::Key::F,
    rtk::ecs::concepts::Key::G,         rtk::ecs::concepts::Key::H,
    rtk::ecs::concepts::Key::I,         rtk::ecs::concepts::Key::J,
    rtk::ecs::concepts::Key::K,         rtk::ecs::concepts::Key::L,
    rtk::ecs::concepts::Key::M,         rtk::ecs::concepts::Key::N,
    rtk::ecs::concepts::Key::O,         rtk::ecs::concepts::Key::P,
    rtk::ecs::concepts::Key::Q,         rtk::ecs::concepts::Key::R,
    rtk::ecs::concepts::Key::S,         rtk::ecs::concepts::Key::T,
    rtk::ecs::concepts::Key::U,         rtk::ecs::concepts::Key::V,
    rtk::ecs::concepts::Key::W,         rtk::ecs::concepts::Key::X,
    rtk::ecs::concepts::Key::Y,         rtk::ecs::concepts::Key::Z,
    rtk::ecs::concepts::Key::Num0,      rtk::ecs::concepts::Key::Num1,
    rtk::ecs::concepts::Key::Num2,      rtk::ecs::concepts::Key::Num3,
    rtk::ecs::concepts::Key::Num4,      rtk::ecs::concepts::Key::Num5,
    rtk::ecs::concepts::Key::Num6,      rtk::ecs::concepts::Key::Num7,
    rtk::ecs::concepts::Key::Num8,      rtk::ecs::concepts::Key::Num9,
    rtk::ecs::concepts::Key::Numpad0,   rtk::ecs::concepts::Key::Numpad1,
    rtk::ecs::concepts::Key::Numpad2,   rtk::ecs::concepts::Key::Numpad3,
    rtk::ecs::concepts::Key::Numpad4,   rtk::ecs::concepts::Key::Numpad5,
    rtk::ecs::concepts::Key::Numpad6,   rtk::ecs::concepts::Key::Numpad7,
    rtk::ecs::concepts::Key::Numpad8,   rtk::ecs::concepts::Key::Numpad9,
    rtk::ecs::concepts::Key::Space,     rtk::ecs::concepts::Key::Comma,
    rtk::ecs::concepts::Key::Period,    rtk::ecs::concepts::Key::Slash,
    rtk::ecs::concepts::Key::Semicolon, rtk::ecs::concepts::Key::Hyphen,
};

constexpr rtk::ecs::concepts::Key CONTROL_KEYS[] = {
    rtk::ecs::concepts::Key::Enter,     rtk::ecs::concepts::Key::Escape,
    rtk::ecs::concepts::Key::Backspace, rtk::ecs::concepts::Key::LShift,
    rtk::ecs::concepts::Key::RShift,
};

std::string labelOf(std::uint32_t senderId) {
  if (senderId == protocol::SERVER_SENDER_ID)
    return "SERVER";
  return "P" + std::to_string(senderId);
}

} // namespace

ChatBox::ChatBox(rtk::RenderWindow &window, RtkWindowAdapter &keys)
    : _window(window), _keys(keys) {}

void ChatBox::setFont(const rtk::Font &font) { _font = font; }

void ChatBox::addMessage(std::uint32_t senderId, const std::string &text,
                         bool fromLocalPlayer) {
  Message message;
  const std::string label = labelOf(senderId);
  const float labelWidth = measure(label, TEXT_SCALE);
  const float textWidth = PANEL_WIDTH - 2.0f * PADDING - labelWidth - LABEL_GAP;

  const auto parts = wrap(text, textWidth);
  for (std::size_t i = 0; i < parts.size(); ++i) {
    message.lines.push_back({i == 0 ? label : std::string{}, parts[i],
                             labelWidth + LABEL_GAP, fromLocalPlayer,
                             senderId == protocol::SERVER_SENDER_ID});
  }
  if (message.lines.empty())
    return;

  _history.push_back(std::move(message));
  while (_history.size() > MAX_HISTORY)
    _history.pop_front();
}

void ChatBox::clearHistory() {
  _history.clear();
  _input.clear();
  if (_open)
    close();
}

bool ChatBox::capturesKeyboard() const { return _open || _releaseLock; }

bool ChatBox::isDown(Key key) const {
  return _keyDown[static_cast<std::size_t>(key)];
}

bool ChatBox::wasPressed(Key key) const {
  const auto index = static_cast<std::size_t>(key);
  return _keyDown[index] && !_previousKeyDown[index];
}

void ChatBox::open() {
  _open = true;
  _input.clear();
  _backspaceHeld = 0.0f;
  _backspaceRepeat = 0.0f;
}

void ChatBox::close() {
  _open = false;
  _input.clear();
  _releaseLock = true;
}

std::optional<std::string> ChatBox::update(rtk::ecs::Registry &registry,
                                           float dt, bool available,
                                           bool keepHistoryVisible) {
  std::optional<std::string> sent;

  _previousKeyDown = _keyDown;
  for (const auto key : TEXT_KEYS)
    _keyDown[static_cast<std::size_t>(key)] = _keys.isKeyPressed(key);
  for (const auto key : CONTROL_KEYS)
    _keyDown[static_cast<std::size_t>(key)] = _keys.isKeyPressed(key);

  for (auto &message : _history)
    message.age += dt;

  if (!available) {
    if (_open)
      close();
  } else {
    readKeys(dt, sent);
  }

  if (_releaseLock && !isDown(Key::Enter) && !isDown(Key::Escape))
    _releaseLock = false;

  const std::string signature = buildSignature(available, keepHistoryVisible);
  if (signature != _signature) {
    _signature = signature;
    if (available)
      rebuild(registry, keepHistoryVisible);
    else
      destroyEntities(registry);
  }
  return sent;
}

void ChatBox::readKeys(float dt, std::optional<std::string> &sent) {
  if (!_open) {
    if (!_releaseLock && wasPressed(Key::Enter))
      open();
    return;
  }

  if (wasPressed(Key::Escape)) {
    close();
    return;
  }
  if (wasPressed(Key::Enter)) {
    std::string text = _input;
    while (!text.empty() && text.back() == ' ')
      text.pop_back();
    const auto first = text.find_first_not_of(' ');
    text = first == std::string::npos ? std::string{} : text.substr(first);
    if (protocol::isValidChatText(text))
      sent = std::move(text);
    close();
    return;
  }

  if (isDown(Key::Backspace)) {
    bool erase = wasPressed(Key::Backspace);
    _backspaceHeld += dt;
    if (!erase && _backspaceHeld >= BACKSPACE_REPEAT_DELAY) {
      _backspaceRepeat += dt;
      if (_backspaceRepeat >= BACKSPACE_REPEAT_INTERVAL) {
        _backspaceRepeat = 0.0f;
        erase = true;
      }
    }
    if (erase && !_input.empty())
      _input.pop_back();
  } else {
    _backspaceHeld = 0.0f;
    _backspaceRepeat = 0.0f;
  }

  const bool shift = isDown(Key::LShift) || isDown(Key::RShift);
  for (const auto key : TEXT_KEYS) {
    if (wasPressed(key))
      typeKey(key, shift);
  }
}

void ChatBox::typeKey(Key key, bool shift) {
  if (_input.size() >= protocol::MAX_CHAT_MESSAGE_LENGTH)
    return;

  const auto value = static_cast<std::uint16_t>(key);
  char c = '\0';

  if (key >= Key::A && key <= Key::Z) {
    const char offset =
        static_cast<char>(value - static_cast<std::uint16_t>(Key::A));
    c = static_cast<char>((shift ? 'A' : 'a') + offset);
  } else if (key >= Key::Num0 && key <= Key::Num9) {
    if (shift && key == Key::Num1)
      c = '!';
    else if (!shift)
      c = static_cast<char>('0' +
                            (value - static_cast<std::uint16_t>(Key::Num0)));
  } else if (key >= Key::Numpad0 && key <= Key::Numpad9) {
    c = static_cast<char>('0' +
                          (value - static_cast<std::uint16_t>(Key::Numpad0)));
  } else {
    switch (key) {
    case Key::Space:
      c = ' ';
      break;
    case Key::Comma:
      c = shift ? '\0' : ',';
      break;
    case Key::Period:
      c = shift ? '\0' : '.';
      break;
    case Key::Slash:
      c = shift ? '?' : '/';
      break;
    case Key::Semicolon:
      c = shift ? '\0' : ';';
      break;
    case Key::Hyphen:
      c = shift ? '_' : '\0';
      break;
    default:
      break;
    }
  }

  if (c != '\0' && protocol::isChatCharacter(c))
    _input.push_back(c);
}

float ChatBox::measure(const std::string &text, float scale) const {
  if (!_font.isValid() || text.empty())
    return 0.0f;
  const rtk::Text glyphs = _window.createText(text, _font, {0.0f, 0.0f});
  float width = 0.0f;
  for (const auto &glyph : glyphs.getSprites())
    width = std::max(width, glyph.position.x + glyph.size.x * glyph.scale.x);
  if (text.back() == ' ')
    width += measure("n", 1.0f);
  return width * scale;
}

std::vector<std::string> ChatBox::wrap(const std::string &text,
                                       float width) const {
  std::vector<std::string> lines;
  std::string current;

  const auto pushWord = [&](std::string word) {
    while (!word.empty()) {
      const std::string candidate =
          current.empty() ? word : current + " " + word;
      if (measure(candidate, TEXT_SCALE) <= width) {
        current = candidate;
        return;
      }
      if (!current.empty()) {
        lines.push_back(std::move(current));
        current.clear();
        continue;
      }
      std::size_t length = word.size();
      while (length > 1 && measure(word.substr(0, length), TEXT_SCALE) > width)
        --length;
      lines.push_back(word.substr(0, length));
      word.erase(0, length);
    }
  };

  std::size_t start = 0;
  while (start < text.size()) {
    const std::size_t end = text.find(' ', start);
    const std::size_t stop = end == std::string::npos ? text.size() : end;
    if (stop > start)
      pushWord(text.substr(start, stop - start));
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  if (!current.empty())
    lines.push_back(std::move(current));
  return lines;
}

std::string ChatBox::buildSignature(bool available,
                                    bool keepHistoryVisible) const {
  if (!available)
    return "hidden";

  std::size_t visibleMessages = 0;
  for (const auto &message : _history) {
    if (_open || keepHistoryVisible || message.age < MESSAGE_LIFETIME)
      ++visibleMessages;
  }

  std::string signature = _open ? "open|" : "closed|";
  signature += keepHistoryVisible ? "lobby|" : "game|";
  signature += std::to_string(_history.size()) + "|";
  signature += std::to_string(visibleMessages) + "|";
  signature += _input;
  return signature;
}

void ChatBox::rebuild(rtk::ecs::Registry &registry, bool keepHistoryVisible) {
  destroyEntities(registry);
  if (!_font.isValid())
    return;

  std::vector<const Line *> lines;
  for (const auto &message : _history) {
    if (!_open && !keepHistoryVisible && message.age >= MESSAGE_LIFETIME)
      continue;
    for (const auto &line : message.lines)
      lines.push_back(&line);
  }
  if (lines.size() > MAX_VISIBLE_LINES)
    lines.erase(lines.begin(),
                lines.end() - static_cast<std::ptrdiff_t>(MAX_VISIBLE_LINES));

  const bool showInputLine = _open || keepHistoryVisible;
  if (lines.empty() && !showInputLine)
    return;

  const float inputY = PANEL_BOTTOM - PADDING - LINE_HEIGHT;
  const float historyBottom =
      showInputLine ? inputY - 4.0f : inputY + LINE_HEIGHT;
  const float historyTop =
      historyBottom - static_cast<float>(lines.size()) * LINE_HEIGHT;

  if (_open || keepHistoryVisible) {
    const float panelTop = PANEL_BOTTOM - PADDING * 2.0f - LINE_HEIGHT -
                           static_cast<float>(MAX_VISIBLE_LINES) * LINE_HEIGHT -
                           4.0f;
    addRect(registry, PANEL_X, panelTop, PANEL_WIDTH, PANEL_BOTTOM - panelTop,
            _open ? OPEN_BACKGROUND : LOBBY_BACKGROUND);
  }

  float y = historyTop;
  for (const auto *line : lines) {
    const Color labelColor = line->fromServer        ? SERVER_COLOR
                             : line->fromLocalPlayer ? LOCAL_COLOR
                                                     : OTHER_COLOR;
    addText(registry, line->label, PANEL_X + PADDING, y, labelColor);
    addText(registry, line->text, PANEL_X + PADDING + line->indent, y, WHITE);
    y += LINE_HEIGHT;
  }

  if (!showInputLine)
    return;
  if (_open) {
    addRect(registry, PANEL_X + 2.0f, inputY - 2.0f, PANEL_WIDTH - 4.0f,
            LINE_HEIGHT + 2.0f, INPUT_BACKGROUND);
    std::string shown = _input + "_";
    // Keep the end of a long input visible.
    while (shown.size() > 1 &&
           measure(shown, TEXT_SCALE) > PANEL_WIDTH - 2.0f * PADDING)
      shown.erase(0, 1);
    addText(registry, shown, PANEL_X + PADDING, inputY, WHITE);
  } else {
    addText(registry, "ENTER TO CHAT", PANEL_X + PADDING, inputY, GREY);
  }
}

void ChatBox::addText(rtk::ecs::Registry &registry, const std::string &content,
                      float x, float y, const Color &color) {
  if (content.empty())
    return;

  const rtk::Text text =
      _window.createText(content, _font, {x, y},
                         rtk::ColorRGBA8{color.r, color.g, color.b, color.a});

  for (const auto &glyph : text.getSprites()) {
    const auto entity = registry.spawn_entity();

    rtk::ecs::concepts::SpriteData sprite{};
    sprite.textureId = glyph.textureId;
    sprite.textureRect.x = glyph.textureRect.left;
    sprite.textureRect.y = glyph.textureRect.top;
    sprite.textureRect.width = glyph.textureRect.width;
    sprite.textureRect.height = glyph.textureRect.height;
    sprite.color = {glyph.color.r, glyph.color.g, glyph.color.b, glyph.color.a};
    sprite.size = {glyph.size.x, glyph.size.y};
    sprite.origin = {glyph.origin.x, glyph.origin.y};
    sprite.scale = {glyph.scale.x * TEXT_SCALE, glyph.scale.y * TEXT_SCALE};
    sprite.layer = TEXT_LAYER;

    component::Transform transform{};
    transform.position.x = x + (glyph.position.x - x) * TEXT_SCALE;
    transform.position.y = y + (glyph.position.y - y) * TEXT_SCALE;
    transform.scale = sprite.scale;
    transform.rotation = 0.0f;

    registry.get_components<component::Transform>().insert_at(entity,
                                                              transform);
    registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                        sprite);
    _entities.push_back(entity);
  }
}

void ChatBox::addRect(rtk::ecs::Registry &registry, float x, float y,
                      float width, float height, const Color &color) {
  if (!_whiteTexture.isValid()) {
    static const std::uint8_t whitePixel[4] = {255, 255, 255, 255};
    _whiteTexture = _window.loadTextureFromMemory(
        std::span<const std::uint8_t>(whitePixel, 4), 1, 1);
  }
  if (!_whiteTexture.isValid())
    return;

  const auto entity = registry.spawn_entity();

  component::Transform transform{};
  transform.position = {x, y};
  transform.scale = {1.0f, 1.0f};
  transform.rotation = 0.0f;

  rtk::ecs::concepts::SpriteData sprite{};
  sprite.textureId = _whiteTexture.getHandle();
  sprite.size = {width, height};
  sprite.origin = {0.0f, 0.0f};
  sprite.textureRect = {0, 0, 1, 1};
  sprite.color = color;
  sprite.scale = {1.0f, 1.0f};
  sprite.layer = BACKGROUND_LAYER;

  registry.get_components<component::Transform>().insert_at(entity, transform);
  registry.get_components<rtk::ecs::concepts::SpriteData>().insert_at(entity,
                                                                      sprite);
  _entities.push_back(entity);
}

void ChatBox::destroyEntities(rtk::ecs::Registry &registry) {
  for (const auto entity : _entities)
    registry.kill_entity(entity);
  _entities.clear();
}

} // namespace rtype::client
