// Ninja Gaiden II for PS5 - the hub's pages (hub_pages.h).

#include "ps5/hub/hub_pages.h"

#include <algorithm>
#include <cmath>

#include "ps5/hub/hub_backdrop.h"

namespace ng2ps5::hub {

namespace {

const char* const kHomeItems[] = {"START GAME", "SETTINGS", "DOWNLOADABLE CONTENT", "SAVES", "ABOUT", "QUIT"};
constexpr int kHomeCount = 6;

const char* ResolutionName(int preset) {
  switch (preset) {
    case 1: return "1440p";
    case 2: return "720p";
    default: return "4K";
  }
}

}  // namespace

Hub::Hub(HubContext context) : ctx_(std::move(context)) { SetTab(0); }

void Hub::SetTab(int tab) {
  tab_ = (tab + int(Tab::kCount)) % int(Tab::kCount);
  std::vector<const Setting*> rows;
  for (const Setting& s : Settings())
    if (int(s.tab) == tab_)
      rows.push_back(&s);
  list_.SetRows(std::move(rows));
}

void Hub::GoTo(Page page) {
  page_ = page;
  page_in_ = 0;
}

void Hub::Toast(std::string text) {
  toast_ = std::move(text);
  toast_time_ = 4.0f;
}

void Hub::Confirm(std::string title, std::string text, std::function<void()> yes) {
  confirm_title_ = std::move(title);
  confirm_text_ = std::move(text);
  confirm_yes_ = std::move(yes);
  confirm_open_ = true;
}

std::string Hub::StartCaption() const {
  const Ng2Settings& s = *ctx_.settings;
  return std::string(ResolutionName(s.ps5_resolution)) + "  ·  " + std::to_string(s.fps) + " fps";
}

Hub::Result Hub::Draw(const PadState& pad_in, float dt, ImDrawList* dl, const ImVec2& display) {
  dt_ = std::min(dt, 0.1f);
  time_ += dt_;
  c_.Begin(dl, display);
  c_.alpha = 1.0f - leave_;
  // The display outside the 16:9 canvas, if any, is plain black.
  dl->AddRectFilled(ImVec2(0, 0), display, IM_COL32(0, 0, 0, 255));

  // Input stops the moment START GAME (or QUIT) is chosen: the fade is not
  // interruptible.
  const PadState pad = leaving_ ? PadState{} : pad_in;
  const bool modal = confirm_open_ && ConfirmOpen(pad);
  const PadState page_pad = modal ? PadState{} : pad;

  DrawBackdrop(c_, time_);
  Ease(page_in_, 1.0f, dt_, 10.0f);
  switch (page_) {
    case Page::kHome: Home(page_pad); break;
    case Page::kSettings: SettingsPage(page_pad); break;
    case Page::kDlc: DlcPage(page_pad); break;
    case Page::kSaves: SavesPage(page_pad); break;
    case Page::kAbout: AboutPage(page_pad); break;
  }

  if (confirm_open_) {
    c_.Rect(0, 0, 1920, 1080, IM_COL32(0, 0, 0, 150));
    Panel(560, 380, 800, 300);
    c_.Embossed(GetFonts().menu, 44, 960, 410, colour::kIvory, confirm_title_, 0.5f, 0.0f);
    c_.Wrapped(GetFonts().body, 28, 620, 490, 680, colour::kIvoryDim, confirm_text_);
    c_.Hint(1340, 640, kPadCircle, "No");
    c_.Hint(1180, 640, kPadCross, "Yes");
  }

  if (toast_time_ > 0) {
    toast_time_ -= dt_;
    const float a = std::min(1.0f, toast_time_ * 2.0f);
    const float w = c_.TextWidth(GetFonts().body, 26, toast_) + 60;
    c_.Rect(960 - w / 2, 950, w, 52, IM_COL32(10, 10, 12, int(220 * a)), 8);
    c_.Text(GetFonts().body, 26, 960, 962, IM_COL32(236, 230, 214, int(255 * a)), toast_, 0.5f);
  }

  if (settings_dirty_) {
    ctx_.settings->Save();
    settings_dirty_ = false;
  }

  if (leaving_) {
    leave_ = std::min(1.0f, leave_ + dt_ / 0.6f);
    if (leave_ >= 1.0f)
      return quitting_ ? Result::kQuit : Result::kStart;
  }
  return Result::kNone;
}

bool Hub::ConfirmOpen(const PadState& pad) {
  if (pad.pressed & kPadCross) {
    confirm_open_ = false;
    PlaySound(Sound::kSelect);
    if (confirm_yes_)
      confirm_yes_();
  } else if (pad.pressed & kPadCircle) {
    confirm_open_ = false;
    PlaySound(Sound::kBack);
  }
  return true;
}

void Hub::Hints(std::initializer_list<std::pair<uint32_t, const char*>> hints) {
  float right = 1840;
  for (const auto& [button, label] : hints)
    right = c_.Hint(right, 1030, button, label);
}

void Hub::PageTitle(const char* title, const std::string& subtitle) {
  const float slide = (1.0f - page_in_) * 40.0f;
  c_.Embossed(GetFonts().menu, 64, 120 - slide, 70, colour::kIvory, title, 0.0f, 0.0f);
  if (!subtitle.empty())
    c_.Text(GetFonts().body, 26, 124 - slide, 150, colour::kIvoryDim, subtitle);
}

void Hub::Panel(float x, float y, float w, float h) {
  c_.Rect(x, y, w, h, colour::kPanel, 10);
  c_.Frame(x, y, w, h, IM_COL32(120, 20, 18, 160), 2, 10);
}

// --- Home --------------------------------------------------------------------

void Hub::Home(const PadState& pad) {
  const bool ready = ctx_.game.Ready();
  if (pad.pressed & kPadUp) {
    home_sel_ = (home_sel_ + kHomeCount - 1) % kHomeCount;
    wipe_ = 0;
    PlaySound(Sound::kMove);
  }
  if (pad.pressed & kPadDown) {
    home_sel_ = (home_sel_ + 1) % kHomeCount;
    wipe_ = 0;
    PlaySound(Sound::kMove);
  }
  const bool start = (pad.pressed & kPadOptions) || ((pad.pressed & kPadCross) && home_sel_ == 0);
  if (start) {
    if (!ready) {
      PlaySound(Sound::kError);
      Confirm("GAME DATA", ctx_.game.message + "\n\nThe folder is /data/homebrew/PPSA94202/Game Data.", nullptr);
    } else {
      PlaySound(Sound::kStart);
      leaving_ = true;
    }
  } else if (pad.pressed & kPadCross) {
    PlaySound(Sound::kSelect);
    switch (home_sel_) {
      case 1: GoTo(Page::kSettings); break;
      case 2: GoTo(Page::kDlc); break;
      case 3: GoTo(Page::kSaves); break;
      case 4: GoTo(Page::kAbout); break;
      case 5:
        Confirm("QUIT", "Back to the PS5 home screen?", [this] {
          quitting_ = true;
          leaving_ = true;
        });
        break;
      default: break;
    }
  }

  DrawLogo(c_, 960, 150, 1.0f, time_);

  const float top = 500, step = 72;
  if (home_focus_ < 0)
    home_focus_ = float(home_sel_);
  Ease(home_focus_, float(home_sel_), dt_, 16.0f);
  Ease(wipe_, 1.0f, dt_, 6.0f);
  for (int i = 0; i < kHomeCount; ++i) {
    const float y = top + float(i) * step;
    const float focus = std::max(0.0f, 1.0f - std::fabs(home_focus_ - float(i)));
    ImU32 face = colour::kIvory;
    if (i == 0 && !ready)
      face = colour::kMuted;
    if (focus > 0.5f)
      face = colour::kRed;
    c_.Embossed(GetFonts().menu, 46, 960, y, face, kHomeItems[i], 0.5f, focus);
    if (i == home_sel_ && wipe_ < 0.999f) {
      // The slash: a bright diagonal that crosses the item once on focus.
      const float w = c_.TextWidth(GetFonts().menu, 46, kHomeItems[i]) + 80;
      const float x = 960 - w / 2 + wipe_ * w;
      const float a = std::sin(wipe_ * 3.14159f);
      c_.dl->AddLine(c_.P(x - 18, y + 58), c_.P(x + 18, y - 4), c_.C(IM_COL32(255, 230, 220, int(220 * a))),
                     3.0f * c_.s);
    }
  }
  // Under START GAME: the resolution it will start at, or why it cannot.
  c_.Text(GetFonts().body, 24, 960, top + 6 * step + 24, ready ? colour::kIvoryDim : colour::kWarn,
          ready ? StartCaption() : ctx_.game.message, 0.5f);

  Hints({{kPadCross, "Select"}, {kPadOptions, "Start game"}});
}

// --- Settings ----------------------------------------------------------------

void Hub::SettingsPage(const PadState& pad) {
  if (pad.pressed & kPadCircle) {
    PlaySound(Sound::kBack);
    GoTo(Page::kHome);
    return;
  }
  if (pad.pressed & kPadL1) {
    SetTab(tab_ - 1);
    PlaySound(Sound::kTab);
  }
  if (pad.pressed & kPadR1) {
    SetTab(tab_ + 1);
    PlaySound(Sound::kTab);
  }
  if (list_.Update(pad, *ctx_.settings, false))
    settings_dirty_ = true;

  PageTitle("SETTINGS", "Changes are saved at once. \"Next start\" ones apply when you press START GAME.");

  // The tabs, L1 and R1 at either end.
  const float ty = 200;
  float x = 420;
  c_.Glyph(kPadL1, 360, ty + 22, 16);
  for (int t = 0; t < int(Tab::kCount); ++t) {
    const char* name = TabName(Tab(t));
    const float w = c_.TextWidth(GetFonts().heading, 30, name);
    const bool on = t == tab_;
    c_.Text(GetFonts().heading, 30, x, ty, on ? colour::kIvory : colour::kIvoryDim, name);
    if (on)
      c_.Rect(x, ty + 44, w, 4, colour::kRed);
    x += w + 46;
  }
  c_.Glyph(kPadR1, x + 10, ty + 22, 16);

  Panel(340, 270, 1240, 700);
  list_.Draw(c_, *ctx_.settings, 360, 290, 1200, 660, false, dt_);

  Hints({{kPadCircle, "Back"}, {kPadSquare, "Default"}, {kPadTriangle, "Help"}, {kPadLeft | kPadRight, "Change"}});
}

// --- Downloadable content ------------------------------------------------------

void Hub::DlcPage(const PadState& pad) {
  if (pad.pressed & kPadCircle) {
    PlaySound(Sound::kBack);
    GoTo(Page::kHome);
    return;
  }
  const int n = int(ctx_.dlc.size());
  if (n && (pad.pressed & kPadUp)) dlc_sel_ = (dlc_sel_ + n - 1) % n, PlaySound(Sound::kMove);
  if (n && (pad.pressed & kPadDown)) dlc_sel_ = (dlc_sel_ + 1) % n, PlaySound(Sound::kMove);

  int valid = 0;
  for (const auto& d : ctx_.dlc) valid += d.valid ? 1 : 0;
  PageTitle("DOWNLOADABLE CONTENT", std::to_string(valid) + " package(s) in the DLC folder. They are installed when "
                                                           "you press START GAME.");
  Panel(340, 210, 1240, 760);
  if (ctx_.dlc.empty()) {
    c_.Wrapped(GetFonts().body, 28, 400, 260, 1120, colour::kIvoryDim,
               "No packages yet. Copy the DLC files from your Xbox 360 (Content/<profile>/544307D5/00000002/) "
               "into /data/homebrew/PPSA94202/DLC/ - as they are, without extensions.");
  }
  for (int i = 0; i < n; ++i) {
    const float y = 240 + float(i) * 60;
    if (y > 860)
      break;
    const auto& d = ctx_.dlc[size_t(i)];
    if (i == dlc_sel_)
      c_.Rect(360, y - 8, 1200, 56, IM_COL32(150, 18, 16, 110));
    c_.Text(GetFonts().bold, 28, 390, y, colour::kIvory, d.name);
    c_.Text(GetFonts().body, 26, 1530, y, d.valid ? colour::kGood : colour::kWarn,
            d.valid ? "ready" : "not a DLC package", 1.0f);
  }
  c_.Wrapped(GetFonts().body, 22, 380, 890, 1160, colour::kMuted,
             "Mission Mode needs the game's title update, which this version does not load yet. The other "
             "packs (costumes, Fiend Challenges) work without it.");
  Hints({{kPadCircle, "Back"}});
}

// --- Saves ---------------------------------------------------------------------

void Hub::SavesPage(const PadState& pad) {
  if (pad.pressed & kPadCircle) {
    PlaySound(Sound::kBack);
    GoTo(Page::kHome);
    return;
  }
  // Row 0: import on START GAME; row 1: back up now.
  if (pad.pressed & kPadUp) saves_sel_ = (saves_sel_ + 1) % 2, PlaySound(Sound::kMove);
  if (pad.pressed & kPadDown) saves_sel_ = (saves_sel_ + 1) % 2, PlaySound(Sound::kMove);
  int usable = 0;
  for (const auto& s : ctx_.saves) usable += s.usable ? 1 : 0;
  if (pad.pressed & (kPadCross | kPadLeft | kPadRight)) {
    if (saves_sel_ == 0) {
      if (usable == 0) {
        PlaySound(Sound::kError);
        Toast("No Ninja Gaiden II saves in the Saves Import folder.");
      } else {
        import_saves_ = !import_saves_;
        PlaySound(Sound::kChange);
      }
    } else if (pad.pressed & kPadCross) {
      PlaySound(Sound::kSelect);
      Toast(ctx_.backup ? ctx_.backup() : std::string());
    }
  }

  PageTitle("SAVES", "Your saves and profile are in the app's UserData folder.");
  Panel(340, 210, 1240, 760);
  const char* rows[2] = {"Import these saves on START GAME", "Back up UserData now"};
  for (int i = 0; i < 2; ++i) {
    const float y = 240 + float(i) * 64;
    if (i == saves_sel_)
      c_.Rect(360, y - 8, 1200, 58, IM_COL32(150, 18, 16, 110));
    c_.Text(GetFonts().bold, 30, 390, y, colour::kIvory, rows[i]);
    if (i == 0)
      c_.Text(GetFonts().body, 30, 1530, y, colour::kIvory, import_saves_ ? "Yes" : "No", 1.0f);
  }
  c_.Text(GetFonts().heading, 28, 390, 400, colour::kIvoryDim, "IN SAVES IMPORT");
  if (ctx_.saves.empty())
    c_.Wrapped(GetFonts().body, 26, 390, 450, 1140, colour::kMuted,
               "Empty. Copy Xbox 360 save packages (Content/<profile>/544307D5/00000001/) into "
               "/data/homebrew/PPSA94202/Saves Import/.");
  for (size_t i = 0; i < ctx_.saves.size(); ++i) {
    const float y = 450 + float(i) * 46;
    if (y > 900)
      break;
    const auto& s = ctx_.saves[i];
    c_.Text(GetFonts().body, 26, 390, y, s.usable ? colour::kIvory : colour::kMuted, s.name);
    c_.Text(GetFonts().body, 22, 1530, y + 3, s.usable ? colour::kGood : colour::kWarn, s.message, 1.0f);
  }
  Hints({{kPadCircle, "Back"}, {kPadCross, "Select"}});
}

// --- About ---------------------------------------------------------------------

void Hub::AboutPage(const PadState& pad) {
  if (pad.pressed & kPadCircle) {
    PlaySound(Sound::kBack);
    GoTo(Page::kHome);
    return;
  }
  if (pad.pressed & kPadCross) {
    PlaySound(Sound::kSelect);
    Toast(ctx_.write_diagnostics ? ctx_.write_diagnostics() : std::string());
  }
  if (pad.pressed & kPadSquare) {
    Confirm("RESET SETTINGS", "Put every setting back to the PS5 defaults?", [this] {
      ResetAll(*ctx_.settings);
      settings_dirty_ = true;
      Toast("Settings reset.");
    });
  }

  PageTitle("ABOUT", "Ninja Gaiden II for PS5  " + ctx_.version);
  Panel(340, 210, 1240, 760);
  float y = 240;
  auto line = [&](ImFont* f, float size, ImU32 col, const std::string& t) {
    y += c_.Wrapped(f, size, 390, y, 1140, col, t) + 6;
  };
  const Fonts& f = GetFonts();
  line(f.heading, 28, colour::kIvory, "FOLDERS");
  line(f.body, 24, ctx_.game.Ready() ? colour::kGood : colour::kWarn, "Game Data: " + ctx_.game.message);
  line(f.body, 24, colour::kIvoryDim, "DLC: " + std::to_string(ctx_.dlc.size()) + " file(s)    Saves Import: " +
                                          std::to_string(ctx_.saves.size()) + " file(s)");
  y += 14;
  line(f.heading, 28, colour::kIvory, "CREDITS");
  line(f.body, 22, colour::kIvoryDim,
       "Ninja's Dawn, the Ninja Gaiden II recompilation, by TheSaltTrader (MIT). ReXGlue and Xenia "
       "(BSD). The PS5 runtime by elripalda (SvR 2011 and Skate 3 PS5 ports). ps5-plume-triangle and "
       "UnleashedRecomp-Prospero by Nazky. PS5_Vulkan, PS5_Mesa and the payload SDK fork by mihawk-99; "
       "the payload SDK by John Törnblom. The hub's design draws on ps5-homebrew-ui by BlackBearReloaded. "
       "Fonts: Cinzel and Inter (SIL Open Font License).");
  line(f.body, 22, colour::kIvoryDim,
       "No game files ship with this app: it runs from the copy of the game you put in Game Data. Ninja "
       "Gaiden is a trademark of Koei Tecmo; this project is not affiliated with them.");
  y += 14;
  line(f.body, 22, colour::kMuted, "Write diagnostics puts the log, settings and system details in logs/ for a "
                                   "report.");
  Hints({{kPadCircle, "Back"}, {kPadSquare, "Reset settings"}, {kPadCross, "Write diagnostics"}});
}

}  // namespace ng2ps5::hub
