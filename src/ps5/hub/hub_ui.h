// Ninja Gaiden II for PS5 - the hub and the in-game overlay: what they share.
//
// The hub is the screen the app opens on (docs/PS5_PORT.md 5.4): Ninja Gaiden II's
// own title menu is the model - misty grey sky, a field of standing katanas,
// the chrome logo with a red brush-stroke "II", and a centred column of
// embossed serif capitals where the focused one turns red. Nothing in it is
// game art: the backdrop and the logo are painted here, the faces are OFL
// fonts built into the app (ps5/assets/fonts).
//
// Everything is drawn with ImGui's draw lists on a virtual 1920x1080 canvas,
// scaled to whatever the display is, and driven by the DualSense only.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>

#include "ng2_settings.h"
#include "ps5/ps5_pad.h"

namespace ng2ps5::hub {

// --- Fonts (built in, loaded from ReXApp::OnConfigureFonts) ---------------

struct Fonts {
  ImFont* logo = nullptr;     // Cinzel Bold: NINJA GAIDEN
  ImFont* menu = nullptr;     // Cinzel Bold: the menu items, page titles
  ImFont* heading = nullptr;  // Cinzel SemiBold: tabs, section names
  ImFont* body = nullptr;     // Inter Regular: text
  ImFont* bold = nullptr;     // Inter SemiBold: labels, values
};
Fonts& GetFonts();
void ConfigureFonts(ImFontAtlas* atlas);

// --- Sounds the menus ask for (hub_sfx.cpp makes them) --------------------

enum class Sound { kMove, kSelect, kBack, kTab, kChange, kError, kStart, kOpen, kCount };
void PlaySound(Sound sound);
// Loads hub/sfx/<name>.wav replacements, synthesizes the rest. Once.
void InitSounds(const std::string& hub_folder);

// --- The canvas -------------------------------------------------------------

struct Canvas {
  ImDrawList* dl = nullptr;
  float s = 1, ox = 0, oy = 0;  // virtual -> screen
  float alpha = 1;              // a whole-layer fade (the hub leaving)

  // Fits 1920x1080 inside the display, centred (pillar/letterbox otherwise).
  void Begin(ImDrawList* list, const ImVec2& display);
  ImVec2 P(float x, float y) const { return ImVec2(ox + x * s, oy + y * s); }
  ImU32 C(ImU32 col) const;  // col with the layer alpha applied

  void Rect(float x, float y, float w, float h, ImU32 col, float rounding = 0) const;
  void Frame(float x, float y, float w, float h, ImU32 col, float thickness, float rounding = 0) const;
  void Gradient(float x, float y, float w, float h, ImU32 top, ImU32 bottom) const;
  // align: 0 left, 0.5 centre, 1 right. Returns the width drawn.
  float Text(ImFont* f, float size, float x, float y, ImU32 col, const std::string& t, float align = 0) const;
  float TextWidth(ImFont* f, float size, const std::string& t) const;
  // Word-wrapped; returns the height used.
  float Wrapped(ImFont* f, float size, float x, float y, float width, ImU32 col, const std::string& t,
                float line = 1.35f) const;
  // The embossed serif capitals of the title menu: dark outline, a lit top
  // edge, a shadowed bottom edge. `glow` (0..1) lays the focused red glow under it.
  void Embossed(ImFont* f, float size, float x, float y, ImU32 face, const std::string& t, float align,
                float glow) const;
  // A PlayStation button drawn from shapes: kPadCross, kPadCircle, kPadSquare,
  // kPadTriangle, kPadL1, kPadR1, kPadOptions, kPadTouchpad, or the d-pad
  // (kPadUp|kPadDown or kPadLeft|kPadRight).
  void Glyph(uint32_t button, float cx, float cy, float r) const;
  // A hint in the bottom bar, right-aligned at `right`; returns its left edge.
  float Hint(float right, float y, uint32_t button, const char* label) const;
};

// Eases `v` toward `target` at a rate that does not depend on the frame rate.
float Ease(float& v, float target, float dt, float speed = 14.0f);

// The colours of the title menu.
namespace colour {
constexpr ImU32 kIvory = IM_COL32(236, 230, 214, 255);
constexpr ImU32 kIvoryDim = IM_COL32(176, 170, 156, 255);
constexpr ImU32 kRed = IM_COL32(214, 32, 28, 255);
constexpr ImU32 kRedDeep = IM_COL32(140, 12, 14, 255);
constexpr ImU32 kInk = IM_COL32(14, 14, 16, 255);
constexpr ImU32 kPanel = IM_COL32(12, 12, 14, 200);
constexpr ImU32 kMuted = IM_COL32(150, 150, 150, 255);
constexpr ImU32 kGood = IM_COL32(150, 200, 140, 255);
constexpr ImU32 kWarn = IM_COL32(230, 170, 70, 255);
}  // namespace colour

// --- Settings, as rows (hub_settings.cpp) ---------------------------------

enum class Tab { kDisplay, kGraphics, kGame, kTextures, kPerformance, kDeveloper, kCount };
const char* TabName(Tab tab);

struct Setting {
  Tab tab;
  const char* label;
  const char* help;  // one line, shown under the list for the focused row
  std::vector<std::string> choices;
  std::function<int(const Ng2Settings&)> get;
  std::function<void(Ng2Settings&, int)> set;
  int def = 0;           // Square puts this back
  bool restart = false;  // applies at the next START GAME
  bool live = false;     // offered in the in-game overlay
  // Greyed out, with the reason, when this returns a non-empty string.
  std::function<std::string(const Ng2Settings&)> unavailable;
};
const std::vector<Setting>& Settings();

// The PS5's first-run values (resolution 1440p with FSI, 720p base, PlayStation prompts...),
// applied over the PC defaults the first time the app starts.
void ApplyPs5Defaults(Ng2Settings& s);
// The defaults round this build applies; UpdatePs5Defaults brings an older
// settings file up to it (first run: all of ApplyPs5Defaults).
constexpr int kPs5DefaultsVersion = 3;
void UpdatePs5Defaults(Ng2Settings& s);
// Every row back to its default, then the PS5's.
void ResetAll(Ng2Settings& s);

// A settings list: draws the rows of one tab (or the overlay's live rows) and
// handles left/right, Square and Triangle. Shared by the hub and the overlay so
// the same setting behaves the same in both.
class SettingsList {
 public:
  void SetRows(std::vector<const Setting*> rows) { rows_ = std::move(rows), sel_ = 0, scroll_ = 0; }
  // true if a setting changed (the caller saves and applies).
  bool Update(const PadState& pad, Ng2Settings& s, bool in_game);
  void Draw(const Canvas& c, const Ng2Settings& s, float x, float y, float w, float h, bool in_game,
            float dt);
  int Selected() const { return sel_; }
  const Setting* Current() const { return rows_.empty() ? nullptr : rows_[size_t(sel_)]; }
  bool ShowingHelp() const { return help_; }

 private:
  std::vector<const Setting*> rows_;
  int sel_ = 0;
  float scroll_ = 0, focus_y_ = -1, nudge_ = 0;
  bool help_ = false;
};

}  // namespace ng2ps5::hub
