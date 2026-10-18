// Ninja Gaiden II for PS5 - PlayStation button prompts (ps5_button_prompts.h).

#include "ps5/ps5_button_prompts.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <vector>

#include <rex/graphics/pipeline/texture/util.h>
#include <rex/graphics/texture_load_hook.h>
#include <rex/logging.h>

#include "ps5/ps5_paths.h"
#include "ps5/ps5_textures.h"

namespace fs = std::filesystem;

namespace ng2ps5::prompts {

namespace {

// Button pictures are small; the size filter keeps the hash off the GPU
// thread for everything else (a 4 MB texture is not a button). The game's 2D
// HUD is drawn from 8-bit index pictures (k_8) looked up in a 256x1 colour
// table, with text and icons laid out in wide strips, so those may be larger.
constexpr uint32_t kMaxSide = 256;
constexpr uint32_t kMaxBytes = 256 * 1024;
constexpr uint32_t kMaxIndexedSide = 1024;
constexpr uint32_t kMaxIndexedBytes = 1024 * 1024;

bool Considered(const rex::graphics::GuestTextureLoad& t) {
  if (t.dimension != 1 || t.base_size == 0)
    return false;
  if (t.format == 2)
    return t.width <= kMaxIndexedSide && t.height <= kMaxIndexedSide && t.base_size <= kMaxIndexedBytes;
  return t.width <= kMaxSide && t.height <= kMaxSide && t.base_size <= kMaxBytes;
}

std::mutex g_mutex;
fs::path g_dir;
std::set<uint64_t> g_available;                                // hashes with a file
// Whether any replacement may be in a format other than k_8: a .dds or .bin
// file (a .rect patches k_8 index pictures only). Without one, and with the
// HUD dump off, only k_8 loads are hashed - the GPU thread's profile had this
// hook at ~8% of its time in heavy scenes, hashing every small DXT texture.
bool g_other_formats = false;
std::map<uint64_t, std::optional<std::vector<uint8_t>>> g_converted;  // per hash, once
std::set<uint64_t> g_dumped;
// Distinct pictures dumped per guest address: an address whose picture keeps
// changing (a movie's frames, decoded into the same k_8 planes every frame)
// stops being dumped after a few, or the dump fills with video.
std::map<uint32_t, int> g_dumped_per_address;
constexpr int kMaxDumpsPerAddress = 16;  // (HUD memory is reused between screens too)
bool g_logged_first = false;

struct BlockInfo {
  uint32_t width = 0;  // texels per block side (1 or 4)
  uint32_t bytes = 0;  // bytes per block
  const char* fourcc = nullptr;
};

BlockInfo Block(uint32_t format) {
  switch (format) {
    case 18: return {4, 8, "DXT1"};
    case 19: return {4, 16, "DXT3"};
    case 20: return {4, 16, "DXT5"};
    case 2: return {1, 1, nullptr};  // k_8, the HUD's index pictures (.bin only)
    case 6:
    case 14: return {1, 4, nullptr};
    case 3:
    case 4:
    case 15: return {1, 2, nullptr};
    default: return {};
  }
}

uint64_t Fnv1a64(const uint8_t* p, size_t n) {
  uint64_t h = 0xCBF29CE484222325ull;
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 0x100000001B3ull;
  }
  return h;
}

// The guest's byte order for one block, applied in place. Each swap is its
// own inverse, so applying the texture's endianness to a little-endian block
// gives the bytes the loader will swap back.
void SwapEndian(uint8_t* p, uint32_t bytes, uint32_t endianness) {
  switch (endianness) {
    case 1:  // k8in16
      for (uint32_t i = 0; i + 1 < bytes; i += 2) std::swap(p[i], p[i + 1]);
      break;
    case 2:  // k8in32
      for (uint32_t i = 0; i + 3 < bytes; i += 4) {
        std::swap(p[i], p[i + 3]);
        std::swap(p[i + 1], p[i + 2]);
      }
      break;
    case 3:  // k16in32
      for (uint32_t i = 0; i + 3 < bytes; i += 4) {
        std::swap(p[i], p[i + 2]);
        std::swap(p[i + 1], p[i + 3]);
      }
      break;
    default:
      break;
  }
}

std::vector<uint8_t> ReadFile(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

uint32_t Le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }

uint16_t Le16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }

