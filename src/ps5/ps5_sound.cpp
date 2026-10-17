// Ninja Gaiden II for PS5 - the menus' own sound (ps5_sound.h).

#include "ps5/ps5_sound.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <thread>

#include <rex/logging.h>

extern "C" {
int32_t sceAudioOutInit(void);
int32_t sceAudioOutOpen(int32_t user_id, int32_t type, int32_t index, uint32_t len, uint32_t freq,
                        uint32_t param);
int32_t sceAudioOutOutput(int32_t handle, const void* ptr);
int32_t sceAudioOutClose(int32_t handle);
}

namespace ng2ps5::sound {

namespace {

constexpr int32_t kSystemUser = 0xFF;
constexpr int32_t kPortTypeMain = 0;
constexpr uint32_t kFormatS16Stereo = 1;
constexpr uint32_t kFrames = 256;
constexpr uint32_t kRate = 48000;

struct Voice {
  Clip clip;
  size_t pos = 0;  // in samples (interleaved)
  float volume = 1.0f;
  bool loop = false;
  size_t loop_start = 0;  // where a loop goes back to (interleaved samples)
  float fade_per_frame = 0.0f;  // volume lost per output frame while fading
};

std::mutex g_mutex;
std::vector<Voice> g_voices;  // one-shots
Voice g_music;
std::atomic<bool> g_running{false};
std::atomic<bool> g_stopping{false};
std::thread g_thread;
int32_t g_port = -1;

void MixInto(Voice& v, float* out, bool& done) {
  const auto& s = *v.clip;
  for (uint32_t i = 0; i < kFrames * 2; ++i) {
    if (v.pos >= s.size()) {
      if (!v.loop || s.empty()) {
        done = true;
        return;
      }
      v.pos = v.loop_start < s.size() ? v.loop_start : 0;
    }
    out[i] += s[v.pos++] * v.volume;
  }
  if (v.fade_per_frame > 0.0f) {
    v.volume -= v.fade_per_frame;
    if (v.volume <= 0.0f)
      done = true;
  }
}

void MixerThread() {
  float mix[kFrames * 2];
  int16_t pcm[kFrames * 2];
  while (g_running.load(std::memory_order_acquire)) {
    std::memset(mix, 0, sizeof(mix));
    bool idle;
    {
      std::lock_guard lock(g_mutex);
      for (auto it = g_voices.begin(); it != g_voices.end();) {
        bool done = false;
        MixInto(*it, mix, done);
        it = done ? g_voices.erase(it) : it + 1;
      }
      if (g_music.clip) {
        bool done = false;
        MixInto(g_music, mix, done);
        if (done)
          g_music = Voice();
      }
      idle = g_voices.empty() && !g_music.clip;
    }
    if (idle && g_stopping.load(std::memory_order_acquire))
      break;
    for (uint32_t i = 0; i < kFrames * 2; ++i)
      pcm[i] = int16_t(std::clamp(mix[i], -1.0f, 1.0f) * 32767.0f);
    // Blocks until the port takes the frame: this is the thread's clock.
    sceAudioOutOutput(g_port, pcm);
  }
}

// Little-endian readers for the WAV header.
uint32_t U32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
uint16_t U16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }

}  // namespace

bool Start() {
  if (g_running.load())
    return true;
  sceAudioOutInit();  // (already initialized by the SDK is fine)
  g_port = sceAudioOutOpen(kSystemUser, kPortTypeMain, 0, kFrames, kRate, kFormatS16Stereo);
  if (g_port < 0) {
    REXLOG_WARN("PS5 sound: no audio port for the menus ({:#x})", uint32_t(g_port));
    return false;
  }
  g_stopping = false;
  g_running = true;
  g_thread = std::thread(MixerThread);
  return true;
}

