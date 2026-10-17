// Ninja Gaiden II for PS5 - the app folder's layout (ps5_paths.h).

#include "ps5/ps5_paths.h"

#include <algorithm>
#include <cstdio>
#include <fstream>

#include <rex/filesystem.h>
#include <rex/logging.h>

#include "ng2_disc.h"

namespace fs = std::filesystem;

namespace ng2ps5::paths {

fs::path App() { return rex::filesystem::GetExecutableFolder(); }
fs::path GameData() { return App() / "Game Data"; }
fs::path Dlc() { return App() / "DLC"; }
fs::path SavesImport() { return App() / "Saves Import"; }
fs::path TexturePacks() { return App() / "Texture Packs"; }
namespace {

// Whether `dir` can be written: made if missing, then a file created in it.
bool CanWrite(const fs::path& dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  const fs::path probe = dir / ".write_test";
  bool ok = false;
  if (FILE* f = std::fopen(probe.c_str(), "wb")) {
    ok = std::fputc('1', f) != EOF;
    ok = (std::fclose(f) == 0) && ok;
  }
  fs::remove(probe, ec);
  return ok;
}

}  // namespace

// The app folder is normally writable (the launcher mounts it so), but an
// install whose folder came over with read-only permissions, or a launcher
// that mounts it read-only, would leave the settings, the shader cache and
// the saves with nowhere to go. Then they live in /data/ng2recomp instead -
// decided once, said in the log.
fs::path WritableRoot() {
  static const fs::path root = [] {
    if (CanWrite(App() / "UserData"))
      return App();
    const fs::path fallback = "/data/ng2recomp";
    if (CanWrite(fallback / "UserData")) {
      REXLOG_WARN("PS5: {} cannot be written; settings, cache, saves and logs go to {}",
                  (App() / "UserData").string(), fallback.string());
      return fallback;
    }
    REXLOG_ERROR("PS5: neither {} nor {} can be written; settings will not be saved", App().string(),
                 fallback.string());
    return App();
  }();
  return root;
}

fs::path UserData() { return WritableRoot() / "UserData"; }
fs::path Cache() { return UserData() / "cache"; }
fs::path Logs() { return WritableRoot() / "logs"; }
fs::path Backups() { return WritableRoot() / "Backups"; }

void EnsureFolders() {
  // Every path here is a plain directory under /app0. An error is logged and
  // otherwise ignored: a folder that cannot be made shows up later as the
  // specific thing that could not be written, which says more than failing
  // here would.
  for (const fs::path& dir : {GameData(), Dlc(), SavesImport(), TexturePacks(), UserData(),
                              Cache(), Cache() / "shaders", Logs()}) {
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec)
      REXLOG_WARN("PS5: cannot create {} ({})", dir.string(), ec.message());
  }
}

bool GameDataStatus::RightGame() const { return title_id == ng2::kNg2TitleId; }

GameDataStatus CheckGameData() {
  GameDataStatus s;
  std::error_code ec;
  const fs::path folder = GameData();
  s.folder = fs::is_directory(folder, ec);
  if (!s.folder) {
    s.message = "Copy your Ninja Gaiden II disc into the Game Data folder of this app.";
    return s;
  }
  const ng2::DiscInfo info = ng2::InspectFolder(folder);
  s.xex = info.has_xex;
  s.title_id = info.title_id;
  for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec))
      continue;
    std::string ext = it->path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (ext == ".ng2")
      ++s.archives;
  }
  // The order matters: each message names the first thing to fix, and fixing
  // it is what reveals the next one.
  if (!s.xex) {
    s.message = "Game Data has no default.xex. Copy the disc's files directly into Game Data, "
                "not into a folder inside it.";
  } else if (!s.RightGame()) {
    char id[16];
    std::snprintf(id, sizeof(id), "%08X", s.title_id);
    s.message = s.title_id ? std::string("The default.xex in Game Data is another game (title ") + id +
                                 "); Ninja Gaiden II is 544307D5."
                           : "The default.xex in Game Data cannot be read. Copy it again.";
  } else if (s.archives == 0) {
    s.message = "Game Data has default.xex but none of the game's .ng2 archives. Copy the whole disc.";
  } else {
    s.message = "Ninja Gaiden II found (" + std::to_string(s.archives) + " archives).";
  }
  return s;
}

namespace {

// The package's display name: UTF-16BE at 0x411 in the STFS header, the name
// the dashboard shows ("Fiend Challenges", ...). ASCII is all these names use;
// anything else is replaced rather than decoded badly.
std::string StfsDisplayName(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return {};
  in.seekg(0x411);
  char raw[256] = {};
  in.read(raw, sizeof(raw));
  std::string name;
  for (size_t i = 0; i + 1 < size_t(in.gcount()); i += 2) {
    const uint16_t c = uint16_t(uint8_t(raw[i]) << 8 | uint8_t(raw[i + 1]));
    if (c == 0)
      break;
    name += (c >= 0x20 && c < 0x7F) ? char(c) : '?';
  }
  return name;
}

}  // namespace

std::vector<DlcPackage> ListDlc() {
  std::vector<DlcPackage> out;
  std::error_code ec;
  // Recursive, because a copy straight from a console keeps the
  // <title id>/00000002/ folders and that should work as it is.
  for (fs::recursive_directory_iterator it(Dlc(), ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec))
      continue;
    const fs::path& p = it->path();
    if (p.filename() == "README.txt" || p.filename().string().front() == '.')
      continue;
    DlcPackage pkg;
    pkg.path = p;
    pkg.valid = ng2::IsStfsPackage(p);
    pkg.name = pkg.valid ? StfsDisplayName(p) : std::string();
    if (pkg.name.empty())
      pkg.name = p.filename().string();
    out.push_back(std::move(pkg));
  }
  std::sort(out.begin(), out.end(), [](const DlcPackage& a, const DlcPackage& b) { return a.name < b.name; });
  return out;
}

}  // namespace ng2ps5::paths
