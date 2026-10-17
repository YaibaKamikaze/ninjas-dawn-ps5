// Ninja Gaiden II for PS5 - the DualSense for the menus (ps5_pad.h).

#include "ps5/ps5_pad.h"

#include <cstring>

extern "C" {
int sceUserServiceInitialize(void* params);
int sceUserServiceGetInitialUser(int32_t* user_id);
int scePadInit(void);
int scePadOpen(int32_t user_id, int32_t type, int32_t index, const void* param);
int scePadGetHandle(int32_t user_id, int32_t type, int32_t index);
int scePadReadState(int32_t handle, void* data);
}

namespace ng2ps5 {

namespace {

// ScePadData's first bytes, as verified on hardware by ps5-plume-triangle and
// the SDK's input driver; the rest of the structure is not needed here.
struct ScePadData {
  uint32_t buttons;
  uint8_t lx, ly, rx, ry, l2, r2;
  uint8_t padding[2];
  uint8_t rest[240];
};

constexpr uint32_t kSceOptions = 0x00000008u, kSceUp = 0x00000010u, kSceRight = 0x00000020u,
                   kSceDown = 0x00000040u, kSceLeft = 0x00000080u, kSceL2 = 0x00000100u,
                   kSceR2 = 0x00000200u, kSceL1 = 0x00000400u, kSceR1 = 0x00000800u,
                   kSceTriangle = 0x00001000u, kSceCircle = 0x00002000u, kSceCross = 0x00004000u,
                   kSceSquare = 0x00008000u, kSceTouchpad = 0x00100000u;

// SCE_PAD_ERROR_ALREADY_OPENED: the SDK's input driver (or an earlier menu)
// has the pad; scePadGetHandle hands back the same handle.
constexpr int kAlreadyOpened = int(0x80920004u);

}  // namespace

PadState Pad::Read() {
  PadState out;
  if (handle_ < 0) {
    sceUserServiceInitialize(nullptr);  // (already initialized is fine)
    scePadInit();
    int32_t user = -1;
    if (sceUserServiceGetInitialUser(&user) != 0 || user < 0)
      return out;
    handle_ = scePadOpen(user, 0, 0, nullptr);
    if (handle_ == kAlreadyOpened || handle_ < 0)
      handle_ = scePadGetHandle(user, 0, 0);
    if (handle_ < 0)
      return out;
  }
  ScePadData d;
  std::memset(&d, 0, sizeof(d));
  if (scePadReadState(handle_, &d) != 0)
    return out;
  out.connected = true;

  // The left stick moves like the d-pad, past half deflection.
  uint32_t b = 0;
  if ((d.buttons & kSceUp) || d.ly < 64) b |= kPadUp;
  if ((d.buttons & kSceDown) || d.ly > 192) b |= kPadDown;
  if ((d.buttons & kSceLeft) || d.lx < 64) b |= kPadLeft;
  if ((d.buttons & kSceRight) || d.lx > 192) b |= kPadRight;
  if (d.buttons & kSceCross) b |= kPadCross;
  if (d.buttons & kSceCircle) b |= kPadCircle;
  if (d.buttons & kSceSquare) b |= kPadSquare;
  if (d.buttons & kSceTriangle) b |= kPadTriangle;
  if (d.buttons & kSceL1) b |= kPadL1;
  if (d.buttons & kSceR1) b |= kPadR1;
  if (d.buttons & kSceL2) b |= kPadL2;
  if (d.buttons & kSceR2) b |= kPadR2;
  if (d.buttons & kSceOptions) b |= kPadOptions;
  if (d.buttons & kSceTouchpad) b |= kPadTouchpad;

  const auto now = std::chrono::steady_clock::now();
  out.held = b;
  out.pressed = b & ~last_;
  out.released = last_ & ~b;
  for (int i = 0; i < 16; ++i) {
    if (out.pressed & (1u << i))
      down_since_[i] = now;
  }
  // Held directions repeat: after 0.4 s, then every 0.09 s - quick enough to
  // run down a long list, slow enough to stop on the row wanted.
  constexpr uint32_t kDirs = kPadUp | kPadDown | kPadLeft | kPadRight;
  if (b & kDirs) {
    if ((b & kDirs) != (last_ & kDirs)) {
      repeat_at_ = now + std::chrono::milliseconds(400);
    } else if (now >= repeat_at_) {
      out.pressed |= b & kDirs;
      repeat_at_ = now + std::chrono::milliseconds(90);
    }
  }
  last_ = b;
  if (first_) {
    first_ = false;
    out.pressed = 0;
    out.released = 0;
  }
  return out;
}

float Pad::HeldFor(uint32_t button) const {
  if (!(last_ & button))
    return 0.0f;
  for (int i = 0; i < 16; ++i) {
    if (button == (1u << i))
      return std::chrono::duration<float>(std::chrono::steady_clock::now() - down_since_[i]).count();
  }
  return 0.0f;
}

}  // namespace ng2ps5
