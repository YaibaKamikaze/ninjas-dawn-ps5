// Ninja Gaiden II for PS5 - the hub's backdrop and logo, painted in code
// (hub_backdrop.cpp). No game art: this is a drawing after the title menu's
// scene, not a copy of it.
#pragma once

#include "ps5/hub/hub_ui.h"

namespace ng2ps5::hub {

// The whole 1920x1080 canvas: misty grey sky, layered mountains, a grass
// field with katanas standing in it (drifting with a slow parallax), fog.
void DrawBackdrop(const Canvas& c, float time);

// "NINJA GAIDEN" in chrome with a red brush-stroke "II", centred at (cx, y)
// (y is the top of the letters), `scale` 1 = the hub's size.
void DrawLogo(const Canvas& c, float cx, float y, float scale, float time);

}  // namespace ng2ps5::hub
