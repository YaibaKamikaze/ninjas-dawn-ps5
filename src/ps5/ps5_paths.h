// Ninja Gaiden II for PS5 - the app folder's layout (docs/PS5_PORT.md section 4).
//
// The homebrew launcher mounts the app's folder as /app0, writable, and the
// SDK's GetExecutableFolder() answers /app0. Everything the player copies in
// and everything the app writes lives there, so the whole install is one
// folder that can be backed up or moved over FTP:
//
//   Game Data/       the player's extracted disc (default.xex at its root)
//   DLC/             STFS packages, optional
//   Saves Import/    Xbox 360 save packages to import, optional
//   Texture Packs/   packs made on a PC, and the dump folder, optional
//   UserData/        settings, profile, saves, installed DLC, shader cache
//   logs/            the runtime's logs, diagnostics, HUD picture dumps
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ng2ps5::paths {

std::filesystem::path App();
std::filesystem::path GameData();
std::filesystem::path Dlc();
std::filesystem::path SavesImport();
std::filesystem::path TexturePacks();
// Where the app writes: the app folder, or /data/ng2recomp when that
// cannot be written (see ps5_paths.cpp).
std::filesystem::path WritableRoot();
std::filesystem::path UserData();
std::filesystem::path Cache();    // UserData/cache: shader storage, tuning file
std::filesystem::path Logs();
std::filesystem::path Backups();  // UserData backups made from the Saves page

// Makes the folders above that the app writes to, and the player-facing ones
// so an FTP client shows where things go. Safe to call every launch.
void EnsureFolders();

// What Game Data/ holds, looked at when the hub opens and again on START GAME.
struct GameDataStatus {
  bool folder = false;    // Game Data/ exists
  bool xex = false;       // default.xex at its root
  uint32_t title_id = 0;  // read from default.xex, 0 if unreadable
  int archives = 0;       // *.ng2 archives found beside it
  // One line for the hub: what is wrong and exactly where to copy what.
  std::string message;

  bool RightGame() const;
  bool Ready() const { return xex && RightGame() && archives > 0; }
};
GameDataStatus CheckGameData();

// DLC/ and Saves Import/ contents, for the hub's pages.
struct DlcPackage {
  std::filesystem::path path;
  std::string name;  // the package's own display name, or its file name
  bool valid = false;  // an STFS package (LIVE/PIRS/CON)
};
std::vector<DlcPackage> ListDlc();

}  // namespace ng2ps5::paths
