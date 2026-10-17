// Ninja Gaiden II for PS5 - the in-game overlay (ps5_overlay.h).

#include "ps5/overlay/ps5_overlay.h"

#include <algorithm>
#include <atomic>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/logging.h>

namespace ng2ps5::overlay {

namespace {

std::atomic<bool> g_open{false};
// How long the touchpad is held before the menu opens. Long enough that the
// game's own BACK presses (a tap) never open it, short enough not to feel
// like waiting.
constexpr float kHoldSeconds = 0.6f;
const char* const kActions[] = {"Resume", "Return to hub", "Quit to home screen", "Capture frame report"};
constexpr int kActionCount = 4;

}  // namespace

Overlay::Overlay(rex::ui::ImGuiDrawer* drawer, Ng2Settings* settings, Callbacks callbacks)
    : ImGuiDialog(drawer), settings_(settings), callbacks_(std::move(callbacks)) {
  std::vector<const hub::Setting*> rows;
  for (const hub::Setting& s : hub::Settings())
    rows.push_back(&s);
  // The live ones first: they are what this menu is for.
  std::stable_sort(rows.begin(), rows.end(),
                   [](const hub::Setting* a, const hub::Setting* b) { return a->live && !b->live; });
  list_.SetRows(std::move(rows));
}

Overlay::~Overlay() { g_open = false; }

bool Overlay::IsOpen() { return g_open.load(std::memory_order_acquire); }

void Overlay::Open() {
  open_ = true;
  g_open = true;
  pane_ = 0;
  action_ = 0;
  confirm_quit_ = false;
  hub::InitSounds("/app0/hub");  // (once; the hub may not have run in a reload)
  hub::PlaySound(hub::Sound::kOpen);
  REXLOG_INFO("PS5 overlay: opened");
}

void Overlay::CloseMenu() {
  open_ = false;
  g_open = false;
  hub::PlaySound(hub::Sound::kBack);
}

void Overlay::OnDraw(ImGuiIO& io) {
  if (!enabled_)
    return;
  const PadState pad = pad_.Read();
  const float dt = std::min(io.DeltaTime, 0.1f);

  if (!(pad.held & kPadTouchpad))
    hold_spent_ = false;
  if (!open_) {
    if (!hold_spent_ && pad_.HeldFor(kPadTouchpad) >= kHoldSeconds) {
      hold_spent_ = true;
      Open();
    }
    hub::Ease(in_, 0.0f, dt, 16.0f);
    if (in_ <= 0.001f)
      return;
  } else {
    hub::Ease(in_, 1.0f, dt, 16.0f);
  }

  if (open_) {
    if (confirm_quit_) {
      if (pad.pressed & kPadCross) {
        hub::PlaySound(hub::Sound::kSelect);
        if (callbacks_.quit_home)
          callbacks_.quit_home();
        return;
      }
      if (pad.pressed & kPadCircle) {
        confirm_quit_ = false;
        hub::PlaySound(hub::Sound::kBack);
      }
    } else if ((pad.pressed & kPadCircle) || ((pad.pressed & kPadTouchpad) && !hold_spent_)) {
      CloseMenu();
    } else if (pad.pressed & (kPadL1 | kPadR1)) {
      pane_ ^= 1;
      hub::PlaySound(hub::Sound::kTab);
    } else if (pane_ == 0) {
      if (list_.Update(pad, *settings_, true)) {
        settings_->Save();
        if (callbacks_.apply_live)
          callbacks_.apply_live();
      }
    } else {
      if (pad.pressed & kPadUp)
        action_ = (action_ + kActionCount - 1) % kActionCount, hub::PlaySound(hub::Sound::kMove);
      if (pad.pressed & kPadDown) action_ = (action_ + 1) % kActionCount, hub::PlaySound(hub::Sound::kMove);
      if (pad.pressed & kPadCross) {
        hub::PlaySound(hub::Sound::kSelect);
        if (action_ == 0) {
          CloseMenu();
        } else if (action_ == 1) {
          if (callbacks_.return_to_hub)
            callbacks_.return_to_hub();
          return;
        } else if (action_ == 2) {
          confirm_quit_ = true;
        } else {
          // Diagnostic: the GPU backend saves the next two frames' images to
          // logs/frame_reports (rex frame_report_now).
          rex::cvar::SetFlagByName("frame_report_now", "true");
          REXLOG_INFO("PS5 overlay: frame report requested");
          CloseMenu();
        }
      }
    }
  }

  // The panel slides in from the right over a dimmed game.
  hub::Canvas c;
  c.Begin(ImGui::GetForegroundDrawList(), io.DisplaySize);
  c.alpha = in_;
  const hub::Fonts& f = hub::GetFonts();
  c.Rect(0, 0, 1920, 1080, IM_COL32(0, 0, 0, 140));
  const float x = 1920 - 980 * in_ + 40;
  c.Rect(x, 60, 900, 960, hub::colour::kPanel, 10);
  c.Frame(x, 60, 900, 960, IM_COL32(120, 20, 18, 160), 2, 10);
  c.Embossed(f.menu, 46, x + 40, 84, hub::colour::kIvory, "NINJA GAIDEN II", 0, 0);
  const char* panes[] = {"SETTINGS", "GAME"};
  float tx = x + 40;
  for (int p = 0; p < 2; ++p) {
    const float w = c.TextWidth(f.heading, 28, panes[p]);
    c.Text(f.heading, 28, tx, 160, p == pane_ ? hub::colour::kIvory : hub::colour::kIvoryDim, panes[p]);
    if (p == pane_)
      c.Rect(tx, 200, w, 4, hub::colour::kRed);
    tx += w + 40;
  }
  if (pane_ == 0) {
    list_.Draw(c, *settings_, x + 20, 220, 860, 700, true, dt);
  } else {
    for (int i = 0; i < kActionCount; ++i) {
      const float y = 250 + float(i) * 72;
      if (i == action_)
        c.Rect(x + 20, y - 10, 860, 62, IM_COL32(150, 18, 16, 120));
      c.Text(f.bold, 32, x + 48, y, i == action_ ? hub::colour::kIvory : hub::colour::kIvoryDim, kActions[i]);
    }
    c.Wrapped(f.body, 24, x + 48, 560, 800, hub::colour::kMuted,
              "Return to hub saves your settings and restarts into the hub; progress since your last save "
              "in the game is lost, as when quitting from the game's own menu.");
  }
  if (confirm_quit_) {
    c.Rect(x + 60, 420, 780, 220, IM_COL32(10, 10, 12, 245), 10);
    c.Text(f.bold, 32, x + 450, 460, hub::colour::kIvory, "Quit to the home screen?", 0.5f);
    c.Text(f.body, 24, x + 450, 510, hub::colour::kIvoryDim, "Unsaved progress is lost.", 0.5f);
    c.Hint(x + 800, 600, kPadCircle, "No");
    c.Hint(x + 640, 600, kPadCross, "Yes");
  }
  float right = x + 860;
  right = c.Hint(right, 990, kPadCircle, "Close");
  right = c.Hint(right, 990, kPadR1, "Pane");
}

}  // namespace ng2ps5::overlay
