// Ninja Gaiden II for PS5 - the hub's backdrop and logo (hub_backdrop.h).

#include "ps5/hub/hub_backdrop.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace ng2ps5::hub {

namespace {

// Deterministic hash noise: the same scene on every launch.
float Hash(float x) {
  const float s = std::sin(x * 127.1f) * 43758.5453f;
  return s - std::floor(s);
}

float ValueNoise(float x) {
  const float i = std::floor(x), f = x - i;
  const float u = f * f * (3.0f - 2.0f * f);
  return Hash(i) * (1.0f - u) + Hash(i + 1.0f) * u;
}

// A ridge line: a few octaves of noise, sharper with each one.
float Ridge(float x, float seed) {
  float v = 0, amp = 1, freq = 1, total = 0;
  for (int o = 0; o < 4; ++o) {
    v += (1.0f - std::fabs(ValueNoise(x * freq + seed * 17.0f) * 2.0f - 1.0f)) * amp;
    total += amp;
    amp *= 0.5f;
    freq *= 2.1f;
  }
  return v / total;
}

ImU32 Grey(int v, int a = 255) { return IM_COL32(v, v, std::min(255, v + 4), a); }

// One mountain layer: a filled silhouette from x = 0 to 1920, as vertical
// quads (a concave outline cannot go through AddConvexPolyFilled whole).
void Mountains(const Canvas& c, float base_y, float height, float seed, float drift, ImU32 col) {
  const int steps = 96;
  const float w = 1920.0f / float(steps);
  for (int i = 0; i < steps; ++i) {
    const float x0 = float(i) * w, x1 = x0 + w;
    const float y0 = base_y - Ridge((x0 + drift) / 520.0f, seed) * height;
    const float y1 = base_y - Ridge((x1 + drift) / 520.0f, seed) * height;
    const ImVec2 q[4] = {c.P(x0, y0), c.P(x1 + 0.5f, y1), c.P(x1 + 0.5f, 1080.0f), c.P(x0, 1080.0f)};
    c.dl->AddConvexPolyFilled(q, 4, c.C(col));
  }
}

// A katana standing point-down in the field: blade, guard (tsuba), handle.
// Silhouettes only, dark against the fog, as in the title scene.
void Katana(const Canvas& c, float x, float ground, float length, float tilt, float alpha) {
  const float blade = length * 0.72f, handle = length * 0.24f;
  const float dx = std::sin(tilt), dy = -std::cos(tilt);
  auto pt = [&](float along, float across) {
    return c.P(x + dx * along - dy * across, ground + dy * along + dx * across);
  };
  const ImU32 steel = c.C(IM_COL32(22, 22, 26, int(235 * alpha)));
  const ImU32 edge = c.C(IM_COL32(120, 122, 128, int(90 * alpha)));
  const float bw = std::max(2.0f, length * 0.018f);
  // The blade, slightly curved: two segments.
  const ImVec2 b[4] = {pt(0, -bw * 0.2f), pt(blade, -bw), pt(blade, bw), pt(0, bw * 0.2f)};
  c.dl->AddConvexPolyFilled(b, 4, steel);
  c.dl->AddLine(pt(blade * 0.05f, -bw * 0.25f), pt(blade, -bw * 0.9f), edge, std::max(1.0f, c.s));
  // The guard, a flat oval.
  const ImVec2 g0 = pt(blade, 0);
  c.dl->AddEllipseFilled(g0, ImVec2(bw * 3.2f * c.s, bw * 1.1f * c.s), steel, -tilt, 16);
  // The handle, wrapped: a darker band with a few lighter diamonds.
  const ImVec2 h[4] = {pt(blade + bw, -bw * 0.9f), pt(blade + handle, -bw * 0.8f), pt(blade + handle, bw * 0.8f),
                       pt(blade + bw, bw * 0.9f)};
  c.dl->AddConvexPolyFilled(h, 4, c.C(IM_COL32(12, 10, 12, int(245 * alpha))));
  for (int k = 1; k < 5; ++k) {
    const float a = blade + bw + (handle - bw) * float(k) / 5.0f;
    c.dl->AddLine(pt(a, -bw * 0.7f), pt(a + bw, bw * 0.7f), c.C(IM_COL32(70, 66, 60, int(120 * alpha))),
                  std::max(1.0f, c.s));
  }
}

struct Blade {
  float x, depth, length, tilt;
};

const std::vector<Blade>& Field() {
  static const std::vector<Blade> field = [] {
    std::vector<Blade> v;
    for (int i = 0; i < 46; ++i) {
      Blade b;
      b.depth = Hash(float(i) * 3.7f + 1.0f);  // 0 far .. 1 near
      b.x = Hash(float(i) * 9.1f + 4.0f) * 2300.0f - 190.0f;
      b.length = 120.0f + b.depth * b.depth * 360.0f;
      b.tilt = (Hash(float(i) * 5.3f + 2.0f) - 0.5f) * 0.32f;
      v.push_back(b);
    }
    // Far ones first, so the near ones are drawn over them.
    std::sort(v.begin(), v.end(), [](const Blade& a, const Blade& b) { return a.depth < b.depth; });
    return v;
  }();
  return field;
}

}  // namespace

