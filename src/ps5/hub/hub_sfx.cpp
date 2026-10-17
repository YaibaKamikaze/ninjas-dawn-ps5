// Ninja Gaiden II for PS5 - the menus' sounds (hub_ui.h): made here, in code,
// so no recording ships and none is borrowed. A sword theme, after the title
// menu: a short metallic "shing" to move, a blade slash to confirm, a muted
// thud to go back. hub/sfx/<name>.wav replaces any of them.

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include <rex/logging.h>

#include "ps5/hub/hub_ui.h"
#include "ps5/ps5_sound.h"

namespace ng2ps5::hub {

namespace {

constexpr float kRate = 48000.0f;
constexpr float kTau = 6.2831853f;

// A tiny deterministic noise source: the same sound every launch.
struct Noise {
  uint32_t state = 0x9E3779B9u;
  float Next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return float(int32_t(state)) / 2147483648.0f;
  }
};

std::vector<float> Stereo(const std::vector<float>& mono, float pan = 0.0f) {
  std::vector<float> out(mono.size() * 2);
  const float l = std::sqrt(0.5f * (1.0f - pan)), r = std::sqrt(0.5f * (1.0f + pan));
  for (size_t i = 0; i < mono.size(); ++i) {
    out[i * 2] = mono[i] * l;
    out[i * 2 + 1] = mono[i] * r;
  }
  return out;
}

// Struck metal: inharmonic partials, each decaying at its own rate, the high
// ones first - which is what makes it read as a blade rather than a bell.
std::vector<float> Shing(float seconds, float base, float gain) {
  static constexpr std::array<float, 5> kRatios = {1.0f, 1.47f, 2.09f, 2.56f, 3.43f};
  const size_t n = size_t(seconds * kRate);
  std::vector<float> m(n);
  Noise noise;
  for (size_t i = 0; i < n; ++i) {
    const float t = float(i) / kRate;
    const float attack = std::min(1.0f, t * 900.0f);
    float v = 0;
    for (size_t k = 0; k < kRatios.size(); ++k) {
      const float decay = std::exp(-t * (9.0f + 7.0f * float(k)) / seconds * 0.35f);
      v += std::sin(kTau * base * kRatios[k] * t) * decay / float(k + 1);
    }
    v += noise.Next() * 0.25f * std::exp(-t * 90.0f);  // the scrape at the start
    m[i] = v * attack * gain;
  }
  return m;
}

// A slash: noise swept down through a resonant filter, then a ring.
std::vector<float> Slash(float seconds, float gain) {
  const size_t n = size_t(seconds * kRate);
  std::vector<float> m(n);
  Noise noise;
  float lp = 0, bp = 0;
  for (size_t i = 0; i < n; ++i) {
    const float t = float(i) / kRate;
    const float sweep = 6000.0f * std::exp(-t * 9.0f) + 500.0f;
    const float f = 2.0f * std::sin(3.14159f * sweep / kRate);
    const float x = noise.Next();
    lp += f * bp;
    const float hp = x - lp - 0.35f * bp;
    bp += f * hp;
    const float env = std::min(1.0f, t * 400.0f) * std::exp(-t * 11.0f);
    m[i] = bp * env * gain;
  }
  const auto ring = Shing(seconds, 1450.0f, gain * 0.35f);
  for (size_t i = 0; i < n && i < ring.size(); ++i)
    m[i] += ring[i];
  return m;
}

// A muted thud: a falling low sine with a little noise on the attack.
std::vector<float> Thud(float seconds, float gain) {
  const size_t n = size_t(seconds * kRate);
  std::vector<float> m(n);
  Noise noise;
  float phase = 0;
  for (size_t i = 0; i < n; ++i) {
    const float t = float(i) / kRate;
    const float f = 70.0f + 60.0f * std::exp(-t * 30.0f);
    phase += kTau * f / kRate;
    const float env = std::min(1.0f, t * 600.0f) * std::exp(-t * 22.0f);
    m[i] = (std::sin(phase) + noise.Next() * 0.15f * std::exp(-t * 120.0f)) * env * gain;
  }
  return m;
}

std::vector<float> Buzz(float gain) {
  const size_t n = size_t(0.22f * kRate);
  std::vector<float> m(n);
  for (size_t i = 0; i < n; ++i) {
    const float t = float(i) / kRate;
    const float gate = (t < 0.08f || (t > 0.12f && t < 0.2f)) ? 1.0f : 0.0f;
    m[i] = (std::fmod(t * 110.0f, 1.0f) * 2.0f - 1.0f) * 0.5f * gate * gain;
  }
  return m;
}

const char* Name(Sound s) {
  switch (s) {
    case Sound::kMove: return "move";
    case Sound::kSelect: return "select";
    case Sound::kBack: return "back";
    case Sound::kTab: return "tab";
    case Sound::kChange: return "change";
    case Sound::kError: return "error";
    case Sound::kStart: return "start";
    case Sound::kOpen: return "open";
    default: return "";
  }
}

std::vector<float> Synth(Sound s) {
  switch (s) {
    case Sound::kMove: return Stereo(Shing(0.16f, 2350.0f, 0.18f), 0.1f);
    case Sound::kSelect: return Stereo(Slash(0.28f, 0.45f));
    case Sound::kBack: return Stereo(Thud(0.18f, 0.5f));
    case Sound::kTab: return Stereo(Shing(0.2f, 1700.0f, 0.16f), -0.2f);
    case Sound::kChange: return Stereo(Shing(0.09f, 3100.0f, 0.12f));
    case Sound::kError: return Stereo(Buzz(0.22f));
    case Sound::kStart: return Stereo(Slash(0.6f, 0.55f));
    case Sound::kOpen: return Stereo(Shing(0.35f, 1250.0f, 0.22f));
    default: return {};
  }
}

std::array<sound::Clip, size_t(Sound::kCount)> g_clips;
bool g_ready = false;

}  // namespace

void InitSounds(const std::string& hub_folder) {
  if (g_ready)
    return;
  g_ready = true;
  sound::Start();
  int replaced = 0;
  for (int k = 0; k < int(Sound::kCount); ++k) {
    const Sound s = Sound(k);
    sound::Clip clip = sound::LoadWav(std::filesystem::path(hub_folder) / "sfx" / (std::string(Name(s)) + ".wav"));
    if (clip)
      ++replaced;
    else
      clip = std::make_shared<const std::vector<float>>(Synth(s));
    g_clips[size_t(k)] = std::move(clip);
  }
  REXLOG_INFO("PS5 hub: menu sounds ready ({} replaced from hub/sfx)", replaced);
}

void PlaySound(Sound sound) {
  if (g_ready && sound != Sound::kCount)
    sound::Play(g_clips[size_t(sound)]);
}

}  // namespace ng2ps5::hub