// A .rect file: rectangles of new texels pasted over the game's own picture,
// so a picture that is mostly fine (the button atlas keeps the game's sticks
// and d-pad) changes only where this project drew something. Only the drawn
// texels ship. Layout, little-endian: "NG2R", u32 version (1), u32 count, then
// per rectangle u16 x, y, w, h and w*h texels, row by row. One byte per texel
// (the HUD's k_8 index pictures: the values are indices into the picture's
// own colour table, which stays the game's).
std::optional<std::vector<uint8_t>> ApplyRects(const fs::path& path, const rex::graphics::GuestTextureLoad& t,
                                               const uint8_t* guest) {
  const auto file = ReadFile(path);
  const BlockInfo block = Block(t.format);
  if (block.width != 1 || block.bytes != 1) {
    REXLOG_WARN("PS5 prompts: {} needs a one-byte-per-texel picture; the Xbox picture is format {}",
                path.filename().string(), t.format);
    return std::nullopt;
  }
  if (file.size() < 12 || std::memcmp(file.data(), "NG2R", 4) || Le32(&file[4]) != 1) {
    REXLOG_WARN("PS5 prompts: {} is not a version 1 .rect file", path.filename().string());
    return std::nullopt;
  }
  const uint32_t pitch = std::max(t.pitch * 32, t.width);
  std::vector<uint8_t> out(guest, guest + t.base_size);
  size_t at = 12;
  for (uint32_t r = 0, count = Le32(&file[8]); r < count; ++r) {
    if (at + 8 > file.size())
      return std::nullopt;
    const uint32_t x0 = Le16(&file[at]), y0 = Le16(&file[at + 2]), w = Le16(&file[at + 4]), h = Le16(&file[at + 6]);
    at += 8;
    if (x0 + w > t.width || y0 + h > t.height || at + size_t(w) * h > file.size()) {
      REXLOG_WARN("PS5 prompts: {} has a rectangle outside the {}x{} picture", path.filename().string(),
                  t.width, t.height);
      return std::nullopt;
    }
    for (uint32_t y = 0; y < h; ++y) {
      for (uint32_t x = 0; x < w; ++x) {
        const size_t dst = t.tiled ? size_t(rex::graphics::texture_util::GetTiledOffset2D(
                                         int32_t(x0 + x), int32_t(y0 + y), pitch, 0))
                                   : size_t(y0 + y) * pitch + (x0 + x);
        if (dst >= out.size())
          return std::nullopt;
        out[dst] = file[at + size_t(y) * w + x];
      }
    }
    at += size_t(w) * h;
  }
  return out;
}

// The PlayStation picture for `hash`, laid out as the Xbox picture is in
// guest memory. Empty when the file is missing or does not match the Xbox
// picture's shape - said once in the log, never guessed at.
std::optional<std::vector<uint8_t>> Convert(uint64_t hash, const rex::graphics::GuestTextureLoad& t,
                                            const uint8_t* guest) {
  char name[32];
  std::snprintf(name, sizeof(name), "%016llX", (unsigned long long)hash);
  const fs::path bin = g_dir / (std::string(name) + ".bin");
  std::error_code ec;
  const fs::path rect = g_dir / (std::string(name) + ".rect");
  if (fs::is_regular_file(rect, ec))
    return ApplyRects(rect, t, guest);
  if (fs::is_regular_file(bin, ec)) {
    auto bytes = ReadFile(bin);
    if (bytes.size() != t.base_size) {
      REXLOG_WARN("PS5 prompts: {} is {} bytes, the Xbox picture is {}; not used", bin.filename().string(),
                  bytes.size(), t.base_size);
      return std::nullopt;
    }
    return bytes;
  }

  const fs::path dds_path = g_dir / (std::string(name) + ".dds");
  const auto dds = ReadFile(dds_path);
  const BlockInfo block = Block(t.format);
  if (dds.size() < 128 || std::memcmp(dds.data(), "DDS ", 4) || !block.fourcc) {
    REXLOG_WARN("PS5 prompts: {} is not a DDS this build can use (the Xbox picture is format {})",
                dds_path.filename().string(), t.format);
    return std::nullopt;
  }
  const uint32_t height = Le32(&dds[12]), width = Le32(&dds[16]);
  if (width != t.width || height != t.height || std::memcmp(&dds[84], block.fourcc, 4)) {
    REXLOG_WARN("PS5 prompts: {} is {}x{} {:.4}, the Xbox picture is {}x{} {}", dds_path.filename().string(),
                width, height, reinterpret_cast<const char*>(&dds[84]), t.width, t.height, block.fourcc);
    return std::nullopt;
  }
  const uint32_t bw = (t.width + block.width - 1) / block.width;
  const uint32_t bh = (t.height + block.width - 1) / block.width;
  if (dds.size() < 128 + size_t(bw) * bh * block.bytes)
    return std::nullopt;

  // TextureKey::pitch is in 32-texel units; the tiling wants blocks.
  const uint32_t pitch_texels = std::max(t.pitch * 32, t.width);
  const uint32_t pitch_blocks = std::max(1u, pitch_texels / block.width);
  uint32_t bpb_log2 = 0;
  while ((1u << bpb_log2) < block.bytes) ++bpb_log2;

  std::vector<uint8_t> out(t.base_size, 0);
  const uint8_t* src = dds.data() + 128;
  for (uint32_t by = 0; by < bh; ++by) {
    for (uint32_t bx = 0; bx < bw; ++bx) {
      const size_t dst = t.tiled ? size_t(rex::graphics::texture_util::GetTiledOffset2D(
                                       int32_t(bx), int32_t(by), pitch_blocks, bpb_log2))
                                 : size_t(by) * pitch_blocks * block.bytes + size_t(bx) * block.bytes;
      if (dst + block.bytes > out.size())
        return std::nullopt;
      std::memcpy(&out[dst], src + (size_t(by) * bw + bx) * block.bytes, block.bytes);
      SwapEndian(&out[dst], block.bytes, t.endianness);
    }
  }
  return out;
}

}  // namespace

