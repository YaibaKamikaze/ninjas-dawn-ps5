// Ninja Gaiden II for PS5 - the hub on the console (ps5_hub.h).

#include "ps5/hub/ps5_hub.h"

#include <algorithm>
#include <chrono>
#include <ctime>

#include <imgui.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include "ng2_saveimport.h"
#include "ps5/hub/hub_pages.h"
#include "ps5/ps5_paths.h"
#include "ps5/ps5_sound.h"

namespace fs = std::filesystem;

namespace ng2ps5::hub {

namespace {

// A copy of UserData in Backups/UserData-<date>-<time>, without the shader
// cache: that is rebuilt by playing and is most of the folder's size.
std::string BackupUserData() {
  const std::time_t now = std::time(nullptr);
  char stamp[32];
  std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&now));
  const fs::path dest = paths::Backups() / (std::string("UserData-") + stamp);
  std::error_code ec;
  fs::create_directories(dest, ec);
  size_t files = 0;
  for (fs::recursive_directory_iterator it(paths::UserData(), ec), end; !ec && it != end; it.increment(ec)) {
    const fs::path rel = fs::relative(it->path(), paths::UserData(), ec);
    if (rel.begin() != rel.end() && *rel.begin() == "cache") {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory(ec)) {
      fs::create_directories(dest / rel, ec);
    } else if (it->is_regular_file(ec)) {
      fs::copy_file(it->path(), dest / rel, fs::copy_options::overwrite_existing, ec);
      if (!ec)
        ++files;
    }
  }
  if (ec)
    return "Backup stopped: " + ec.message();
  REXLOG_INFO("PS5 hub: backed up {} file(s) to {}", files, dest.string());
  return "Backed up " + std::to_string(files) + " file(s) to Backups/UserData-" + stamp;
}

std::vector<SaveRow> ListSaves() {
  std::vector<SaveRow> rows;
  for (const fs::path& p : ng2::FindSavePackages(paths::SavesImport())) {
    const ng2::SaveInfo info = ng2::InspectSavePackage(p);
    SaveRow row;
    row.path = p;
    row.name = info.display_name.empty() ? p.filename().string() : info.display_name;
    row.usable = info.ok && info.right_game && info.is_save;
    row.message = row.usable ? "will import" : info.message;
    rows.push_back(std::move(row));
  }
  return rows;
}

// The first file in hub/music, in name order (WAV: see ps5_sound.h).
fs::path FirstSong(const fs::path& hub) {
  std::vector<fs::path> songs;
  std::error_code ec;
  for (fs::directory_iterator it(hub / "music", ec), end; !ec && it != end; it.increment(ec)) {
    std::string ext = it->path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (ext == ".wav")
      songs.push_back(it->path());
  }
  std::sort(songs.begin(), songs.end());
  return songs.empty() ? fs::path() : songs.front();
}

class HubDialog final : public rex::ui::ImGuiDialog {
 public:
  HubDialog(rex::ui::ImGuiDrawer* drawer, ShowOptions options, HubContext context)
      : ImGuiDialog(drawer), options_(std::move(options)), hub_(std::move(context)) {
    music_on_ = options_.settings->ps5_menu_music;
    if (music_on_) {
      const fs::path song = FirstSong(paths::App() / "hub");
      if (!song.empty())
        sound::PlayMusic(song, 0.5f);
    }
  }

 protected:
  void OnDraw(ImGuiIO& io) override {
    if (done_)
      return;
    const PadState pad = pad_.Read();
    // The music follows its setting at once, so the row can be tried here.
    if (options_.settings->ps5_menu_music != music_on_) {
      music_on_ = options_.settings->ps5_menu_music;
      if (music_on_) {
        const fs::path song = FirstSong(paths::App() / "hub");
        if (song.empty() || !sound::PlayMusic(song, 0.5f))
          REXLOG_INFO("PS5 hub: no WAV in hub/music to play");
      } else {
        sound::FadeMusic(0.4f);
      }
    }
    const Hub::Result result = hub_.Draw(pad, io.DeltaTime, ImGui::GetForegroundDrawList(), io.DisplaySize);
    if (result == Hub::Result::kNone)
      return;
    done_ = true;
    sound::FadeMusic(0.3f);
    std::vector<fs::path> imports;
    if (result == Hub::Result::kStart && hub_.ImportSaves()) {
      for (const SaveRow& row : ListSaves())
        if (row.usable)
          imports.push_back(row.path);
    }
    // Callbacks after Close(): the app tears the hub's world down from them,
    // and this object is deleted at the end of the frame either way.
    auto start = options_.start;
    auto quit = options_.quit;
    Close();
    if (result == Hub::Result::kStart) {
      REXLOG_INFO("PS5 hub: START GAME ({} save(s) to import)", imports.size());
      if (start)
        start(std::move(imports));
    } else {
      REXLOG_INFO("PS5 hub: QUIT");
      if (quit)
        quit();
    }
  }

 private:
  ShowOptions options_;
  Hub hub_;
  Pad pad_;
  bool music_on_ = false;
  bool done_ = false;
};

}  // namespace

void Show(rex::ui::ImGuiDrawer* drawer, ShowOptions options) {
  paths::EnsureFolders();
  InitSounds((paths::App() / "hub").string());
  HubContext ctx;
  ctx.settings = options.settings;
  ctx.game = paths::CheckGameData();
  ctx.dlc = paths::ListDlc();
  ctx.saves = ListSaves();
  ctx.version = options.version;
  ctx.write_diagnostics = options.write_diagnostics;
  ctx.backup = BackupUserData;
  REXLOG_INFO("PS5 hub: shown - {}; {} DLC file(s), {} save package(s)", ctx.game.message, ctx.dlc.size(),
              ctx.saves.size());
  new HubDialog(drawer, std::move(options), std::move(ctx));  // (deletes itself on Close)
}

}  // namespace ng2ps5::hub
