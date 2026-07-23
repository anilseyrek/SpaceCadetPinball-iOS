#include "pch.h"
#include "Background.h"

#include "winmain.h"

bool Background::Enabled = true;

namespace
{
	SDL_Texture* BgTexture = nullptr;
	int BgWidth = 0, BgHeight = 0;

	// Deterministic PRNG so the starfield is identical every frame and launch.
	uint32_t RngState = 0x13579BDFu;

	void ResetRng() { RngState = 0x13579BDFu; }

	uint32_t NextRand()
	{
		RngState = RngState * 1664525u + 1013904223u;
		return RngState;
	}

	float Rand01() { return static_cast<float>(NextRand() >> 8) / 16777216.0f; }

	inline uint32_t Clamp255(float v)
	{
		return static_cast<uint32_t>(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v));
	}

	// A soft elliptical nebula, in normalized screen coordinates. Elongated
	// shapes read more like galactic dust than round blobs.
	struct Glow
	{
		float Cx, Cy;  // centre, fractions of width / height
		float Rx, Ry;  // radii, fractions of width / height
		float R, G, B; // peak colour contribution
	};

	void GenerateBackground(int width, int height)
	{
		std::vector<uint32_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));

		// The table and score panel cover most of the screen, so the purple is
		// concentrated only where the game never draws: the top band around the
		// score tiles, the gap between the panel and the table, and below the
		// table. Everything else stays essentially black.
		const Glow glows[] =
		{
			{0.20f, 0.06f, 0.46f, 0.115f, 78.f, 30.f, 130.f}, // violet, top left
			{0.87f, 0.12f, 0.38f, 0.100f, 62.f, 22.f, 112.f}, // deep purple, top right
			{0.55f, 0.02f, 0.34f, 0.050f, 26.f, 32.f, 86.f},  // cool blue, very top
			{0.48f, 0.285f, 0.64f, 0.055f, 80.f, 28.f, 122.f},// wisp between panel & table
			{0.42f, 0.980f, 0.60f, 0.070f, 66.f, 28.f, 120.f},// glow below the table
			// Down the left/right edges, behind the table. The cabinet hides most
			// of this, but it shows through the dissolved black margins beside the
			// perspective taper - widest at the top, which is where these sit.
			// NOTE: no glows inside the table's band. Keeping it near-black means
			// the black margins beside the cabinet blend into the backdrop on
			// their own - far cleaner than trying to reveal colour through them,
			// which only creates contrast and visible seams.
		};

		for (int y = 0; y < height; y++)
		{
			float ny = static_cast<float>(y) / static_cast<float>(height);

			// Near-black base with only the faintest lift toward the middle.
			float band = 1.0f - std::abs(ny - 0.5f) * 0.7f;
			if (band < 0.0f) band = 0.0f;
			float baseR = 1.0f + band * 2.0f;
			float baseG = 2.0f + band * 2.0f;
			float baseB = 5.0f + band * 6.0f;

			for (int x = 0; x < width; x++)
			{
				float r = baseR, g = baseG, b = baseB;

				for (const auto& gl : glows)
				{
					float dx = (static_cast<float>(x) - gl.Cx * width) / (gl.Rx * width);
					float dy = (static_cast<float>(y) - gl.Cy * height) / (gl.Ry * height);
					float d2 = dx * dx + dy * dy;
					if (d2 >= 1.0f)
						continue;
					// Smooth squared falloff (no sqrt needed).
					float f = 1.0f - d2;
					f *= f;
					r += gl.R * f;
					g += gl.G * f;
					b += gl.B * f;
				}

				pixels[static_cast<size_t>(y) * width + x] =
					0xFF000000u | (Clamp255(r) << 16) | (Clamp255(g) << 8) | Clamp255(b);
			}
		}

		// Dense starfield. Density scales with area so it reads the same on any
		// screen. Most stars are faint dust; a few are bright with a cross flare.
		ResetRng();
		int starCount = std::max(260, width * height / 780);
		for (int i = 0; i < starCount; i++)
		{
			int sx = static_cast<int>(Rand01() * width);
			int sy = static_cast<int>(Rand01() * height);
			if (sx < 2 || sy < 2 || sx >= width - 2 || sy >= height - 2)
				continue;

			// Cubic curve: lots of faint dust, few bright stars.
			float t = Rand01();
			float bright = 40.0f + t * t * t * 240.0f;
			// Colour variation: pale blue through warm white.
			float tint = Rand01();
			float sr = bright * (0.72f + 0.32f * tint);
			float sg = bright * (0.80f + 0.18f * tint);
			float sb = bright;

			auto plot = [&](int px, int py, float k)
			{
				auto& dst = pixels[static_cast<size_t>(py) * width + px];
				float dr = static_cast<float>((dst >> 16) & 0xFF) + sr * k;
				float dg = static_cast<float>((dst >> 8) & 0xFF) + sg * k;
				float db = static_cast<float>(dst & 0xFF) + sb * k;
				dst = 0xFF000000u | (Clamp255(dr) << 16) | (Clamp255(dg) << 8) | Clamp255(db);
			};

			plot(sx, sy, 1.0f);
			if (t > 0.90f)
			{
				// Brightest: a small cross flare.
				plot(sx - 1, sy, 0.45f);
				plot(sx + 1, sy, 0.45f);
				plot(sx, sy - 1, 0.45f);
				plot(sx, sy + 1, 0.45f);
				plot(sx - 2, sy, 0.18f);
				plot(sx + 2, sy, 0.18f);
				plot(sx, sy - 2, 0.18f);
				plot(sx, sy + 2, 0.18f);
			}
			else if (t > 0.72f)
			{
				plot(sx - 1, sy, 0.30f);
				plot(sx + 1, sy, 0.30f);
				plot(sx, sy - 1, 0.30f);
				plot(sx, sy + 1, 0.30f);
			}
		}

		// --- Planets ----------------------------------------------------------
		// Shaded spheres drawn after the stars so they occlude rather than
		// sparkle through. Rendered on a chunky pixel grid with posterised
		// shading and hard edges, to sit alongside the game's pixel art rather
		// than looking like a smooth modern gradient. Kept to the upper half:
		// the area below the table is deliberately left untouched.
		struct Planet
		{
			float Cx, Cy;    // centre, fractions of width / height
			float Radius;    // fraction of width
			float R, G, B;   // body colour
			float Lx, Ly;    // light direction
			float RingScale; // 0 = no ring
		};
		const Planet planets[] =
		{
			{0.82f, 0.292f, 0.095f, 176.f, 126.f, 220.f, -0.55f, -0.60f, 2.0f},
			{0.10f, 0.400f, 0.060f, 120.f, 152.f, 200.f, 0.60f, -0.50f, 0.0f},
		};

		// Size of one "art pixel", in backdrop pixels.
		const int BlockPx = 2;
		// Number of discrete shading steps (low = more retro banding).
		const float ShadeSteps = 6.0f;

		for (const auto& pl : planets)
		{
			float cx = pl.Cx * width, cy = pl.Cy * height;
			float rad = pl.Radius * width;
			float lz = std::sqrt(std::max(0.0f, 1.0f - pl.Lx * pl.Lx - pl.Ly * pl.Ly));
			float reach = rad * (pl.RingScale > 0.0f ? pl.RingScale + 0.4f : 1.3f);
			int x0 = std::max(0, static_cast<int>(cx - reach));
			int x1 = std::min(width, static_cast<int>(cx + reach) + 1);
			int y0 = std::max(0, static_cast<int>(cy - reach));
			int y1 = std::min(height, static_cast<int>(cy + reach) + 1);

			for (int y = y0; y < y1; y++)
			{
				for (int x = x0; x < x1; x++)
				{
					// Snap to the block grid so each block shades as one flat pixel.
					float bxc = (std::floor(static_cast<float>(x) / BlockPx) + 0.5f) * BlockPx;
					float byc = (std::floor(static_cast<float>(y) / BlockPx) + 0.5f) * BlockPx;
					float dx = (bxc - cx) / rad;
					float dy = (byc - cy) / rad;
					float d2 = dx * dx + dy * dy;

					auto& dst = pixels[static_cast<size_t>(y) * width + x];
					float dr = static_cast<float>((dst >> 16) & 0xFF);
					float dg = static_cast<float>((dst >> 8) & 0xFF);
					float db = static_cast<float>(dst & 0xFF);

					// Ring: a flattened annulus. The far half (above the centre)
					// passes behind the body; the near half (below) passes in
					// front of it, so the ring genuinely encircles the planet.
					float ringA = 0.0f;
					bool ringNear = false;
					if (pl.RingScale > 0.0f)
					{
						float ry = dy * 3.6f;
						float rd = std::sqrt(dx * dx + ry * ry);
						if (rd > 1.20f && rd < pl.RingScale)
						{
							float t = (rd - 1.20f) / (pl.RingScale - 1.20f);
							ringA = 0.60f * (1.0f - std::abs(t * 2.0f - 1.0f));
							ringA = std::floor(ringA * 4.0f + 0.5f) / 4.0f; // posterised
							ringNear = dy > 0.0f;
						}
					}

					// Far half - occluded by the planet.
					if (ringA > 0.0f && !ringNear && d2 > 1.0f)
					{
						dr = dr * (1.0f - ringA) + 205.f * ringA;
						dg = dg * (1.0f - ringA) + 170.f * ringA;
						db = db * (1.0f - ringA) + 240.f * ringA;
					}

					if (d2 <= 1.0f)
					{
						// Fake sphere normal -> diffuse shading with a terminator.
						float nz = std::sqrt(1.0f - d2);
						float lam = dx * pl.Lx + dy * pl.Ly + nz * lz;
						if (lam < 0.0f) lam = 0.0f;
						float k = 0.12f + 0.88f * lam;
						// Quantise into flat bands - the retro part.
						k = std::floor(k * ShadeSteps + 0.5f) / ShadeSteps;
						// Hard edge, no antialiasing: keeps the silhouette blocky.
						dr = pl.R * k;
						dg = pl.G * k;
						db = pl.B * k;
					}
					else if (d2 < 1.45f)
					{
						float t = (1.45f - d2) / 0.45f;
						float a = std::floor(t * t * 3.0f + 0.5f) / 3.0f * 0.28f;
						dr += pl.R * a * 0.45f;
						dg += pl.G * a * 0.45f;
						db += pl.B * a * 0.55f;
					}

					// Near half - drawn last so it crosses in front of the body.
					if (ringA > 0.0f && ringNear)
					{
						dr = dr * (1.0f - ringA) + 215.f * ringA;
						dg = dg * (1.0f - ringA) + 178.f * ringA;
						db = db * (1.0f - ringA) + 248.f * ringA;
					}

					dst = 0xFF000000u | (Clamp255(dr) << 16) | (Clamp255(dg) << 8) | Clamp255(db);
				}
			}
		}

		BgTexture = SDL_CreateTexture(winmain::Renderer, SDL_PIXELFORMAT_ARGB8888,
		                              SDL_TEXTUREACCESS_STATIC, width, height);
		if (!BgTexture)
			return;
		SDL_UpdateTexture(BgTexture, nullptr, pixels.data(), width * static_cast<int>(sizeof(uint32_t)));
		BgWidth = width;
		BgHeight = height;
	}
}

