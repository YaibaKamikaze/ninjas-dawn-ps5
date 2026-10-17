// Ninja Gaiden II for PS5 - the hub on the console (ps5_hub.cpp): shown from
// ReXApp::OnFinalizePaths, where NG2's PC setup screen was, before the runtime
// is built; START GAME resumes the start.
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "ng2_settings.h"

struct ImFontAtlas;
namespace rex::ui {
class ImGuiDrawer;
}

namespace ng2ps5::hub {

struct ShowOptions {
  Ng2Settings* settings = nullptr;
  std::string version;
  std::function<std::string()> write_diagnostics;
  // On the UI thread, after the hub has gone: START GAME (with the save
  // packages to import, possibly none) or QUIT.
  std::function<void(std::vector<std::filesystem::path> import_saves)> start;
  std::function<void()> quit;
};

// The dialog owns itself and closes itself.
void Show(rex::ui::ImGuiDrawer* drawer, ShowOptions options);

}  // namespace ng2ps5::hub
