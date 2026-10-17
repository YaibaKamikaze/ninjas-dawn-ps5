// Ninja Gaiden II for PS5 - the game's textures as they load (ps5_textures.h).

#include "ps5/ps5_textures.h"

#include <array>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <deque>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>
#include <utility>

#include <rex/graphics/texture_load_hook.h>
#include <rex/logging.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "ps5/ps5_button_prompts.h"

namespace fs = std::filesystem;

namespace ng2ps5::textures {

namespace {

// --- The writer thread --------------------------------------------------

struct Job {
  fs::path path;
  std::vector<uint8_t> bytes;
  bool append_line = false;
};

std::mutex g_jobs_mutex;
std::condition_variable g_jobs_cv;
std::deque<Job> g_jobs;
bool g_writer_started = false;

void WriterThread() {
  for (;;) {
    Job job;
    {
      std::unique_lock lock(g_jobs_mutex);
      g_jobs_cv.wait(lock, [] { return !g_jobs.empty(); });
      job = std::move(g_jobs.front());
      g_jobs.pop_front();
    }
    std::error_code ec;
    fs::create_directories(job.path.parent_path(), ec);
    std::ofstream out(job.path, job.append_line ? std::ios::binary | std::ios::app
                                                : std::ios::binary | std::ios::trunc);
    if (!out) {
      REXLOG_WARN("PS5 textures: cannot write {}", job.path.string());
      continue;
    }
    out.write(reinterpret_cast<const char*>(job.bytes.data()), std::streamsize(job.bytes.size()));
  }
}

void Enqueue(Job job) {
  std::lock_guard lock(g_jobs_mutex);
  if (!g_writer_started) {
    g_writer_started = true;
    std::thread(WriterThread).detach();
  }
  // A dump of a whole stage is hundreds of megabytes; if the drive falls this
  // far behind, textures are dropped (and seen again next run) rather than
  // held in memory without limit.
  if (g_jobs.size() >= 512) {
    static bool warned = false;
    if (!warned) {
      warned = true;
      REXLOG_WARN("PS5 textures: the dump is behind by 512 files; dropping some until it catches up");
    }
    return;
  }
  g_jobs.push_back(std::move(job));
  g_jobs_cv.notify_one();
}

// --- Configuration and the hook -----------------------------------------

std::mutex g_config_mutex;
Config g_config;
std::set<std::pair<uint64_t, uint32_t>> g_dumped;  // (id, hash) already in the dump
bool g_dumped_loaded = false;

// The formats a shipped asset is stored in. Render targets, the video
// decoder's planes and the HDR scene buffer are left out at the source, as
// the PC plugin does (TexturePackIsArtFormat), so the dump does not fill with
// framebuffers rewritten every frame. The tool filters again, reversibly.
bool IsArtFormat(uint32_t format) {
  switch (format) {
    case 2:   // k_8
    case 3:   // k_1_5_5_5
    case 4:   // k_5_6_5
    case 6:   // k_8_8_8_8
    case 10:  // k_8_8
    case 14:  // k_8_8_8_8_A
    case 15:  // k_4_4_4_4
    case 18:  // k_DXT1
    case 19:  // k_DXT2_3
    case 20:  // k_DXT4_5
      return true;
    default:
      return false;
  }
}

// What earlier runs already dumped, so a second visit to a stage writes only
// what is new. Read once, the first time a texture is dumped.
void LoadDumpedIndex(const fs::path& dump_dir) {
  g_dumped_loaded = true;
  std::ifstream in(dump_dir / "index.txt");
  std::string line;
  while (std::getline(in, line)) {
    unsigned long long id = 0;
    unsigned w, h, fmt, tiled, pitch, endian, dim, size, hash = 0;
    if (std::sscanf(line.c_str(), "%llx %u %u %u %u %u %u %u %u %x", &id, &w, &h, &fmt, &tiled, &pitch,
                    &endian, &dim, &size, &hash) == 10)
      g_dumped.emplace(uint64_t(id), uint32_t(hash));
  }
}

void Dump(const rex::graphics::GuestTextureLoad& t, const uint8_t* guest, const fs::path& dump_dir) {
  if (t.dimension != 1 || !IsArtFormat(t.format) || t.base_size == 0 || t.base_size > (32u << 20))
    return;
  const uint64_t id = PackId(t.base_address, t.format, t.tiled, t.width, t.height);
  const uint32_t hash = Crc32(guest, t.base_size);
  {
    std::lock_guard lock(g_config_mutex);
    if (!g_dumped_loaded)
      LoadDumpedIndex(dump_dir);
    if (!g_dumped.emplace(id, hash).second)
      return;
  }
  char name[64];
  std::snprintf(name, sizeof(name), "tex_%016llX-%08X.bin", (unsigned long long)id, hash);
  // The index line the tool reads: id w h fmt tiled pitch endian dim size hash.
  char line[160];
  std::snprintf(line, sizeof(line), "%016llX %u %u %u %u %u %u %u %u %08X\n", (unsigned long long)id, t.width,
                t.height, t.format, t.tiled, t.pitch, t.endianness, t.dimension, t.base_size, hash);
  Job file;
  file.path = dump_dir / name;
  file.bytes.assign(guest, guest + t.base_size);
  Enqueue(std::move(file));
  Job index;
  index.path = dump_dir / "index.txt";
  index.bytes.assign(line, line + std::strlen(line));
  index.append_line = true;
  Enqueue(std::move(index));
}

void OnTextureLoad(const rex::graphics::GuestTextureLoad& t) {
  auto* kernel = REX_KERNEL_STATE();
  auto* memory = kernel ? kernel->memory() : nullptr;
  if (!memory)
    return;
  uint8_t* guest = memory->TranslatePhysical<uint8_t*>(t.base_address);
  Config config;
  {
    std::lock_guard lock(g_config_mutex);
    config = g_config;
  }
  // Prompts first: when a picture is replaced, the dump records what the game
  // will actually draw from now on.
  if (config.prompts || config.dump_hud_pictures)
    prompts::OnTextureLoad(t, guest, config.prompts, config.dump_hud_pictures);
  if (config.dump && !config.dump_dir.empty())
    Dump(t, guest, config.dump_dir);
}

}  // namespace

void Configure(const Config& config) {
  {
    std::lock_guard lock(g_config_mutex);
    if (config.dump_dir != g_config.dump_dir) {
      g_dumped.clear();
      g_dumped_loaded = false;
    }
    g_config = config;
  }
#if !defined(NG2_PS5_HOST_TEST)  // (the host test's GPU plugin is a separate library)
  rex::graphics::SetGuestTextureLoadHook(&OnTextureLoad);
#endif
  REXLOG_INFO("PS5 textures: dump {}{}, PlayStation prompts {}, HUD picture dump {}",
              config.dump ? "on -> " : "off", config.dump ? config.dump_dir.string() : std::string(),
              config.prompts ? "on" : "off", config.dump_hud_pictures ? "on" : "off");
}

void WriteFileAsync(fs::path path, std::vector<uint8_t> bytes) {
  Job job;
  job.path = std::move(path);
  job.bytes = std::move(bytes);
  Enqueue(std::move(job));
}

void AppendLineAsync(fs::path path, std::string line) {
  Job job;
  job.path = std::move(path);
  job.bytes.assign(line.begin(), line.end());
  job.append_line = true;
  Enqueue(std::move(job));
}

// The PC plugin's TexturePackId, read back from the ids in its dumps
// (docs/texpack/sample_index.txt): the base page above bit 40, the format and
// the tiled bit in bits 32-39, then width and height. Endianness and pitch are
// not part of it - textures differing only there share an id, and the content
// hash beside it tells them apart.
uint64_t PackId(uint32_t base_address, uint32_t format, uint32_t tiled, uint32_t width, uint32_t height) {
  return uint64_t(base_address >> 12) << 40 | uint64_t((format << 1) | (tiled & 1)) << 32 |
         uint64_t(width & 0xFFFF) << 16 | uint64_t(height & 0xFFFF);
}

// CRC-32 with the zlib polynomial, as the plugin and the tool (zlib.crc32)
// compute it.
uint32_t Crc32(const uint8_t* data, size_t size) {
  static const auto table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k)
        c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      t[i] = c;
    }
    return t;
  }();
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < size; ++i)
    crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFFu;
}

}  // namespace ng2ps5::textures
