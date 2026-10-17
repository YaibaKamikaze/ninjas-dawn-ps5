PlayStation button pictures for Ninja Gaiden II on PS5.

When the game loads one of its Xbox button pictures, the PlayStation picture
here is written over it in memory, in the game's own tiling and byte order,
before the GPU sees it. The game's files are never changed. Every picture
here is drawn for this project. SETTINGS > Game > Button prompts switches
between PlayStation and the original buttons.

  playstation/<hash>.dds   DXT1, DXT3 or DXT5 (matching the Xbox picture),
                           the same width and height, no mipmaps
  playstation/<hash>.bin   or the exact bytes the game expects, already in
                           its layout (for a format the .dds route does not
                           cover - the HUD's 8-bit index pictures, k_8)
  playstation/<hash>.rect  or only rectangles drawn for this project, pasted
                           over the game's own picture in memory (k_8: the
                           values are indices into the picture's own colour
                           table). Made by ps5/tools/make_button_prompts.py.

<hash> is the FNV-1a 64 hash of the Xbox picture's bytes, 16 hex digits.

Finding the Xbox pictures: turn on SETTINGS > Developer > Dump HUD pictures,
start the game, visit the screens with button icons, and look in
logs/hud_pictures/. Each file is named
<hash>_<width>x<height>_f<format>_t<tiled>_e<endian>_p<pitch>_m<mips>_a<address>.bin.
ps5/tools/decode_hud_pictures.py turns them into PNGs.

The game's HUD is mostly 8-bit index pictures (format 2) drawn through a
256x1 colour table (format 6) kept in the page after each picture; the
decoder pairs them by address. A replacement for an index picture is a .bin
of new indices into the same colour table.

Mapping: A->Cross, B->Circle, X->Square, Y->Triangle, LB/RB->L1/R1,
LT/RT->L2/R2, sticks and clicks->L/R sticks and L3/R3, START->OPTIONS,
BACK->touchpad.

In place in v0.1.0-alpha, all as .rect:
  8F9624F514C0E9AB  the button atlas: face buttons -> Square/Triangle/Circle/
                    Cross, LT/RT -> L2/R2, LB/RB -> L1/R1, BACK -> TOUCH (the
                    touch pad), START -> OPTIONS; sticks and d-pad unchanged
  F3C472A55C851518  "Xbox LIVE Marketplace" -> "Downloadable Content"
  D66D4AD32FB753CE  "PRESS START BUTTON" -> "PRESS OPTIONS BUTTON"
  572F595ECBA7B762  action names: "Gamer Card" -> "Profile"