void Background::Destroy()
{
	if (BgTexture)
	{
		SDL_DestroyTexture(BgTexture);
		BgTexture = nullptr;
	}
	BgWidth = BgHeight = 0;
}

void Background::Render()
{
	if (!Enabled || !winmain::Renderer)
		return;

	int width = 0, height = 0;
	if (SDL_GetRendererOutputSize(winmain::Renderer, &width, &height) != 0 || width <= 0 || height <= 0)
		return;

	if (!BgTexture || width != BgWidth || height != BgHeight)
	{
		Destroy();
		GenerateBackground(width, height);
		if (!BgTexture)
			return;
	}

	SDL_RenderCopy(winmain::Renderer, BgTexture, nullptr, nullptr);
}

void Background::RenderFadeBand(int y0, int y1, float alphaStart, float alphaEnd)
{
	if (!Enabled || !BgTexture || y1 <= y0)
		return;

	SDL_SetTextureBlendMode(BgTexture, SDL_BLENDMODE_BLEND);
	const int bands = 28;
	for (int i = 0; i < bands; i++)
	{
		int a0 = y0 + (y1 - y0) * i / bands;
		int a1 = y0 + (y1 - y0) * (i + 1) / bands;
		if (a1 <= a0)
			continue;
		// Backdrop alpha ramps linearly across the band, so the table edge
		// dissolves gradually and reads as receding into space.
		float t = static_cast<float>(i + 1) / bands;
		float a = alphaStart + (alphaEnd - alphaStart) * t;
		a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
		SDL_SetTextureAlphaMod(BgTexture, static_cast<Uint8>(255.0f * a));
		SDL_Rect band{0, a0, BgWidth, a1 - a0};
		SDL_RenderCopy(winmain::Renderer, BgTexture, &band, &band);
	}
	SDL_SetTextureAlphaMod(BgTexture, 255);
	SDL_SetTextureBlendMode(BgTexture, SDL_BLENDMODE_NONE);
}

