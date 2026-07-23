#pragma once

union SDL_Event;

/// <summary>
/// On-screen touch input for mobile (iOS/Android). Maps normalized finger
/// positions to the game's existing bindings (flippers, plunger, nudge) and
/// feeds them through pb::InputDown / pb::InputUp, so it respects user rebinds
/// and works exactly like the physical keys.
///
/// Zones (portrait, normalized 0..1):
///   - Bottom band  (y >= PlungerZoneTop)  -> Plunger (hold to pull, release to launch)
///   - Top band     (y <  NudgeZoneBottom) -> Nudge: left / bottom(up) / right thirds
///   - Everything between, split at x=0.5  -> Left / Right flipper
/// Multitouch is fully supported (hold both flippers at once); a finger that
/// slides across a zone boundary re-triggers the appropriate control.
/// </summary>
class TouchControls
{
public:
	// Master toggle. Harmless on desktop (no finger events are generated there).
	static bool Enabled;

	// Draw translucent on-screen zone hints (helps first-time players).
	static bool ShowOverlay;

	// Handle SDL_FINGERDOWN / SDL_FINGERMOTION / SDL_FINGERUP.
	static void HandleFingerEvent(const SDL_Event* event);

	// Release every held control. Call on focus loss / app backgrounding so a
	// flipper is never left stuck down.
	static void ReleaseAll();

	// Optional ImGui overlay showing the touch zones. No-op when ShowOverlay is false.
	static void RenderOverlay();
};
