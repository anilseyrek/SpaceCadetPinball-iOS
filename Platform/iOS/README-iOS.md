# Space Cadet Pinball — iOS

An iOS port of the [k4zmu2a/SpaceCadetPinball](https://github.com/k4zmu2a/SpaceCadetPinball)
decompilation, with on-screen touch controls for mobile play.

Status: **built and verified in the iOS Simulator** (iPhone 17, iOS 26.5).
Table loads, ball launches, flippers/plunger/nudge respond, scoring works.

## What was changed for iOS

| Area | Change |
| --- | --- |
| **Touch controls** | New `SpaceCadetPinball/TouchControls.{h,cpp}` maps finger zones to the game's existing bindings and feeds them through `pb::InputDown/Up`. Wired into the SDL event loop in `winmain.cpp`. Multitouch (both flippers at once) and slide-across re-triggering. |
| **Portrait relayout** | The native landscape layout (tall table + score panel on its right) is reflowed for portrait: the score panel is cut in half and its two halves sit side-by-side as a horizontal strip **above** a full-width table. Implemented in `fullscrn.cpp` (`ComputeMobileLayout`, piecewise `GetScreenRectFromPinballRect`) and `render.cpp` (`PresentVScreen`), so both the bitmap blit and the ImGui score/message text follow the same transform. |
| **Mobile menu** | The desktop menu bar (Game/Options/Help) is replaced on iOS by a popup (New Game, Launch Ball, Pause/Resume, Sound, Music, control-hint toggle) in `winmain::RenderMobileMenu`, opened by **tapping the logo tile**. |
| **Space backdrop** | `Background.{h,cpp}` generates a deep-space backdrop at runtime (navy gradient + soft nebula glows + a stable starfield) sized to the screen, filling the letterboxed area around the table and score panel. Resolution-independent, no bundled asset; regenerates on size change. |
| **Touch → ImGui** | Finger events drive ImGui's mouse directly (`winmain.cpp`), with `WantCaptureMouse` arbitrating between the menu and the flippers; SDL touch→mouse synthesis is disabled to avoid double input. |
| **On-screen keyboard** | `winmain.cpp` starts/stops SDL text input to match `ImGui::WantTextInput`, which is what presents the iOS keyboard (needed for high score name entry). It tracks the requested state itself - SDL may already report text input active, which would suppress the `SDL_StartTextInput()` call. |
| **Settings / high scores** | `options::SaveToDisk()` flushes settings to the .ini, called on `SDL_APP_WILLENTERBACKGROUND` / `SDL_APP_TERMINATING` and whenever a high score is written. `high_score::write()` is now called when a score is entered or the table is cleared - previously its only caller was `pb::uninit()`, which never runs on iOS, so scores were lost. |
| **Entry point** | `pch.h` no longer sets `SDL_MAIN_HANDLED` on iOS, so SDL's UIKit `SDL_main` starts the app. `SDL_SetMainReady()` is skipped on iOS. |
| **Fullscreen** | The game forces fullscreen on iOS (no desktop window). |
| **Build system** | `CMakeModules/iOS.cmake` builds SDL2 + SDL2_mixer from source (static, `BUILD_SHARED_LIBS=OFF`, WAV+Timidity only) via FetchContent and packages the `.app`, bundling `game_resources/`. Hooked into `CMakeLists.txt` behind `-DCMAKE_SYSTEM_NAME=iOS`. |
| **App config** | `Platform/iOS/Info.plist.in` (portrait, hidden status bar) + `LaunchScreen.storyboard`. |

## Portrait layout

```
+---------------------------------+
| [logo + ball + score] [ player ]|  <- score panel cut at 0.56 (fullscrn.cpp),
| [   left tile      ] [ mission ]|     two columns, no controls here
+---------------------------------+
|                                 |
|      TABLE (upper) - PLUNGER     |  <- table scaled up (edges overscan off-
|                                 |     screen), centered/lowered
|      TABLE (lower)               |
|   LEFT FLIPPER | RIGHT FLIPPER   |
|                                 |
+---------------------------------+
```

- The score panel cut point is `SidebarSplitFraction` (default 0.56) and the
  table zoom is `BoardOverscan` (default 1.15), both in `fullscrn.cpp`.
- The **menu opens by tapping the logo tile** (top-left) - handled in
  `winmain::event_handler`; there is no separate menu button.

## Touch zones (tunable in `TouchControls.cpp`)

```
score strip (above the table)  -> no input
upper ~28% of the table        -> PLUNGER (hold to pull, release to launch)
lower part of the table        -> LEFT FLIPPER (left half) | RIGHT FLIPPER (right half)
```

The flipper area is intentionally the larger share. Zones are derived from the
live table rect (`fullscrn::BoardDstRect`), so they track the table position.
Translucent zone hints are on by default (toggle from the menu); multitouch
holds both flippers at once. (Nudge was dropped in this layout.)

## Prerequisites

- **Full Xcode** (not just Command Line Tools):
  ```bash
  sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
  ```
- CMake 3.14+ (`brew install cmake`).
- **Game data** — the original `PINBALL.DAT`, sounds, etc. must be present in
  `game_resources/` (they are bundled into the `.app` at build time). They are
  Microsoft's copyrighted assets and are not distributed with the source.

## Build & run (Simulator)

```bash
Platform/iOS/build-ios.sh simulator Debug
```

Then install and launch in the Simulator:

```bash
open -a Simulator
xcrun simctl install booted "$(find build-ios-simulator -name SpaceCadetPinball.app -maxdepth 4 | head -1)"
xcrun simctl launch booted com.example.spacecadetpinball
```

Or just open the generated project and press Run:

```bash
open build-ios-simulator/SpaceCadetPinball.xcodeproj
```

## Build for a physical device

Set your Apple Developer Team ID and build the device target:

```bash
SCP_DEV_TEAM=XXXXXXXXXX Platform/iOS/build-ios.sh device Release
```

## Known follow-ups

- **MIDI music**: SDL2_mixer is built with Timidity but without an instrument
  set, so background music may be silent on first build. WAV sound effects work.
  A soundfont/Timidity config can be bundled later.
- **Aspect ratio**: the table is 4:3; on tall phones it is letterboxed. Zone
  constants may want tuning per device — adjust in `TouchControls.cpp`.
- The SDL / SDL_mixer FetchContent versions are pinned to known-good releases;
  bump them in `CMakeModules/iOS.cmake` if needed.
