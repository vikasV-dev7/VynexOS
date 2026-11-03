#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "vynexos/core/event_bus.hpp"

namespace vynexos::hal {

/// Event-bus topics published by input drivers. This is the single contract between
/// drivers (publishers) and consumers (e.g. the desktop shell).
inline constexpr const char* kTopicInputKey = "HAL_INPUT_KEY";
inline constexpr const char* kTopicInputMouse = "HAL_INPUT_MOUSE";
inline constexpr const char* kTopicInputPointer = "HAL_INPUT_POINTER";  ///< PointerSignalEvent payloads
inline constexpr const char* kTopicInputText = "HAL_INPUT_TEXT";        ///< TextInputEvent payloads

/// Pointer happenings that carry no position, published on kTopicInputPointer.
enum class PointerSignal : uint8_t {
    Left,   ///< the pointer left the host window: hover feedback should clear (a press in progress continues)
    Cancel  ///< the interaction was interrupted (e.g. the host window lost focus): abandon any press, drag or resize
};

struct PointerSignalEvent {
    PointerSignal signal;
};

/// A physical key going down (`pressed`, repeated while held) or up. Key codes are SDL keycodes: for keys that produce
/// an ASCII character it is that character (lower case), and the named keys below. Characters typed are delivered
/// separately as TextInputEvent (layout, shift and composition applied), so key events are for keys, not text.
struct KeyEvent {
    uint32_t key_code;
    bool pressed;
    KeyEvent(uint32_t k, bool p) : key_code(k), pressed(p) {}
};
inline constexpr uint32_t kKeyBackspace = 8;
inline constexpr uint32_t kKeyReturn = 13;
inline constexpr uint32_t kKeyEscape = 27;
inline constexpr uint32_t kKeyKeypadEnter = 0x40000058; // SDLK_KP_ENTER: the numeric keypad's Enter

/// Text produced by typing, UTF-8, published on kTopicInputText.
struct TextInputEvent {
    std::string text;
};

struct MouseEvent {
    int32_t x;
    int32_t y;
    uint8_t button_state;
    MouseEvent(int32_t _x, int32_t _y, uint8_t _b) : x(_x), y(_y), button_state(_b) {}
};

class IInputDriver {
public:
    virtual ~IInputDriver() = default;
    virtual void poll() = 0;
    virtual bool is_shutdown_requested() const = 0;
};

} // namespace vynexos::hal
