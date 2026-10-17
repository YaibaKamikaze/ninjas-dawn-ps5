// Ninja Gaiden II for PS5 - the menus' own sound (ps5_sound.cpp).
//
// A small mixer on an audio-out port of its own: the hub's menu sounds and its
// optional music, and the in-game overlay's sounds. The game's audio has its
// own port in the SDK's audio driver; the system mixes the two, so nothing
// here can starve or delay the game's sound.
//
// 48 kHz stereo, 256-sample frames, the port shape the SDK's driver uses.
#pragma once

#include <filesystem>
#include <memory>
#include <vector>

namespace ng2ps5::sound {

// Interleaved stereo float samples at 48 kHz.
using Clip = std::shared_ptr<const std::vector<float>>;

// Opens the port and starts the mixer thread. Safe to call more than once.
bool Start();
// Lets queued sounds finish, then closes the port.
void Stop();

void Play(const Clip& clip, float volume = 1.0f);

// Music: a WAV file (16-bit PCM or 32-bit float, 44.1 or 48 kHz, mono or
// stereo), looped - from its smpl chunk's loop start when it has one, so an
// opening plays once and the rest repeats. volume 0..1; Stop/FadeMusic end it.
bool PlayMusic(const std::filesystem::path& wav, float volume);
void SetMusicVolume(float volume);
void FadeMusic(float seconds);

// A WAV file as a clip (for hub/sfx/<name>.wav replacements); nullptr if it
// cannot be read. loop_start, if given, receives the smpl chunk's first loop
// start as an index into the clip (0 without one).
Clip LoadWav(const std::filesystem::path& wav, size_t* loop_start = nullptr);

}  // namespace ng2ps5::sound