void DrawBackdrop(const Canvas& c, float time) {
  // The sky: grey, lighter towards the horizon, as through mist.
  c.Gradient(0, 0, 1920, 640, Grey(74), Grey(168));
  c.Gradient(0, 640, 1920, 440, Grey(168), Grey(120));

  // Three ranges, each paler and slower with distance (aerial perspective).
  Mountains(c, 640, 260, 1.0f, time * 2.0f, Grey(138, 255));
  Mountains(c, 700, 230, 2.0f, time * 5.0f + 300, Grey(112, 255));
  Mountains(c, 770, 200, 3.0f, time * 9.0f + 900, Grey(84, 255));

  // A band of low fog over the valley floor.
  c.Gradient(0, 640, 1920, 180, Grey(170, 0), Grey(176, 200));
  c.Gradient(0, 820, 1920, 260, Grey(176, 200), Grey(150, 120));

  // The field: dark grass rising to the bottom edge.
  c.Gradient(0, 840, 1920, 240, Grey(70, 230), Grey(28, 255));
  for (int i = 0; i < 260; ++i) {
    const float x = Hash(float(i) * 1.3f) * 1920.0f;
    const float h = 14.0f + Hash(float(i) * 2.9f) * 30.0f;
    const float y = 860.0f + Hash(float(i) * 4.1f) * 220.0f;
    const float sway = std::sin(time * 0.8f + float(i)) * 3.0f;
    c.dl->AddLine(c.P(x, y), c.P(x + sway, y - h), c.C(Grey(30, 200)), std::max(1.0f, 1.6f * c.s));
  }

  // The katanas, with a parallax drift: near ones move more.
  for (const Blade& b : Field()) {
    const float drift = std::fmod(time * (3.0f + b.depth * 14.0f), 2300.0f);
    float x = b.x - drift;
    if (x < -190.0f)
      x += 2300.0f;
    const float ground = 860.0f + b.depth * 200.0f;
    Katana(c, x, ground, b.length, b.tilt, 0.45f + b.depth * 0.55f);
  }

  // Slow drifting fog in front of everything, two layers moving apart.
  for (int layer = 0; layer < 2; ++layer) {
    const float speed = layer ? 11.0f : -7.0f;
    for (int i = 0; i < 7; ++i) {
      const float w = 700.0f;
      float x = std::fmod(float(i) * 420.0f + time * speed, 2600.0f) - 400.0f;
      if (x < -700.0f)
        x += 2600.0f;
      const float y = 760.0f + float(layer) * 140.0f + Hash(float(i) + float(layer) * 9.0f) * 60.0f;
      const int a = 40 + int(Hash(float(i) * 7.0f) * 40.0f);
      c.dl->AddRectFilledMultiColor(c.P(x, y), c.P(x + w * 0.5f, y + 120), c.C(Grey(190, 0)),
                                    c.C(Grey(190, a)), c.C(Grey(190, a)), c.C(Grey(190, 0)));
      c.dl->AddRectFilledMultiColor(c.P(x + w * 0.5f, y), c.P(x + w, y + 120), c.C(Grey(190, a)),
                                    c.C(Grey(190, 0)), c.C(Grey(190, 0)), c.C(Grey(190, a)));
    }
  }

  // A vignette, so the menu column in the middle reads clearly.
  c.Gradient(0, 0, 1920, 200, IM_COL32(0, 0, 0, 120), IM_COL32(0, 0, 0, 0));
  c.Gradient(0, 900, 1920, 180, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 150));
}

