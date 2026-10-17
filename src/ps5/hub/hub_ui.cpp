// Ninja Gaiden II for PS5 - the hub's drawing, fonts and settings list
// (hub_ui.h).

#include "ps5/hub/hub_ui.h"

#include <algorithm>
#include <cmath>

namespace ng2ps5::hub {

// --- Fonts -------------------------------------------------------------------

// Built into the app so the hub works with nothing copied but the package:
// SIL Open Font License faces, licences beside them in ps5/assets/fonts.
#define NG2PS5_INCBIN(name, file)                                                         \
  __asm__(".section .rodata\n.global " #name "\n.balign 16\n" #name ":\n.incbin \"" file \
          "\"\n.global " #name "_end\n" #name "_end:\n.byte 0\n.previous\n");           \
  extern "C" const unsigned char name[];                                                \
  extern "C" const unsigned char name##_end[];
NG2PS5_INCBIN(ng2ps5_font_cinzel_bold, NG2_PS5_FONTS_DIR "/cinzel-700.ttf")
NG2PS5_INCBIN(ng2ps5_font_cinzel_semibold, NG2_PS5_FONTS_DIR "/cinzel-600.ttf")
NG2PS5_INCBIN(ng2ps5_font_inter, NG2_PS5_FONTS_DIR "/inter-400.ttf")
NG2PS5_INCBIN(ng2ps5_font_inter_semibold, NG2_PS5_FONTS_DIR "/inter-600.ttf")

Fonts& GetFonts() {
  static Fonts fonts;
  return fonts;
}

namespace {
ImFont* AddFont(ImFontAtlas* atlas, const unsigned char* begin, const unsigned char* end, float size) {
  ImFontConfig cfg;
  cfg.FontDataOwnedByAtlas = false;  // (built into the app, not the heap)
  cfg.OversampleH = 2;
  return atlas->AddFontFromMemoryTTF(const_cast<unsigned char*>(begin), int(end - begin), size, &cfg);
}
}  // namespace

void ConfigureFonts(ImFontAtlas* atlas) {
  // Rasterised for a 4K screen; the canvas draws them at whatever size the
  // display needs (ImGui 1.92 re-rasterises per size on demand).
  Fonts& f = GetFonts();
  f.logo = AddFont(atlas, ng2ps5_font_cinzel_bold, ng2ps5_font_cinzel_bold_end, 96.0f);
  f.menu = f.logo;
  f.heading = AddFont(atlas, ng2ps5_font_cinzel_semibold, ng2ps5_font_cinzel_semibold_end, 48.0f);
  f.body = AddFont(atlas, ng2ps5_font_inter, ng2ps5_font_inter_end, 40.0f);
  f.bold = AddFont(atlas, ng2ps5_font_inter_semibold, ng2ps5_font_inter_semibold_end, 40.0f);
}

// --- Canvas ------------------------------------------------------------------

void Canvas::Begin(ImDrawList* list, const ImVec2& display) {
  dl = list;
  s = std::min(display.x / 1920.0f, display.y / 1080.0f);
  ox = (display.x - 1920.0f * s) * 0.5f;
  oy = (display.y - 1080.0f * s) * 0.5f;
}

ImU32 Canvas::C(ImU32 col) const {
  if (alpha >= 1.0f)
    return col;
  const uint32_t a = uint32_t(float((col >> IM_COL32_A_SHIFT) & 0xFF) * std::clamp(alpha, 0.0f, 1.0f));
  return (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

void Canvas::Rect(float x, float y, float w, float h, ImU32 col, float rounding) const {
  dl->AddRectFilled(P(x, y), P(x + w, y + h), C(col), rounding * s);
}

void Canvas::Frame(float x, float y, float w, float h, ImU32 col, float thickness, float rounding) const {
  dl->AddRect(P(x, y), P(x + w, y + h), C(col), rounding * s, 0, thickness * s);
}

void Canvas::Gradient(float x, float y, float w, float h, ImU32 top, ImU32 bottom) const {
  dl->AddRectFilledMultiColor(P(x, y), P(x + w, y + h), C(top), C(top), C(bottom), C(bottom));
}

float Canvas::TextWidth(ImFont* f, float size, const std::string& t) const {
  if (!f)
    f = ImGui::GetFont();
  return f->CalcTextSizeA(size, FLT_MAX, 0.0f, t.c_str()).x;
}

float Canvas::Text(ImFont* f, float size, float x, float y, ImU32 col, const std::string& t, float align) const {
  if (!f)
    f = ImGui::GetFont();
  const float w = TextWidth(f, size, t);
  dl->AddText(f, size * s, P(x - w * align, y), C(col), t.c_str());
  return w;
}

float Canvas::Wrapped(ImFont* f, float size, float x, float y, float width, ImU32 col, const std::string& t,
                      float line) const {
  if (!f)
    f = ImGui::GetFont();
  // Greedy word wrap in virtual units, so the break points do not move with
  // the display's size.
  float yy = y;
  size_t start = 0;
  while (start < t.size()) {
    size_t end = start, last_fit = std::string::npos;
    while (end <= t.size()) {
      if (end == t.size() || t[end] == ' ' || t[end] == '\n') {
        if (TextWidth(f, size, t.substr(start, end - start)) > width && last_fit != std::string::npos)
          break;
        last_fit = end;
        if (end == t.size() || t[end] == '\n')
          break;
      }
      ++end;
    }
    if (last_fit == std::string::npos)
      last_fit = t.size();
    Text(f, size, x, yy, col, t.substr(start, last_fit - start));
    yy += size * line;
    start = last_fit + 1;
  }
  return yy - y;
}

void Canvas::Embossed(ImFont* f, float size, float x, float y, ImU32 face, const std::string& t, float align,
                      float glow) const {
  if (!f)
    f = ImGui::GetFont();
  const float w = TextWidth(f, size, t);
  const float left = x - w * align;
  if (glow > 0.01f) {
    // A soft red bloom: the same text, larger offsets, falling alpha.
    for (int ring = 3; ring >= 1; --ring) {
      const float o = 2.5f * float(ring);
      const ImU32 g = IM_COL32(220, 30, 24, int(46.0f * glow / float(ring)));
      for (int k = 0; k < 8; ++k) {
        const float a = float(k) * 0.785398f;
        dl->AddText(f, size * s, P(left + std::cos(a) * o, y + std::sin(a) * o), C(g), t.c_str());
      }
    }
  }
  // The dark outline, then the bevel: a light top-left edge and a dark
  // bottom-right edge around the face.
  const ImU32 outline = IM_COL32(8, 8, 10, 230);
  for (int k = 0; k < 8; ++k) {
    const float a = float(k) * 0.785398f;
    dl->AddText(f, size * s, P(left + std::cos(a) * 2.2f, y + std::sin(a) * 2.2f), C(outline), t.c_str());
  }
  dl->AddText(f, size * s, P(left + 1.2f, y + 1.6f), C(IM_COL32(40, 36, 32, 255)), t.c_str());
  dl->AddText(f, size * s, P(left - 0.8f, y - 1.0f), C(IM_COL32(255, 252, 240, 200)), t.c_str());
  dl->AddText(f, size * s, P(left, y), C(face), t.c_str());
}

void Canvas::Glyph(uint32_t button, float cx, float cy, float r) const {
  const ImVec2 c = P(cx, cy);
  const float R = r * s;
  const ImU32 ring = C(IM_COL32(232, 232, 232, 255));
  const ImU32 fill = C(IM_COL32(30, 30, 34, 235));
  auto pill = [&](const char* label) {
    const float w = r * 2.6f, h = r * 1.5f;
    dl->AddRectFilled(P(cx - w / 2, cy - h / 2), P(cx + w / 2, cy + h / 2), fill, h * 0.5f * s);
    dl->AddRect(P(cx - w / 2, cy - h / 2), P(cx + w / 2, cy + h / 2), ring, h * 0.5f * s, 0, 1.6f * s);
    Text(GetFonts().bold, r * 0.95f, cx, cy - r * 0.55f, IM_COL32(240, 240, 240, 255), label, 0.5f);
  };
  switch (button) {
    case kPadL1: pill("L1"); return;
    case kPadR1: pill("R1"); return;
    case kPadOptions: pill("OPT"); return;
    case kPadTouchpad: pill("TP"); return;
    default: break;
  }
  if (button == (kPadUp | kPadDown) || button == (kPadLeft | kPadRight)) {
    // A d-pad cross with the two arms that matter lit.
    const bool vertical = button == (kPadUp | kPadDown);
    const float a = r * 0.36f, l = r * 0.95f;
    dl->AddRectFilled(P(cx - a, cy - l), P(cx + a, cy + l), C(vertical ? IM_COL32(235, 235, 235, 255)
                                                                         : IM_COL32(110, 110, 110, 255)), 2 * s);
    dl->AddRectFilled(P(cx - l, cy - a), P(cx + l, cy + a), C(vertical ? IM_COL32(110, 110, 110, 255)
                                                                         : IM_COL32(235, 235, 235, 255)), 2 * s);
    return;
  }
  dl->AddCircleFilled(c, R, fill, 32);
  dl->AddCircle(c, R, ring, 32, 1.6f * s);
  const float g = R * 0.48f, t = std::max(1.5f, 2.6f * s);
  switch (button) {
    case kPadCross: {
      const ImU32 col = C(IM_COL32(124, 178, 232, 255));
      dl->AddLine(ImVec2(c.x - g, c.y - g), ImVec2(c.x + g, c.y + g), col, t);
      dl->AddLine(ImVec2(c.x - g, c.y + g), ImVec2(c.x + g, c.y - g), col, t);
      break;
    }
    case kPadCircle:
      dl->AddCircle(c, g * 1.05f, C(IM_COL32(255, 102, 102, 255)), 24, t);
      break;
    case kPadSquare:
      dl->AddRect(ImVec2(c.x - g * 0.9f, c.y - g * 0.9f), ImVec2(c.x + g * 0.9f, c.y + g * 0.9f),
                  C(IM_COL32(255, 105, 248, 255)), 0, 0, t);
      break;
    case kPadTriangle: {
      const ImU32 col = C(IM_COL32(64, 226, 160, 255));
      dl->AddTriangle(ImVec2(c.x, c.y - g * 1.05f), ImVec2(c.x - g * 1.05f, c.y + g * 0.8f),
                      ImVec2(c.x + g * 1.05f, c.y + g * 0.8f), col, t);
      break;
    }
    default: break;
  }
}

float Canvas::Hint(float right, float y, uint32_t button, const char* label) const {
  const float size = 24.0f;
  const float w = TextWidth(GetFonts().body, size, label);
  Text(GetFonts().body, size, right - w, y - size * 0.62f, colour::kIvory, label);
  const float r = 15.0f;
  const bool pill = button == kPadL1 || button == kPadR1 || button == kPadOptions || button == kPadTouchpad;
  const float gw = pill ? r * 2.6f : r * 2.0f;
  Glyph(button, right - w - 12.0f - gw * 0.5f, y, r);
  return right - w - 12.0f - gw - 34.0f;
}

float Ease(float& v, float target, float dt, float speed) {
  v += (target - v) * (1.0f - std::exp(-speed * dt));
  if (std::fabs(target - v) < 0.001f)
    v = target;
  return v;
}

// --- SettingsList ------------------------------------------------------------

bool SettingsList::Update(const PadState& pad, Ng2Settings& s, bool in_game) {
  if (rows_.empty())
    return false;
  const int n = int(rows_.size());
  if (pad.pressed & kPadUp) {
    sel_ = (sel_ + n - 1) % n;
    help_ = false;
    PlaySound(Sound::kMove);
  }
  if (pad.pressed & kPadDown) {
    sel_ = (sel_ + 1) % n;
    help_ = false;
    PlaySound(Sound::kMove);
  }
  if (pad.pressed & kPadTriangle) {
    help_ = !help_;
    PlaySound(Sound::kSelect);
  }
  const Setting& row = *rows_[size_t(sel_)];
  const bool locked = (row.unavailable && !row.unavailable(s).empty()) || (in_game && row.restart);
  int dir = 0;
  // Left/Right change a value; Cross does not, so "selecting" a row can never
  // flip a setting by accident.
  if (pad.pressed & kPadRight) dir = 1;
  if (pad.pressed & kPadLeft) dir = -1;
  if (dir == 0 && !(pad.pressed & kPadSquare))
    return false;
  if (locked) {
    PlaySound(Sound::kError);
    nudge_ = float(dir ? dir : 1);
    return false;
  }
  const int count = int(row.choices.size());
  const int current = std::clamp(row.get(s), 0, count - 1);
  const int next = (pad.pressed & kPadSquare) ? row.def : (current + dir + count) % count;
  if (next == current)
    return false;
  row.set(s, next);
  s.Clamp();
  PlaySound(Sound::kChange);
  return true;
}

void SettingsList::Draw(const Canvas& c, const Ng2Settings& s, float x, float y, float w, float h, bool in_game,
                        float dt) {
  const Fonts& f = GetFonts();
  const float rh = 64.0f;
  const float help_h = 92.0f;
  const float list_h = h - help_h;
  const int n = int(rows_.size());
  // Keep the focused row in view, eased so the list glides rather than jumps.
  const float want = std::clamp(float(sel_) * rh - list_h * 0.5f + rh * 0.5f, 0.0f,
                                std::max(0.0f, float(n) * rh - list_h));
  Ease(scroll_, want, dt, 12.0f);
  if (focus_y_ < 0)
    focus_y_ = float(sel_) * rh;
  Ease(focus_y_, float(sel_) * rh, dt, 18.0f);
  Ease(nudge_, 0.0f, dt, 10.0f);

  c.dl->PushClipRect(c.P(x, y), c.P(x + w, y + list_h), true);
  const float fy = y + focus_y_ - scroll_;
  c.Rect(x, fy, w, rh, IM_COL32(150, 18, 16, 120));
  c.Rect(x, fy, 6.0f, rh, colour::kRed);
  for (int i = 0; i < n; ++i) {
    const float ry = y + float(i) * rh - scroll_;
    if (ry + rh < y || ry > y + list_h)
      continue;
    const Setting& row = *rows_[size_t(i)];
    const std::string why = row.unavailable ? row.unavailable(s) : std::string();
    const bool greyed = !why.empty() || (in_game && row.restart);
    const ImU32 label_col = greyed ? colour::kMuted : (i == sel_ ? colour::kIvory : colour::kIvoryDim);
    c.Text(f.bold, 30.0f, x + 28.0f, ry + 15.0f, label_col, row.label);
    std::string value = row.choices.empty() ? std::string()
                                            : row.choices[size_t(std::clamp(row.get(s), 0, int(row.choices.size()) - 1))];
    const float vx = x + w - 40.0f + (i == sel_ ? nudge_ * 8.0f : 0.0f);
    c.Text(f.body, 30.0f, vx, ry + 15.0f, greyed ? colour::kMuted : colour::kIvory, value, 1.0f);
    if (i == sel_ && !greyed && row.choices.size() > 1) {
      const float vw = c.TextWidth(f.body, 30.0f, value);
      c.Text(f.body, 30.0f, vx - vw - 18.0f, ry + 15.0f, colour::kRed, "<", 1.0f);
      c.Text(f.body, 30.0f, vx + 8.0f, ry + 15.0f, colour::kRed, ">");
    }
    std::string tag;
    if (in_game && row.restart)
      tag = "restart";
    else if (!in_game && row.restart)
      tag = "next start";
    if (!tag.empty())
      c.Text(f.body, 20.0f, x + 28.0f + c.TextWidth(f.bold, 30.0f, row.label) + 16.0f, ry + 22.0f,
             colour::kMuted, tag);
  }
  c.dl->PopClipRect();

  // The focused row's description (Triangle: the longer help).
  if (const Setting* row = Current()) {
    const std::string why = row->unavailable ? row->unavailable(s) : std::string();
    std::string text = !why.empty() ? why
                       : (in_game && row->restart)
                           ? std::string("Changed from the hub before START GAME; this one cannot change mid-game.")
                           : std::string(row->help);
    c.Rect(x, y + list_h + 8.0f, w, help_h - 8.0f, IM_COL32(0, 0, 0, 120), 6.0f);
    c.Wrapped(f.body, 24.0f, x + 24.0f, y + list_h + 22.0f, w - 48.0f, why.empty() ? colour::kIvoryDim : colour::kWarn,
              text);
  }
}

}  // namespace ng2ps5::hub
