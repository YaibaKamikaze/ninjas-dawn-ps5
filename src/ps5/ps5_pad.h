// Ninja Gaiden II for PS5 - the DualSense, as the hub and the in-game overlay
// read it (ps5_pad.cpp).
//
// The game reads the controller through the SDK's input driver; these menus
// read it themselves, with scePad, because the hub runs before the runtime
// exists and the overlay must see the touchpad held even while the game's
// input is switched off. Reading the pad's state does not consume it, so both
// can read the same pad.
#pragma once

#include <chrono>
#include <cstdint>

namespace ng2ps5 {

enum PadButton : uint32_t {
  kPadUp = 1u << 0,
  kPadDown = 1u << 1,
  kPadLeft = 1u << 2,
  kPadRight = 1u << 3,
  kPadCross = 1u << 4,
  kPadCircle = 1u << 5,
  kPadSquare = 1u << 6,
  kPadTriangle = 1u << 7,
  kPadL1 = 1u << 8,
  kPadR1 = 1u << 9,
  kPadOptions = 1u << 10,
  kPadTouchpad = 1u << 11,
  kPadL2 = 1u << 12,
  kPadR2 = 1u << 13,
};

struct PadState {
  uint32_t held = 0;     // down now
  uint32_t pressed = 0;  // went down this read (directions repeat while held)
  uint32_t released = 0;
  bool connected = false;
};

class Pad {
 public:
  // One read per frame. The first read after construction reports nothing as
  // pressed, so the button that started the app (or opened a menu) does not
  // also act inside it.
  PadState Read();
  // How long a button has been held without a break, in seconds (0 if up).
  float HeldFor(uint32_t button) const;

 private:
  int32_t handle_ = -1;
  uint32_t last_ = 0;
  bool first_ = true;
  std::chrono::steady_clock::time_point repeat_at_{};
  std::chrono::steady_clock::time_point down_since_[16]{};
};

}  // namespace ng2ps5