void DrawLogo(const Canvas& c, float cx, float y, float scale, float time) {
  const Fonts& f = GetFonts();
  const float size = 132.0f * scale;
  const std::string text = "NINJA GAIDEN";
  const float w = c.TextWidth(f.logo, size, text);
  const float left = cx - w * 0.5f - 60.0f * scale;
  ImFont* font = f.logo ? f.logo : ImGui::GetFont();

  // A heavy black bevel under the letters.
  for (int k = 0; k < 12; ++k) {
    const float a = float(k) * 0.5236f;
    c.dl->AddText(font, size * c.s, c.P(left + std::cos(a) * 5.0f * scale, y + std::sin(a) * 5.0f * scale + 3.0f),
                  c.C(IM_COL32(6, 6, 8, 255)), text.c_str());
  }
  // Chrome: the letters drawn white, then their vertices recoloured by height
  // - bright sky above, a dark horizon band, a lighter ground below it - which
  // is how polished metal reflects a landscape.
  ImDrawList* dl = c.dl;
  const int first = dl->VtxBuffer.Size;
  dl->AddText(font, size * c.s, c.P(left, y), c.C(IM_COL32_WHITE), text.c_str());
  const float top = c.P(0, y + size * 0.18f).y, bottom = c.P(0, y + size * 0.92f).y;
  const float shine = 0.5f + 0.5f * std::sin(time * 0.6f);
  for (int i = first; i < dl->VtxBuffer.Size; ++i) {
    ImDrawVert& v = dl->VtxBuffer[i];
    const float t = std::clamp((v.pos.y - top) / std::max(1.0f, bottom - top), 0.0f, 1.0f);
    float g;
    if (t < 0.48f)
      g = 255.0f - t * 170.0f;               // bright to grey
    else if (t < 0.56f)
      g = 60.0f;                              // the horizon
    else
      g = 110.0f + (t - 0.56f) * 230.0f;      // the lit ground
    g = std::min(255.0f, g + shine * 18.0f);
    const uint32_t a = (v.col >> IM_COL32_A_SHIFT) & 0xFF;
    v.col = IM_COL32(int(g), int(g), int(std::min(255.0f, g + 6.0f)), int(a));
  }

  // The red brush-stroke "II": two strokes, each a tapered ragged polygon,
  // dry-brushed at the ends, leaning like a quick hand.
  const float bx = left + w + 36.0f * scale;
  const float by = y - 14.0f * scale;
  const float h = size * 1.12f;
  for (int stroke = 0; stroke < 2; ++stroke) {
    const float x0 = bx + float(stroke) * 52.0f * scale;
    const int segments = 18;
    std::vector<ImVec2> left_edge, right_edge;
    for (int i = 0; i <= segments; ++i) {
      const float t = float(i) / float(segments);
      const float lean = (1.0f - t) * 20.0f * scale;
      // Thick in the middle, a sharp entry and a dragged exit.
      const float width = (8.0f + 26.0f * std::sin(3.14159f * std::pow(t, 0.8f))) * scale;
      const float jitter = (Hash(float(i) * 3.1f + float(stroke) * 11.0f) - 0.5f) * 6.0f * scale;
      const float x = x0 + lean;
      const float yy = by + t * h;
      left_edge.push_back(c.P(x - width * 0.5f + jitter, yy));
      right_edge.push_back(c.P(x + width * 0.5f + jitter * 0.5f, yy));
    }
    for (int i = 0; i < segments; ++i) {
      const ImVec2 q[4] = {left_edge[size_t(i)], right_edge[size_t(i)], right_edge[size_t(i + 1)],
                           left_edge[size_t(i + 1)]};
      c.dl->AddConvexPolyFilled(q, 4, c.C(colour::kRedDeep));
      // The wet core of the stroke, inset.
      const ImVec2 mid0 = ImVec2((q[0].x * 0.7f + q[1].x * 0.3f), q[0].y);
      const ImVec2 mid1 = ImVec2((q[0].x * 0.3f + q[1].x * 0.7f), q[1].y);
      const ImVec2 mid2 = ImVec2((q[3].x * 0.3f + q[2].x * 0.7f), q[2].y);
      const ImVec2 mid3 = ImVec2((q[3].x * 0.7f + q[2].x * 0.3f), q[3].y);
      const ImVec2 core[4] = {mid0, mid1, mid2, mid3};
      c.dl->AddConvexPolyFilled(core, 4, c.C(colour::kRed));
    }
    // Dry-brush bristle marks trailing off the bottom.
    for (int k = 0; k < 6; ++k) {
      const float t = Hash(float(k) * 5.7f + float(stroke) * 3.0f);
      const ImVec2 a = right_edge.back(), b = left_edge.back();
      const ImVec2 p0(a.x + (b.x - a.x) * t, a.y);
      c.dl->AddLine(p0, ImVec2(p0.x - 4.0f * c.s, p0.y + (10.0f + 24.0f * t) * scale * c.s),
                    c.C(IM_COL32(170, 20, 18, 200)), std::max(1.0f, 2.2f * scale * c.s));
    }
  }
}

}  // namespace ng2ps5::hub
