// Ninja Gaiden II for PS5 - every setting the hub and the overlay offer, as
// rows (hub_ui.h). The table is docs/PS5_PORT.md 5.6: each PC setting that makes
// sense on a console, mapped onto the same Ng2Settings field the PC menus
// write, so the rest of the port (ng2_tuning.h, ApplyLiveSettings) applies it
// exactly as it does on PC.

#include <algorithm>

#include <rex/cvar.h>

#include "ps5/hub/hub_ui.h"

namespace ng2ps5::hub {

const char* TabName(Tab tab) {
  switch (tab) {
    case Tab::kDisplay: return "Display";
    case Tab::kGraphics: return "Graphics";
    case Tab::kGame: return "Game";
    case Tab::kTextures: return "Textures";
    case Tab::kPerformance: return "Performance";
    case Tab::kDeveloper: return "Developer";
    default: return "";
  }
}

namespace {

// A two-way switch over a bool field.
Setting Toggle(Tab tab, const char* label, const char* help, bool Ng2Settings::*field, bool def, bool restart,
               bool live, const char* off = "Off", const char* on = "On") {
  Setting s;
  s.tab = tab;
  s.label = label;
  s.help = help;
  s.choices = {off, on};
  s.get = [field](const Ng2Settings& v) { return (v.*field) ? 1 : 0; };
  s.set = [field](Ng2Settings& v, int i) { v.*field = i != 0; };
  s.def = def ? 1 : 0;
  s.restart = restart;
  s.live = live;
  return s;
}

// A choice over string values.
Setting Strings(Tab tab, const char* label, const char* help, std::string Ng2Settings::*field,
                std::vector<std::string> values, std::vector<std::string> names, int def, bool restart, bool live) {
  Setting s;
  s.tab = tab;
  s.label = label;
  s.help = help;
  s.choices = std::move(names);
  s.get = [field, values](const Ng2Settings& v) {
    auto it = std::find(values.begin(), values.end(), v.*field);
    return it == values.end() ? 0 : int(it - values.begin());
  };
  s.set = [field, values](Ng2Settings& v, int i) { v.*field = values[size_t(i)]; };
  s.def = def;
  s.restart = restart;
  s.live = live;
  return s;
}

// A choice over int values.
Setting Ints(Tab tab, const char* label, const char* help, int Ng2Settings::*field, std::vector<int> values,
             std::vector<std::string> names, int def, bool restart, bool live) {
  Setting s;
  s.tab = tab;
  s.label = label;
  s.help = help;
  s.choices = std::move(names);
  s.get = [field, values](const Ng2Settings& v) {
    auto it = std::find(values.begin(), values.end(), v.*field);
    return it == values.end() ? 0 : int(it - values.begin());
  };
  s.set = [field, values](Ng2Settings& v, int i) { v.*field = values[size_t(i)]; };
  s.def = def;
  s.restart = restart;
  s.live = live;
  return s;
}

std::vector<Setting> Build() {
  std::vector<Setting> t;

  // --- Display ---
  t.push_back(Ints(Tab::kDisplay, "Resolution",
                   "What the game renders at: an exact 3x, 2x or 1x of its 1280x720 frame. 1080p is not "
                   "offered: it would be 1.5x, which the renderer cannot do without blurring.",
                   &Ng2Settings::ps5_resolution, {0, 1, 2}, {"4K  (3840 x 2160)", "1440p  (2560 x 1440)", "720p  (1280 x 720)"},
                   0, true, false));
  t.push_back(Ints(Tab::kDisplay, "Frame rate",
                   "The game paces itself from this. Above 60 it runs fast, not smooth, so 60 is the top.",
                   &Ng2Settings::fps, {30, 60}, {"30", "60"}, 1, true, false));
  {
    Setting s = Toggle(Tab::kDisplay, "Base render size",
                       "1280x720 is the community 720p patch: sharper, and what the resolution presets scale. "
                       "The original 1120x584 is the game as shipped.",
                       &Ng2Settings::internal_720p, true, true, false, "1120 x 584 (original)", "1280 x 720");
    t.push_back(std::move(s));
  }
  t.push_back(Ints(Tab::kDisplay, "Render scale (advanced)",
                   "Overrides the scale the resolution picked. Higher is sharper and costs more GPU time.",
                   &Ng2Settings::ps5_render_scale, {0, 1, 2, 3}, {"From resolution", "1x", "2x", "3x"}, 0, true,
                   false));
  // (No "Keep aspect ratio" row: changing it from the in-game menu went with
  // a crash; the picture always keeps its aspect - see OnConfigurePaths.)
  t.push_back(Toggle(Tab::kDisplay, "Dither output", "Adds faint noise to hide banding in dark gradients.",
                     &Ng2Settings::present_dither, false, false, true));
  {
    Setting s;
    s.tab = Tab::kDisplay;
    s.label = "Field of view";
    s.help = "Widens or narrows the 3D view.";
    s.choices = {"Default"};
    s.get = [](const Ng2Settings&) { return 0; };
    s.set = [](Ng2Settings&, int) {};
    s.unavailable = [](const Ng2Settings&) {
      return std::string("Not in this build: it needs the PC renderer's projection hook, which the PS5 "
                         "renderer does not have yet.");
    };
    t.push_back(std::move(s));
  }

  // --- Graphics ---
  t.push_back(Strings(Tab::kGraphics, "Anti-aliasing", "Smooths jagged edges after the frame is drawn.",
                      &Ng2Settings::antialias, {"none", "fxaa", "fxaa_extreme"}, {"Off", "FXAA", "FXAA Extreme"}, 0,
                      false, true));
  {
    Setting s = Strings(Tab::kGraphics, "Output filter",
                        "How the frame is scaled to the screen. CAS sharpens; FSR upscales.",
                        &Ng2Settings::present_effect, {"bilinear", "cas", "fsr"}, {"Bilinear", "CAS", "FSR"}, 0, true,
                        false);
    // Offered only as far as this build declares it (the presenter lists what
    // it implements); a filter that would do nothing is not a choice.
    s.unavailable = [](const Ng2Settings&) {
      const auto* info = rex::cvar::GetFlagInfo("present_effect");
      if (info && info->constraints.allowed_values.size() > 1)
        return std::string();
      return std::string("This build's presenter has bilinear only.");
    };
    t.push_back(std::move(s));
  }
  t.push_back(Ints(Tab::kGraphics, "Anisotropic filtering", "Sharper textures at a glancing angle.",
                   &Ng2Settings::anisotropic, {-1, 0, 1, 2, 3, 4}, {"Game default", "1x", "2x", "4x", "8x", "16x"}, 0,
                   true, false));
  t.push_back(Toggle(Tab::kGraphics, "Workaround: fuzzy alpha test",
                     "Approximates the alpha test. Only for flickering foliage or fences; off is exact.",
                     &Ng2Settings::fuzzy_alpha, false, true, false));
  t.push_back(Toggle(Tab::kGraphics, "Workaround: accurate depth",
                     "Emulates the original console's 24-bit float depth exactly. Costs GPU time; fixes z-fighting "
                     "where it appears.",
                     &Ng2Settings::accurate_depth, false, true, false));

  // --- Game ---
  t.push_back(Toggle(Tab::kGame, "Button prompts",
                     "The game's button pictures as PlayStation buttons or the original ones.",
                     &Ng2Settings::ps5_playstation_prompts, true, true, false, "Original", "PlayStation"));
  {
    Setting s = Toggle(Tab::kGame, "Flying Swallow flash",
                       "The white flash the screen gives on a Flying Swallow, as in the original. Removed "
                       "spares the eyes; nothing else about the move changes.",
                       &Ng2Settings::ps5_remove_screen_flash, false, false, true, "Original", "Removed");
    // The filter (ng2_remove_screen_flash) did not catch the game's flash on
    // the console: it is drawn some other way. Shown, greyed, until it does.
    s.unavailable = [](const Ng2Settings&) { return std::string("Coming soon."); };
    t.push_back(std::move(s));
  }
  t.push_back(Ints(Tab::kGame, "Skip intro videos", "Goes straight to the title screen.", &Ng2Settings::video_mode,
                   {0, 2}, {"Off", "On"}, 0, false, true));
  t.push_back(Toggle(Tab::kGame, "Cutscene audio fix",
                     "Keeps sound level with the picture. When the game falls behind for a moment, the sound "
                     "that piled up is skipped instead of playing late, so voices and effects don't drift "
                     "out of sync in cutscenes. Turn off to hear the original behaviour.",
                     &Ng2Settings::ps5_audio_resync, true, false, true));
  t.push_back(Toggle(Tab::kGame, "Workaround: chapter 12",
                     "The community fix for the chapter 12 crash. Turn it on only if you are stuck there: it "
                     "causes problems elsewhere.",
                     &Ng2Settings::chapter12_workaround, false, true, false));
  t.push_back(Toggle(Tab::kGame, "Hub music", "Plays the theme (hub/music/*.wav) behind this menu.",
                     &Ng2Settings::ps5_menu_music, true, false, false));

  // --- Textures ---
  t.push_back(Toggle(Tab::kTextures, "Dump textures",
                     "Writes every texture the game loads once, to Texture Packs/PS5 Dump/dump, for the PC "
                     "recompilation's pack tools. Slows loading while on.",
                     &Ng2Settings::texture_dump, false, true, false));
  {
    Setting s = Toggle(Tab::kTextures, "Use texture pack", "Replaces the game's textures with a pack's.",
                       &Ng2Settings::texture_pack, false, true, true);
    s.unavailable = [](const Ng2Settings&) {
      return std::string("Loading packs is not in this PS5 build yet; dumping is.");
    };
    t.push_back(std::move(s));
  }

  // --- Performance ---
  t.push_back(Toggle(Tab::kPerformance, "Shader compilation",
                     "Smooth builds new shaders in the background and skips the frame that would have shown "
                     "them unfinished. Exact waits for every shader, which can hitch the first time an "
                     "effect appears.",
                     &Ng2Settings::ps5_smooth_shaders, true, true, false, "Exact", "Smooth"));
  t.push_back(Ints(Tab::kPerformance, "Texture cache size",
                   "Memory for the game's textures. More means fewer reloads while streaming.",
                   &Ng2Settings::texture_cache_mb, {512, 1024, 1536, 2048}, {"512 MB", "1 GB", "1.5 GB", "2 GB"}, 1,
                   true, false));
  t.push_back(Toggle(Tab::kPerformance, "Performance readouts", "Frame rate and CPU in the corner of the screen.",
                     &Ng2Settings::hud_enabled, false, false, true));
  t.push_back(Toggle(Tab::kPerformance, "Readout: frame rate", "", &Ng2Settings::hud_fps, true, false, true));
  t.push_back(Toggle(Tab::kPerformance, "Readout: CPU", "", &Ng2Settings::hud_cpu, true, false, true));
  t.push_back(Toggle(Tab::kPerformance, "Readout: bars", "A small bar under each number.", &Ng2Settings::hud_bars,
                     true, false, true));

  // --- Developer ---
  t.push_back(Toggle(Tab::kDeveloper, "Dump HUD pictures",
                     "Writes every HUD picture the game loads to logs/hud_pictures, once each. This is how "
                     "the original button pictures are found.",
                     &Ng2Settings::ps5_dump_hud_pictures, false, true, false));
  t.push_back(Toggle(Tab::kPerformance, "Frame pacing",
                     "Display refresh times the game's frames by the TV's own refresh, so none is shown twice "
                     "or skipped. Timer is the runtime's 60 Hz clock, which drifts against the TV.",
                     &Ng2Settings::ps5_display_locked_vblank, true, false, true, "Timer", "Display refresh"));
  t.push_back(Toggle(Tab::kPerformance, "Input latency",
                     "Standard keeps a third picture queued, so a frame that runs a little late is still "
                     "shown on time: steady timing. Low shows each frame about one refresh (16 ms) sooner, "
                     "but a late frame is then shown a refresh later, so timing can feel uneven in heavy "
                     "scenes.",
                     &Ng2Settings::ps5_low_latency, false, true, false, "Standard", "Low"));
  t.push_back(Ints(Tab::kPerformance, "Memory refresh",
                   "Ninja Gaiden II needs its GPU copy of memory refreshed every frame: characters' animation "
                   "is written where nothing else notices it. Less often is faster but makes animation twitch "
                   "- keep Every frame, and let Skip unchanged memory make it cheap.",
                   &Ng2Settings::ps5_memory_refresh, {1, 2, 4},
                   {"Every frame", "Every 2nd frame", "Every 4th frame"}, 0, false, true));
  t.push_back(Toggle(Tab::kPerformance, "Skip unchanged memory",
                     "After the per-frame memory refresh, uploads only the memory whose content actually changed "
                     "(animation still updates every frame) instead of all of it: the biggest saving in heavy "
                     "scenes. Turn it off if anything looks stale or wrong.",
                     &Ng2Settings::ps5_verify_refresh, true, false, true));
  t.push_back(Toggle(Tab::kPerformance, "Fast memory checks",
                     "Checks whether a draw's memory is already on the GPU before taking the runtime's global "
                     "lock, which every draw otherwise takes several times. Turn it off only if something looks "
                     "wrong.",
                     &Ng2Settings::ps5_fast_memory_checks, true, false, true));
  t.push_back(Toggle(Tab::kPerformance, "Async submission",
                     "Hands each finished batch of GPU work to a second thread to record and submit, so the main "
                     "GPU thread can carry on with the game's next commands: faster in heavy scenes. If it ever "
                     "stutters, glitches or freezes with it on, turn it off and report it.",
                     &Ng2Settings::ps5_async_submission, true, false, true));
  t.push_back(Toggle(Tab::kPerformance, "Descriptor reuse (experimental)",
                     "Lets a draw reuse the previous draw's texture bindings when nothing changed: less CPU work "
                     "per draw, which is what limits the frame rate in heavy scenes. New - if textures look "
                     "wrong or flicker with it on, turn it off and report it.",
                     &Ng2Settings::ps5_descriptor_reuse, false, false, true));
  t.push_back(Toggle(Tab::kPerformance, "Render target path",
                     "How the original console's EDRAM is emulated. FSI is exact and the stable choice. FBO is "
                     "experimental: faster at high resolutions, but it hitches and stutters more and may show "
                     "small differences.",
                     &Ng2Settings::ps5_render_target_fbo, false, true, false, "FSI (recommended)",
                     "FBO (experimental)"));
  t.push_back(Toggle(Tab::kDeveloper, "GPU timing details",
                     "Diagnostic only: logs where the GPU thread's time goes inside draws, every 10 s. Reading "
                     "the clock thousands of times a frame costs some frame rate, so leave it off to play.",
                     &Ng2Settings::ps5_gpu_timer_details, false, false, true));
  t.push_back(Toggle(Tab::kDeveloper, "Profile GPU thread",
                     "Diagnostic: samples the thread that turns the game's drawing into Vulkan work, a thousand "
                     "times a second, and logs where its time goes (8 s of every 10). Light enough to play with; "
                     "turn it on in a slow area for half a minute, then off.",
                     &Ng2Settings::ps5_profile_gpu, false, false, true));
  t.push_back(Ints(Tab::kDeveloper, "Profile thread", "Which thread Profile GPU thread samples.",
                   &Ng2Settings::ps5_profile_target, {0, 1}, {"GPU commands", "Game main thread"}, 0, false, true));
  t.push_back(Ints(Tab::kDeveloper, "Frame report",
                   "Diagnostic only: saves every image the game renders for two frames, this long after the "
                   "game starts, to logs/frame_reports (use 720p). Or capture one from the in-game menu.",
                   &Ng2Settings::ps5_frame_report_after, {0, 30, 60, 120},
                   {"Off", "30 s after start", "60 s after start", "120 s after start"}, 0, true, false));
  return t;
}

}  // namespace

const std::vector<Setting>& Settings() {
  static const std::vector<Setting> table = Build();
  return table;
}

void ApplyPs5Defaults(Ng2Settings& s) {
  // What suits a PS5 on a TV, over the PC defaults: 1440p from the 720p base
  // with the exact (FSI) render targets - the combination that holds 60 fps
  // steadily in testing - PlayStation buttons, the texture cache the hardware
  // detection picks on PS5. Applied once; after that the player's choices
  // stand.
  s.ps5_resolution = 1;
  s.ps5_render_target_fbo = false;
  s.ps5_menu_music = true;
  s.ps5_async_submission = true;
  s.ps5_low_latency = false;
  s.ps5_render_scale = 0;
  s.internal_720p = true;
  s.fps = 60;
  s.letterbox = true;
  s.ps5_playstation_prompts = true;
  s.ps5_smooth_shaders = true;
  s.texture_cache_mb = 1024;
  s.check_for_updates = false;
  s.configured = true;
  s.ps5_defaults_applied = true;
  s.ps5_defaults_version = kPs5DefaultsVersion;
}

void UpdatePs5Defaults(Ng2Settings& s) {
  if (!s.ps5_defaults_applied) {
    ApplyPs5Defaults(s);
    return;
  }
  if (s.ps5_defaults_version < 2) {
    // Round 2: 4K was the first default and hitches; 1440p with FSI is the
    // tested one. Moved once, then the player's choice stands again.
    s.ps5_resolution = 1;
    s.ps5_render_scale = 0;
    s.ps5_render_target_fbo = false;
    s.ps5_menu_music = true;  // (the hub has its theme from this round on)
  }
  // (The rounds run oldest first, so a later round's choice wins.)
  if (s.ps5_defaults_version < 3) {
    // Round 3: build 14 made Low input latency the default; Standard is again
    // until Low has been compared on the console.
    s.ps5_low_latency = false;
  }
  if (s.ps5_defaults_version < 4) {
    // Round 4: a memory refresh less often than every frame makes animation
    // twitch (16.9 testing); back to every frame, now cheap with Skip
    // unchanged memory.
    s.ps5_memory_refresh = 1;
    s.ps5_verify_refresh = true;
  }
  if (s.ps5_defaults_version < 5) {
    // Round 5: asynchronous submission (with the frame split into batches)
    // measured faster and felt better.
    s.ps5_async_submission = true;
  }
  if (s.ps5_defaults_version < 6) {
    // Round 6: Low input latency (round 5, build 16.13) made jump timing
    // feel uneven: with two pictures a frame that misses a refresh waits for
    // the next one. Standard again; Low stays a choice.
    s.ps5_low_latency = false;
  }
  s.ps5_defaults_version = kPs5DefaultsVersion;
}

void ResetAll(Ng2Settings& s) {
  Ng2Settings fresh;
  fresh.hardware_detected = s.hardware_detected;
  s = fresh;
  ApplyPs5Defaults(s);
  s.Clamp();
}

}  // namespace ng2ps5::hub
