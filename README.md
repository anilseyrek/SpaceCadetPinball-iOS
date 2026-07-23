# Space Cadet Pinball — iOS

A portrait iOS port of [k4zmu2a/SpaceCadetPinball](https://github.com/k4zmu2a/SpaceCadetPinball),
the SDL2 decompilation of *3D Pinball for Windows — Space Cadet*.

Redesigned for phones: the score panel is split into two horizontal tiles above a
full-width table, touch controls replace the keyboard, and a procedural starfield
fills the space around the playfield.

| Gameplay | Control zones | Menu |
| --- | --- | --- |
| ![gameplay](docs/screenshots/gameplay.png) | ![controls](docs/screenshots/controls.png) | ![menu](docs/screenshots/menu.png) |

> ### Not a commercial project
>
> This is a hobby / preservation project, shared so people can build it for
> **themselves** with free provisioning. It is not sold, monetised, or distributed
> as a binary, and it **cannot go on the App Store**.
>
> No game data ships in this repository. The engine is an MIT-licensed
> decompilation of a Microsoft binary, and the game's data, artwork, sounds and
> branding remain Microsoft's property. Nobody involved is affiliated with or
> endorsed by Microsoft, Maxis or Cinematronics.
>
> Build it, play it on your own device, hack on it. Please don't ship it.

---

## Quick start

```bash
brew install cmake
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer   # full Xcode required

python3 Platform/iOS/tools/extract_assets.py    # fetch game data -> game_resources/
Platform/iOS/build-ios.sh simulator Debug       # or: device Release
```

Then:

```bash
xcrun simctl install booted bin/Debug/SpaceCadetPinball.app
xcrun simctl launch booted com.example.spacecadetpinball
```

For a physical device, open `build-ios-device/SpaceCadetPinball.xcodeproj`, set
**Signing & Capabilities → Team**, and Run. Requirements: an Apple ID in Xcode
(free is fine), **Developer Mode** enabled on the phone, and trusting the
certificate under Settings → General → VPN & Device Management. Free provisioning
expires after 7 days — re-run from Xcode to refresh.

**Pick the `SpaceCadetPinball` scheme**, not `SDL2-static`. Building the latter
succeeds but installs nothing.

## Game data

Not included — it is Microsoft's copyrighted content. `extract_assets.py` pulls it
from the Emscripten bundle that the upstream web build already serves publicly and
unpacks it into `game_resources/`, which CMake embeds in the `.app`. On iOS
`SDL_GetBasePath()` returns the bundle root, so the files are found automatically.

Only the 640×480 table resolution exists in `PINBALL.DAT`; the 800×600 and
1024×768 modes are gated behind Full Tilt! data (`fullscrn::GetMaxResolution()`).
The vscreen is therefore always **600×416**, which several layout constants assume.

## Install size

~**8 MB** installed: 4.2 MB stripped arm64 binary (SDL2 + SDL2_mixer are statically
linked), 2.3 MB game data, ~1 MB app icon assets. Negligible on a modern phone.

---

## Architecture notes for contributors (and agents)

The upstream engine is untouched in spirit: rendering still composites everything
into a single `vscreen` bitmap. All mobile work happens at the **blit** stage plus
input, which is why the port is small and stays mergeable with upstream.

### Files added

| File | Purpose |
| --- | --- |
| `SpaceCadetPinball/TouchControls.{h,cpp}` | Touch zones → `pb::InputDown/Up` |
| `SpaceCadetPinball/Background.{h,cpp}` | Procedural starfield, planets, edge dissolves |
| `CMakeModules/iOS.cmake` | SDL2 + SDL2_mixer via FetchContent, `.app` packaging, signing |
| `Platform/iOS/` | `Info.plist.in`, launch screen, icon catalog, build + asset scripts |

### Files modified

| File | Change |
| --- | --- |
| `fullscrn.cpp/.h` | `ComputeMobileLayout()` — portrait layout; piecewise `GetScreenRectFromPinballRect()` |
| `render.cpp` | `PresentVScreen()` mobile branch — split blit + edge dissolves |
| `winmain.cpp/.h` | Touch events, iOS lifecycle, on-screen keyboard, mobile menu |
| `high_score.cpp` | Persist scores on entry (see below) |
| `options.cpp/.h` | `SaveToDisk()` |
| `pch.h` | `SCP_PLATFORM_IOS`; SDL owns `main()` on iOS |

### The portrait layout

`fullscrn::ComputeMobileLayout()` is the single source of truth. It splits the
vscreen into three destination rects, all derived as fractions so they scale to any
device:

- **Score panel** — the tall backbox is cropped to its grey frames (`PanelInset*`)
  and cut at a shared divider bar into two pieces. **Both pieces include the
  divider**, so each tile renders as a complete framed box. They render at equal
  height, which makes the bottom piece proportionally wider.
- **Table** — fills the backdrop's black band (`BlackSectionTop/Bottom`), scaled by
  `BoardStretchY`, deliberately overscanning off-screen at the sides.

`GetScreenRectFromPinballRect()` applies the *same* piecewise transform, so ImGui
text (score, mission messages) tracks the reflowed panel. **Change one and you must
change the other**, or text drifts away from its artwork.

### Edge dissolves

The table is a rectangular bitmap with black margins beside its perspective
cabinet. Three treatments in `render.cpp`, all driven by a measured profile:

- **Sides** — tapered wedges (`SDL_RenderGeometry`) whose inner edge follows the
  cabinet (~20% of board width at top → ~9% at bottom) and whose dissolve fades
  with depth (full at top → none at bottom). It must never cross the cabinet edge:
  the table's top rail stays crisp.
- **Bottom** — the last rows are bled downward and dissolved into the backdrop, so
  the playfield recedes instead of ending on a cut.

`SDL_RenderGeometry` rejects the entire draw if any UV falls outside 0..1, and the
table overscans to negative x — so vertices are clamped, carrying their alpha.

### Input

`TouchControls` maps normalised finger positions to `GameBindings` and feeds them
through the same path as a key press, so rebinding still applies. Zones derive from
the live table rect. Flippers are **binary** — the engine has no analog input, so a
tap is a complete flip.

`winmain::event_handler` also drives ImGui's mouse from finger events (SDL's
touch→mouse synthesis is disabled), with `WantTextInput` gating the on-screen
keyboard and `WantCaptureMouse` deciding between menu and flippers.

