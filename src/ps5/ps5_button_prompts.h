// Ninja Gaiden II for PS5 - PlayStation button prompts (ps5_button_prompts.cpp).
//
// The one change this port makes to what the game shows (docs/PS5_PORT.md 5.9). When
// the game loads one of its Xbox button pictures, the PlayStation
// picture drawn for this project is written over it in guest memory, in the
// guest's tiling and byte order, just before the GPU copy is made - so the
// game draws it exactly as it would have drawn its own. Nothing in Game Data/
// changes, and the Original setting turns it off.
//
// A picture is found by the FNV-1a 64 hash of the Xbox picture's guest bytes:
//
//   hub/button_prompts/playstation/<hash>.dds   DXT1/DXT3/DXT5, same size as
//                                                the Xbox picture, no mips
//   hub/button_prompts/playstation/<hash>.bin   or the exact guest bytes
//   hub/button_prompts/playstation/<hash>.rect  or only rectangles drawn for
//                                                this project, pasted over the
//                                                game's own picture (the HUD's
//                                                k_8 index pictures: the button
//                                                atlas keeps the game's sticks
//                                                and d-pad). Made by
//                                                ps5/tools/make_button_prompts.py
//
// The hashes come from the HUD picture dump (SETTINGS > Developer): every
// small picture and HUD index picture the game loads, written once to
// logs/hud_pictures/ (ps5/tools/decode_hud_pictures.py reads them).
#pragma once

#include <cstdint>
#include <filesystem>

namespace rex::graphics {
struct GuestTextureLoad;
}

namespace ng2ps5::prompts {

// Indexes the PlayStation pictures in <app>/hub/button_prompts/playstation.
void Init(const std::filesystem::path& app);

// From the texture hook (GPU thread). `guest` is the base level's bytes.
void OnTextureLoad(const rex::graphics::GuestTextureLoad& texture, uint8_t* guest, bool replace,
                   bool dump);

}  // namespace ng2ps5::prompts