void Init(const fs::path& app) {
  std::lock_guard lock(g_mutex);
  g_dir = app / "hub" / "button_prompts" / "playstation";
  g_available.clear();
  g_converted.clear();
  g_other_formats = false;
  std::error_code ec;
  for (fs::directory_iterator it(g_dir, ec), end; !ec && it != end; it.increment(ec)) {
    const auto stem = it->path().stem().string();
    const auto ext = it->path().extension().string();
    if (stem.size() != 16 || (ext != ".dds" && ext != ".bin" && ext != ".rect"))
      continue;
    g_available.insert(std::strtoull(stem.c_str(), nullptr, 16));
    if (ext != ".rect")
      g_other_formats = true;
  }
  REXLOG_INFO("PS5 prompts: {} PlayStation picture(s) in {}", g_available.size(), g_dir.string());
}

// A movie decodes into the same k_8 planes some 30 times a second; no HUD
// picture reloads like that. An address loaded more than 8 times within 2 s is
// treated as video and not hashed - until it has been quiet for 5 s.
static bool IsVideo(uint32_t address) {
  struct Rate {
    std::chrono::steady_clock::time_point window, last;
    int loads = 0;
    bool video = false;
  };
  static std::map<uint32_t, Rate> rates;
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard lock(g_mutex);
  Rate& r = rates[address];
  if (r.video && now - r.last > std::chrono::seconds(5))
    r.video = false;
  r.last = now;
  if (now - r.window > std::chrono::seconds(2)) {
    r.window = now;
    r.loads = 0;
  }
  if (++r.loads > 8)
    r.video = true;
  return r.video;
}

void OnTextureLoad(const rex::graphics::GuestTextureLoad& t, uint8_t* guest, bool replace, bool dump) {
  if (!dump && (!replace || (t.format != 2 && !g_other_formats)))
    return;
  if (!Considered(t) || !Block(t.format).bytes)
    return;
  if (t.format == 2 && IsVideo(t.base_address))
    return;
  const uint64_t hash = Fnv1a64(guest, t.base_size);

  if (dump) {
    std::lock_guard lock(g_mutex);
    if (!g_dumped.count(hash) && ++g_dumped_per_address[t.base_address] <= kMaxDumpsPerAddress) {
      g_dumped.insert(hash);
      // The address pairs an index picture with its colour table, which the
      // game keeps in the next page after it.
      char name[112];
      std::snprintf(name, sizeof(name), "%016llX_%ux%u_f%u_t%u_e%u_p%u_m%u_a%08X.bin", (unsigned long long)hash,
                    t.width, t.height, t.format, t.tiled, t.endianness, t.pitch, t.mip_max_level,
                    t.base_address);
      textures::WriteFileAsync(paths::Logs() / "hud_pictures" / name,
                               std::vector<uint8_t>(guest, guest + t.base_size));
    }
  }

  if (!replace)
    return;
  std::lock_guard lock(g_mutex);
  if (!g_available.count(hash))
    return;
  if (t.mip_max_level != 0) {
    // The hook sees the base level only; a picture with smaller levels would
    // show the Xbox picture again from a distance. Button icons have none.
    if (g_converted.emplace(hash, std::nullopt).second)
      REXLOG_WARN("PS5 prompts: {:016X} has mipmaps; not replaced", hash);
    return;
  }
  auto it = g_converted.find(hash);
  if (it == g_converted.end())
    it = g_converted.emplace(hash, Convert(hash, t, guest)).first;
  if (!it->second || it->second->size() != t.base_size)
    return;
  std::memcpy(guest, it->second->data(), t.base_size);
  if (!g_logged_first) {
    g_logged_first = true;
    REXLOG_INFO("PS5 prompts: first PlayStation picture in place ({:016X})", hash);
  }
}

}  // namespace ng2ps5::prompts
