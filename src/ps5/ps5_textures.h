// Ninja Gaiden II for PS5 - the game's textures as they load (ps5_textures.cpp).
//
// One hook in the SDK's texture cache (rex/graphics/texture_load_hook.h) sees
// each texture's guest bytes just before the GPU copy is made from them. Two
// things use it:
//
//   - the texture dump, in exactly the format the PC recompilation's texture
//     tools read (dump/tex_<id>-<hash>.bin and dump/index.txt), so a pack is
//     made on a PC from a dump taken on the PS5 or on the PC alike;
//   - the PlayStation button prompts (ps5_button_prompts.h), which write the
//     PlayStation picture over the Xbox one in guest memory.
//
// Loading a pack (replacing a texture with a larger one) is NOT in this
// build: the Vulkan texture cache sizes each texture from the guest's key and
// converts it on the GPU, so a replacement needs its own image, upload and
// view path. The setting is kept and says so in the hub.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ng2ps5::textures {

struct Config {
  bool dump = false;               // write every art texture once
  std::filesystem::path dump_dir;  // <texture folder>/dump
  bool prompts = true;             // PlayStation button pictures
  bool dump_hud_pictures = false;  // the small pictures, for finding the prompts
};

// Installs the hook (once) and applies `config`. Call before the game starts;
// calling again later changes what the hook does from the next texture on.
void Configure(const Config& config);

// Writes a file on a thread of its own: the hook runs on the GPU thread, and a
// file write there per texture starves the ring buffer (NG2 T8).
void WriteFileAsync(std::filesystem::path path, std::vector<uint8_t> bytes);
// Appends one line to a text file, on the same thread and in order.
void AppendLineAsync(std::filesystem::path path, std::string line);

// The pack tools' id and content hash (TEXTURE_PACK.md).
uint64_t PackId(uint32_t base_address, uint32_t format, uint32_t tiled, uint32_t width, uint32_t height);
uint32_t Crc32(const uint8_t* data, size_t size);

}  // namespace ng2ps5::textures
