// Ninja Gaiden II for PS5 - the hub's pages (hub_pages.cpp): START GAME,
// SETTINGS, DOWNLOADABLE CONTENT, SAVES, ABOUT and QUIT, as in docs/PS5_PORT.md 5.4.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ps5/hub/hub_ui.h"
#include "ps5/ps5_paths.h"

namespace ng2ps5::hub {

struct SaveRow {
  std::filesystem::path path;
  std::string name;
  std::string message;
  bool usable = false;
};

struct HubContext {
  Ng2Settings* settings = nullptr;
  paths::GameDataStatus game;
  std::vector<paths::DlcPackage> dlc;
  std::vector<SaveRow> saves;
  std::string version;
  // Writes the diagnostics bundle; returns the line to show.
  std::function<std::string()> write_diagnostics;
  // Copies UserData to a dated folder; returns the line to show.
  std::function<std::string()> backup;
};

class Hub {
 public:
  enum class Result { kNone, kStart, kQuit };

  explicit Hub(HubContext context);
  Result Draw(const PadState& pad, float dt, ImDrawList* dl, const ImVec2& display);
  // The saves the player chose to import on START GAME.
  bool ImportSaves() const { return import_saves_; }

 private:
  enum class Page { kHome, kSettings, kDlc, kSaves, kAbout };

  void Home(const PadState& pad);
  void SettingsPage(const PadState& pad);
  void DlcPage(const PadState& pad);
  void SavesPage(const PadState& pad);
  void AboutPage(const PadState& pad);
  void Hints(std::initializer_list<std::pair<uint32_t, const char*>> hints);
  void PageTitle(const char* title, const std::string& subtitle);
  void Panel(float x, float y, float w, float h);
  void GoTo(Page page);
  void Toast(std::string text);
  void Confirm(std::string title, std::string text, std::function<void()> yes);
  bool ConfirmOpen(const PadState& pad);
  void SetTab(int tab);
  std::string StartCaption() const;

  HubContext ctx_;
  Canvas c_;
  float time_ = 0, dt_ = 0;
  Page page_ = Page::kHome;
  int home_sel_ = 0;
  float home_focus_ = -1;
  float wipe_ = 1;  // the slash across the focused item, 0 -> 1
  float page_in_ = 1;
  int tab_ = 0;
  SettingsList list_;
  int dlc_sel_ = 0, saves_sel_ = 0, about_sel_ = 0;
  bool import_saves_ = false;
  std::string toast_;
  float toast_time_ = 0;
  bool confirm_open_ = false;
  std::string confirm_title_, confirm_text_;
  std::function<void()> confirm_yes_;
  bool leaving_ = false, quitting_ = false;
  float leave_ = 0;
  bool settings_dirty_ = false;
};

}  // namespace ng2ps5::hub