void Stop() {
  if (!g_running.load())
    return;
  g_stopping = true;
  {
    std::lock_guard lock(g_mutex);
    g_music = Voice();
  }
  // The thread leaves once the last one-shot has played (a confirm sound on
  // START GAME is heard to its end), or is told to after half a second.
  for (int i = 0; i < 50 && g_thread.joinable(); ++i) {
    {
      std::lock_guard lock(g_mutex);
      if (g_voices.empty())
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  g_running = false;
  if (g_thread.joinable())
    g_thread.join();
  sceAudioOutOutput(g_port, nullptr);
  sceAudioOutClose(g_port);
  g_port = -1;
  std::lock_guard lock(g_mutex);
  g_voices.clear();
}

void Play(const Clip& clip, float volume) {
  if (!clip || !g_running.load())
    return;
  std::lock_guard lock(g_mutex);
  // Quick presses overlap rather than cut each other off, but not without
  // limit: a held d-pad repeating through a list must not pile up voices.
  if (g_voices.size() >= 8)
    g_voices.erase(g_voices.begin());
  Voice v;
  v.clip = clip;
  v.volume = volume;
  g_voices.push_back(std::move(v));
}

Clip LoadWav(const std::filesystem::path& wav, size_t* loop_start) {
  if (loop_start)
    *loop_start = 0;
  std::ifstream in(wav, std::ios::binary);
  if (!in)
    return nullptr;
  std::vector<uint8_t> f((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (f.size() < 44 || std::memcmp(f.data(), "RIFF", 4) || std::memcmp(f.data() + 8, "WAVE", 4))
    return nullptr;
  uint16_t format = 0, channels = 0, bits = 0;
  uint32_t rate = 0;
  const uint8_t* data = nullptr;
  size_t data_size = 0;
  uint32_t smpl_loop_start = 0;  // in source frames
  for (size_t p = 12; p + 8 <= f.size();) {
    const uint32_t size = U32(&f[p + 4]);
    if (!std::memcmp(&f[p], "fmt ", 4) && size >= 16 && p + 8 + 16 <= f.size()) {
      format = U16(&f[p + 8]);
      channels = U16(&f[p + 10]);
      rate = U32(&f[p + 12]);
      bits = U16(&f[p + 22]);
    } else if (!std::memcmp(&f[p], "data", 4)) {
      data = &f[p + 8];
      data_size = std::min<size_t>(size, f.size() - (p + 8));
    } else if (!std::memcmp(&f[p], "smpl", 4) && size >= 60 && p + 8 + 60 <= f.size() && U32(&f[p + 8 + 28]) > 0) {
      // sampler header (36 bytes), then the first loop: id, type, start, end...
      smpl_loop_start = U32(&f[p + 8 + 36 + 8]);
    }
    p += 8 + size + (size & 1);
  }
  const bool pcm16 = format == 1 && bits == 16;
  const bool float32 = format == 3 && bits == 32;
  if (!data || !(pcm16 || float32) || (channels != 1 && channels != 2) || (rate != 48000 && rate != 44100))
    return nullptr;
  const size_t frames = data_size / (channels * (bits / 8));
  auto sample = [&](size_t frame, int ch) -> float {
    const size_t i = frame * channels + std::min<int>(ch, channels - 1);
    if (pcm16)
      return int16_t(U16(data + i * 2)) / 32768.0f;
    float v;
    std::memcpy(&v, data + i * 4, 4);
    return v;
  };
  // 44.1 kHz is resampled to the port's 48 kHz by linear interpolation:
  // plenty for menu music, and it keeps this free of a resampler library.
  const double step = double(rate) / kRate;
  const size_t out_frames = size_t(double(frames) / step);
  auto out = std::make_shared<std::vector<float>>(out_frames * 2);
  for (size_t i = 0; i < out_frames; ++i) {
    const double src = i * step;
    const size_t a = std::min(size_t(src), frames - 1), b = std::min(a + 1, frames - 1);
    const float t = float(src - double(a));
    for (int ch = 0; ch < 2; ++ch)
      (*out)[i * 2 + ch] = sample(a, ch) * (1 - t) + sample(b, ch) * t;
  }
  if (loop_start && smpl_loop_start < frames)
    *loop_start = size_t(double(smpl_loop_start) / step) * 2;
  return out;
}

bool PlayMusic(const std::filesystem::path& wav, float volume) {
  size_t loop_start = 0;
  Clip clip = LoadWav(wav, &loop_start);
  if (!clip) {
    REXLOG_WARN("PS5 sound: cannot play {} (WAV, 16-bit or float, 44.1/48 kHz)", wav.string());
    return false;
  }
  std::lock_guard lock(g_mutex);
  g_music = Voice();
  g_music.clip = std::move(clip);
  g_music.volume = volume;
  g_music.loop = true;
  g_music.loop_start = loop_start;
  return true;
}

void SetMusicVolume(float volume) {
  std::lock_guard lock(g_mutex);
  if (g_music.clip && g_music.fade_per_frame == 0.0f)
    g_music.volume = volume;
}

void FadeMusic(float seconds) {
  std::lock_guard lock(g_mutex);
  if (g_music.clip)
    g_music.fade_per_frame = g_music.volume / std::max(1.0f, seconds * kRate / kFrames);
}

}  // namespace ng2ps5::sound
