// [ps5 port] The self-update, on the PS5: nothing.
//
// The PC port checks GitHub and swaps its own files with a PowerShell helper.
// The PS5 app never touches the network, and a homebrew app cannot replace
// its own eboot.bin while it runs: an update is a new app folder copied over
// the old one (ps5/BUILD.md), which keeps Game Data, DLC and UserData. The
// API stays so the rest of the port compiles unchanged; every check reports
// "up to date" without asking anyone, and the launch prompt never draws.

#include "ng2_update.h"

#include <mutex>
#include <utility>

namespace ng2 {
namespace update {
namespace {

std::mutex g_mutex;
Snapshot g_state;

}  // namespace

void Init(const std::string& current_version, const std::filesystem::path&,
          const std::filesystem::path&) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_state.current_version = current_version;
  g_state.latest_version = current_version;
}

void SetApplyHandler(std::function<void()>) {}

void CheckAsync() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_state.phase = Phase::kUpToDate;
}

void BeginUpdate() {}

void ApplyNow() {}

void Dismiss() {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_state.dismissed = true;
}

Snapshot Get() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_state;
}

UpdateOverlay::UpdateOverlay(rex::ui::ImGuiDrawer* drawer) : rex::ui::ImGuiDialog(drawer) {}
UpdateOverlay::~UpdateOverlay() = default;

void UpdateOverlay::OnDraw(ImGuiIO&) {}

}  // namespace update
}  // namespace ng2