### Two iOS traps worth knowing

Both cost real debugging time; don't reintroduce them:

1. **iOS never runs a clean shutdown.** `high_score::write()` was only ever called
   from `pb::uninit()`, so entered scores vanished. Scores are now written on entry,
   and `options::SaveToDisk()` runs on `SDL_APP_WILLENTERBACKGROUND`.
2. **`SDL_StartTextInput()` is what presents the keyboard.** Nothing called it, so
   name entry was impossible on a phone. It's tracked with our own flag, because SDL
   may already report text input active and swallow the call.

### Tuning constants

| Constant | File | Effect |
| --- | --- | --- |
| `SplitFrameTop/BottomFraction` | `fullscrn.cpp` | Where the score panel is cut |
| `PanelInsetL/R/T/B` | `fullscrn.cpp` | Crop to the grey frames |
| `BlackSectionTop/Bottom` | `fullscrn.cpp` | Band the table is centred in |
| `BoardStretchY` | `fullscrn.cpp` | Table vertical stretch (fake tilt) |
| `profile[]` | `render.cpp` | Side wedge width + dissolve per depth |
| `glows[]`, `planets[]` | `Background.cpp` | Backdrop nebulae and planets |
| `PlungerFraction` | `TouchControls.cpp` | Plunger vs flipper split |
| `TriggerAccel`, `CooldownMs` | `MotionNudge.cpp` | Nudge sensitivity and repeat rate |

### Debugging

Simulator caveats that produce misleading results:

- The software keyboard is hidden while a Mac hardware keyboard is attached.
- The renderer runs at **logical points** (e.g. 402×874); iOS upscales.
- The table overscans differently than on a narrower phone, so side effects differ.

```bash
xcrun simctl launch --console-pty booted com.example.spacecadetpinball   # stdout
xcrun simctl get_app_container booted com.example.spacecadetpinball data # settings
```

## Known gaps

- **Tilt nudge is untested on hardware.** Shoving the phone nudges the table via
  the accelerometer (`MotionNudge.cpp`), but the iOS Simulator has no
  accelerometer, so only the graceful-degradation path has been verified. The
  trigger threshold and axis directions may need tuning on a real device - see
  `TriggerAccel` and the axis mapping. There is no touch-gesture fallback yet.
- **No high-score viewer** — the table only appears at game over.
- **Player count is fixed at 1**; the engine supports 1–4.
- **MIDI music is silent** — SDL2_mixer builds with Timidity but no instrument set.
- No language selection, volume sliders, or demo mode in the mobile menu.

Upstream's original documentation is kept at [README-upstream.md](README-upstream.md).

## Licence and credits

The **port code in this repository is MIT**, matching upstream.

That licence covers source code only. It does not grant any rights to the original
game's data, artwork, audio or trademarks, which remain Microsoft's — none of which
is included here. Run `extract_assets.py` and you are obtaining those files
yourself, for your own use.

- Engine: [k4zmu2a/SpaceCadetPinball](https://github.com/k4zmu2a/SpaceCadetPinball) (MIT), by Andrey Muzychenko
- Original game: Cinematronics and Maxis, published by Microsoft
- [SDL2](https://libsdl.org) (zlib) and [Dear ImGui](https://github.com/ocornut/imgui) (MIT)

Non-commercial hobby project. Not affiliated with Microsoft.