void Background::RenderFadeWedge(float xOuterTop, float xInnerTop, float yTop,
                                 float xOuterBot, float xInnerBot, float yBot,
                                 float alphaTop, float alphaBot)
{
	if (!Enabled || !BgTexture)
		return;

#if SDL_VERSION_ATLEAST(2, 0, 18)
	SDL_SetTextureBlendMode(BgTexture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(BgTexture, 255);

	const float W = static_cast<float>(BgWidth), H = static_cast<float>(BgHeight);
	auto clamp = [](float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); };

	// The table deliberately overscans off-screen, so an edge can land at a
	// negative x. SDL_RenderGeometry rejects the whole draw if any uv falls
	// outside 0..1, so clamp every vertex into the surface. Alpha depends only
	// on the row, so clamping x does not distort the gradient.
	auto vertex = [&](float x, float y, float a)
	{
		float cx = clamp(x, 0.0f, W);
		float cy = clamp(y, 0.0f, H);
		SDL_Vertex v;
		v.position = SDL_FPoint{cx, cy};
		v.color = SDL_Color{255, 255, 255, static_cast<Uint8>(255.0f * clamp(a, 0.0f, 1.0f))};
		v.tex_coord = SDL_FPoint{cx / W, cy / H};
		return v;
	};

	SDL_Vertex verts[4] =
	{
		vertex(xOuterTop, yTop, alphaTop),
		vertex(xInnerTop, yTop, alphaTop),
		vertex(xOuterBot, yBot, alphaBot),
		vertex(xInnerBot, yBot, alphaBot),
	};
	int indices[6] = {0, 1, 2, 1, 3, 2};
	SDL_RenderGeometry(winmain::Renderer, BgTexture, verts, 4, indices, 6);

	SDL_SetTextureBlendMode(BgTexture, SDL_BLENDMODE_NONE);
#endif
}

void Background::RenderFadeBandX(const SDL_Rect& area, float alphaStart, float alphaEnd)
{
	if (!Enabled || !BgTexture || area.w <= 0 || area.h <= 0)
		return;

	SDL_SetTextureBlendMode(BgTexture, SDL_BLENDMODE_BLEND);
	const int bands = 28;
	for (int i = 0; i < bands; i++)
	{
		int x0 = area.x + area.w * i / bands;
		int x1 = area.x + area.w * (i + 1) / bands;
		if (x1 <= x0)
			continue;
		float t = (i + 0.5f) / bands;
		float a = alphaStart + (alphaEnd - alphaStart) * t;
		a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
		SDL_SetTextureAlphaMod(BgTexture, static_cast<Uint8>(255.0f * a));
		SDL_Rect band{x0, area.y, x1 - x0, area.h};
		SDL_RenderCopy(winmain::Renderer, BgTexture, &band, &band);
	}
	SDL_SetTextureAlphaMod(BgTexture, 255);
	SDL_SetTextureBlendMode(BgTexture, SDL_BLENDMODE_NONE);
}
