// Ninja Gaiden II for PS5 - the in-game overlay (ps5_overlay.cpp): the PC
// port's F10 menu, for the DualSense (docs/PS5_PORT.md 5.5).
//
// Opened by holding the touchpad button for 0.6 s; a short press stays the
// Xbox BACK button the game sees. While it is open the game gets no input.
// It holds every setting that can change mid-game, the others greyed and
// marked "restart", plus Return to hub and Quit to the home screen.
#pragma once

#include <functional>

#include <rex/ui/imgui_dialog.h>

#include "ng2_settings.h"
#include "ps5/hub/hub_ui.h"
#include "ps5/ps5_pad.h"

namespace ng2ps5::overlay {

struct Callbacks {
  std::function<void()> apply_live;     // a live setting changed (saved already)
  std::function<void()> return_to_hub;  // reload into the hub
  std::function<void()> quit_home;      // close the app through the shell
};

class Overlay final : public rex::ui::ImGuiDialog {
 public:
  Overlay(rex::ui::ImGuiDrawer* drawer, Ng2Settings* settings, Callbacks callbacks);
  ~Overlay() override;

  // For the input gate: the game's input is off while this is true.
  static bool IsOpen();
  // Off until the game is running: before that the hub has the controller.
  void SetEnabled(bool enabled) { enabled_ = enabled; }

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  void Open();
  void CloseMenu();

  Ng2Settings* settings_;
  Callbacks callbacks_;
  Pad pad_;
  hub::SettingsList list_;
  bool enabled_ = false;
  bool open_ = false;
  bool hold_spent_ = false;  // the hold that opened it must be released first
  int pane_ = 0;             // 0 settings, 1 game
  int action_ = 0;
  float in_ = 0;
  bool confirm_quit_ = false;
};

}  // namespace ng2ps5::overlay
