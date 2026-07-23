#pragma once

/// <summary>
/// Procedural deep-space backdrop drawn behind the table and score panel.
///
/// The portrait mobile layout letterboxes the table, leaving empty areas around
/// it. Rather than bundling a fixed image (which would crop or blur across
/// device sizes), the backdrop is generated once at the current screen
/// resolution: a dark navy gradient, a few soft nebula glows, and a stable
/// starfield. It regenerates automatically if the output size changes.
/// </summary>
class Background
{
public:
	// Master toggle.
	static bool Enabled;

	// Draws the backdrop, (re)generating it if the render size changed.
	static void Render();

	// Re-draws the backdrop over [y0, y1) with alpha ramping from 0 to 1 going
	// down. Used to dissolve the table's extended bottom edge into space so the
	// playfield does not end on a hard horizontal cut.
	// Alpha ramps from alphaStart at y0 to alphaEnd at y1.
	static void RenderFadeBand(int y0, int y1, float alphaStart = 0.0f, float alphaEnd = 1.0f);

	// Same idea horizontally: re-draws the backdrop across `area` with alpha
	// ramping from alphaStart (left edge) to alphaEnd (right edge). Used to
	// dissolve the table's bled side edges into space.
	static void RenderFadeBandX(const SDL_Rect& area, float alphaStart, float alphaEnd);

	// Draws the backdrop over a tapered quad spanning the black margin beside the
	// table's cabinet. Alpha is uniform across each row (alphaTop at yTop,
	// alphaBot at yBot), so the dissolve strength varies with depth while the
	// inner edge tracks the cabinet - the cabinet itself is never touched.
	static void RenderFadeWedge(float xOuterTop, float xInnerTop, float yTop,
	                            float xOuterBot, float xInnerBot, float yBot,
	                            float alphaTop, float alphaBot);

	// Frees the cached texture.
	static void Destroy();
};
