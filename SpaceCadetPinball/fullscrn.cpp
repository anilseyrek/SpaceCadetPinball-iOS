#include "pch.h"
#include "fullscrn.h"


#include "options.h"
#include "pb.h"
#include "render.h"
#include "winmain.h"
#include "TPinballTable.h"
#include "TTextBox.h"


int fullscrn::screen_mode;
int fullscrn::display_changed;

int fullscrn::resolution = 0;
const resolution_info fullscrn::resolution_array[3] =
{
	{640, 480, 600, 416, 501},
	{800, 600, 752, 520, 502},
	{1024, 768, 960, 666, 503},
};
float fullscrn::ScaleX = 1;
float fullscrn::ScaleY = 1;
int fullscrn::OffsetX = 0;
int fullscrn::OffsetY = 0;

bool fullscrn::MobileLayout = false;
SDL_Rect fullscrn::BoardDstRect{};
SDL_Rect fullscrn::SideTopSrcRect{};
SDL_Rect fullscrn::SideBotSrcRect{};
SDL_Rect fullscrn::SideTopDstRect{};
SDL_Rect fullscrn::SideBotDstRect{};
int fullscrn::SidebarSepX = 0;
int fullscrn::MobileScreenW = 0;
int fullscrn::MobileScreenH = 0;

void fullscrn::init()
{
	window_size_changed();
}

void fullscrn::shutdown()
{
	if (display_changed)
		set_screen_mode(0);
}

int fullscrn::set_screen_mode(int isFullscreen)
{
	int result = isFullscreen;
	if (isFullscreen == screen_mode)
		return result;
	screen_mode = isFullscreen;
	if (isFullscreen)
	{
		enableFullscreen();
		result = 1;
	}
	else
	{
		disableFullscreen();
		result = 1;
	}
	return result;
}

int fullscrn::enableFullscreen()
{
	if (!display_changed)
	{
		if (SDL_SetWindowFullscreen(winmain::MainWindow, SDL_WINDOW_FULLSCREEN_DESKTOP) == 0)
		{
			display_changed = 1;
			return 1;
		}
	}
	return 0;
}

int fullscrn::disableFullscreen()
{
	if (display_changed)
	{
		if (SDL_SetWindowFullscreen(winmain::MainWindow, 0) == 0)
			display_changed = 0;
	}

	return 0;
}

void fullscrn::activate(int flag)
{
	if (screen_mode)
	{
		if (!flag)
		{
			set_screen_mode(0);
		}
	}
}

int fullscrn::GetResolution()
{
	return resolution;
}

void fullscrn::SetResolution(int value)
{
	if (!pb::FullTiltMode || pb::FullTiltDemoMode)
		value = 0;
	assertm(value >= 0 && value <= 2, "Resolution value out of bounds");
	resolution = value;
}

int fullscrn::GetMaxResolution()
{
	return pb::FullTiltMode && !pb::FullTiltDemoMode ? 2 : 0;
}

// Fraction of the sidebar's height at which to cut it into a top piece
// (logo + ball counter) and a bottom piece (score + player + messages). The
// cut should land in the gap just above the score digits. Tunable.
// The two score boxes are separated by a single shared grey divider bar
// (measured at ~0.538..0.567 of the panel height). BOTH pieces include it, so
// each tile ends up as a fully framed box: the bottom edge of the left tile is
// the same bar as the top edge of the right tile.
static const float SplitFrameTopFraction = 0.5390f;    // -> src y 224
static const float SplitFrameBottomFraction = 0.5605f; // -> src y 233

// Vertical stretch of the table. The table art is a fixed pre-rendered
// perspective (not 3D geometry), so this cannot re-tilt the camera - it simply
// makes the playfield longer, which reads as a slightly steeper table. Ball
// physics is unaffected: everything is composited in vscreen space first, so
// the art and the ball stretch together and stay aligned. 1.0 = true aspect.
static float BoardStretchY = 1.08f;

// The score panel art carries black padding around its grey frames. Measured
// from the rendered panel, in vscreen pixels; cropping it makes the tiles sit
// exactly on their frames.
static const int PanelInsetL = 20;
static const int PanelInsetR = 10;
static const int PanelInsetT = 12; // measured: grey frame starts ~3.6px below y=8
static const int PanelInsetB = 12; // measured: grey frame ends ~3.2px above y=407

// The black region between the middle and bottom nebula bands of the backdrop
// (fractions of screen height). Keep in sync with the glows in Background.cpp;
// the table is centred inside this band.
static const float BlackSectionTop = 0.34f;
static const float BlackSectionBottom = 0.91f;

// Portrait relayout: cut the tall score/backbox panel at SidebarSplitFraction
// and lay the two pieces side-by-side as two equal-width tiles. Both tiles use
// the SAME scale (tileW / sideW), so the panel is not distorted - it is simply
// "folded" into two columns. Tile heights differ (each preserves its piece's
// height), so nothing is squashed and the score digits stay intact.
static void ComputeMobileLayout(int width, int height)
{
	auto* vs = render::vscreen;
	if (!vs || !pb::MainTable)
	{
		fullscrn::MobileLayout = false;
		return;
	}
	int vW = vs->Width, vH = vs->Height;
	int tableW = pb::MainTable->Width;
	if (tableW <= 0 || tableW >= vW)
	{
		// No separate sidebar in this data; fall back to normal layout.
		fullscrn::MobileLayout = false;
		return;
	}
	fullscrn::SidebarSepX = tableW;
	fullscrn::MobileScreenW = width;
	fullscrn::MobileScreenH = height;

	// Crop the panel to its grey frames, dropping the black padding around them.
	int cropX = tableW + PanelInsetL;
	int cropW = (vW - PanelInsetR) - cropX;
	int cropY = PanelInsetT;
	int cropBottom = vH - PanelInsetB;

	// Split into two pieces, both including the shared grey divider bar so each
	// tile is a complete framed box.
	int splitTop = static_cast<int>(vH * SplitFrameTopFraction);
	int splitBottom = static_cast<int>(vH * SplitFrameBottomFraction);
	int topSrcH = splitBottom - cropY;
	int botSrcH = cropBottom - splitTop;
	if (cropW <= 0 || topSrcH <= 0 || botSrcH <= 0)
	{
		fullscrn::MobileLayout = false;
		return;
	}
	fullscrn::SideTopSrcRect = SDL_Rect{cropX, cropY, cropW, topSrcH};
	fullscrn::SideBotSrcRect = SDL_Rect{cropX, splitTop, cropW, botSrcH};

	// Both tiles render at the SAME height. The bottom piece is shorter in the
	// source, so matching heights makes it proportionally WIDER - that is what
	// compensates the height difference and balances the pair.
	int topInset = static_cast<int>(height * 0.062f); // clear the notch / island
	int gap = std::max(2, width / 150);
	int sideMargin = static_cast<int>(width * 0.018f);
	int availW = width - 2 * sideMargin - gap;

	// availW = cropW*H/topSrcH + cropW*H/botSrcH   ->   solve for H
	float widthPerHeight = static_cast<float>(cropW) / topSrcH + static_cast<float>(cropW) / botSrcH;
	int tileH = static_cast<int>(availW / widthPerHeight);
	float maxStripH = height * 0.24f;
	if (tileH > maxStripH)
		tileH = static_cast<int>(maxStripH);

	int leftW = static_cast<int>(cropW * static_cast<float>(tileH) / topSrcH);
	int rightW = static_cast<int>(cropW * static_cast<float>(tileH) / botSrcH);
	int stripX = (width - (leftW + gap + rightW)) / 2;
	fullscrn::SideTopDstRect = SDL_Rect{stripX, topInset, leftW, tileH};
	fullscrn::SideBotDstRect = SDL_Rect{stripX + leftW + gap, topInset, rightW, tileH};

	// Table: fills the backdrop's black band (between the middle and bottom
	// nebula bands) top to bottom. The side edges overscan off-screen, which
	// keeps the playfield large - the table is deliberately clipped.
	int blackTop = static_cast<int>(height * BlackSectionTop);
	int blackH = static_cast<int>(height * BlackSectionBottom) - blackTop;
	float scaleX = static_cast<float>(blackH) / vH;
	int boardW = static_cast<int>(tableW * scaleX);
	int boardH = static_cast<int>(vH * scaleX * BoardStretchY);
	fullscrn::BoardDstRect = SDL_Rect{
		(width - boardW) / 2, blackTop + (blackH - boardH) / 2, boardW, boardH
	};

	// The board is the primary content; keep DestinationRect aligned to it so
	// board-region pinball coordinates map correctly.
	render::DestinationRect = fullscrn::BoardDstRect;
}

void fullscrn::window_size_changed()
{
	int width, height;
	SDL_GetRendererOutputSize(winmain::Renderer, &width, &height);

	if (MobileLayout)
	{
		ComputeMobileLayout(width, height);
		if (MobileLayout)
			return;
	}

	int menuHeight = options::Options.ShowMenu ? winmain::MainMenuHeight : 0;
	height -= menuHeight;
	auto res = &resolution_array[resolution];
	ScaleX = static_cast<float>(width) / res->TableWidth;
	ScaleY = static_cast<float>(height) / res->TableHeight;
	OffsetX = OffsetY = 0;
	auto offset2X = 0, offset2Y = 0;

	if (options::Options.IntegerScaling)
	{
		ScaleX = ScaleX < 1 ? ScaleX : std::floor(ScaleX);
		ScaleY = ScaleY < 1 ? ScaleY : std::floor(ScaleY);
	}

	if (options::Options.UniformScaling)
	{
		ScaleY = ScaleX = std::min(ScaleX, ScaleY);
	}

	offset2X = static_cast<int>(floor(width - res->TableWidth * ScaleX));
	offset2Y = static_cast<int>(floor(height - res->TableHeight * ScaleY));
	OffsetX = offset2X / 2;
	OffsetY = offset2Y / 2;

	render::DestinationRect = SDL_Rect
	{
		OffsetX, OffsetY + menuHeight,
		width - offset2X, height - offset2Y
	};
}

SDL_Rect fullscrn::GetScreenRectFromPinballRect(SDL_Rect rect)
{
	SDL_Rect converted_rect;

	if (MobileLayout)
	{
		int vH = render::vscreen->Height;
		if (rect.x < SidebarSepX)
		{
			// Table region -> board area (source width is the table, not full vscreen).
			converted_rect.x = rect.x * BoardDstRect.w / SidebarSepX + BoardDstRect.x;
			converted_rect.y = rect.y * BoardDstRect.h / vH + BoardDstRect.y;
			converted_rect.w = rect.w * BoardDstRect.w / SidebarSepX;
			converted_rect.h = rect.h * BoardDstRect.h / vH;
			return converted_rect;
		}

		// Sidebar region -> whichever of the two tiles contains this row. The
		// pieces overlap on the divider bar; anything below its start belongs
		// to the bottom tile.
		bool inTop = rect.y < SideBotSrcRect.y;
		const SDL_Rect& src = inTop ? SideTopSrcRect : SideBotSrcRect;
		const SDL_Rect& tile = inTop ? SideTopDstRect : SideBotDstRect;
		converted_rect.x = (rect.x - src.x) * tile.w / src.w + tile.x;
		converted_rect.y = (rect.y - src.y) * tile.h / src.h + tile.y;
		converted_rect.w = rect.w * tile.w / src.w;
		converted_rect.h = rect.h * tile.h / src.h;
		return converted_rect;
	}

	converted_rect.x = rect.x * render::DestinationRect.w / render::vscreen->Width + render::DestinationRect.x;
	converted_rect.y = rect.y * render::DestinationRect.h / render::vscreen->Height + render::DestinationRect.y;

	converted_rect.w = rect.w * render::DestinationRect.w / render::vscreen->Width;
	converted_rect.h = rect.h * render::DestinationRect.h / render::vscreen->Height;

	return converted_rect;
}

float fullscrn::GetScreenToPinballRatio()
{
	if (MobileLayout && SideBotSrcRect.h > 0)
	{
		// The message boxes live in the bottom piece; scale fonts to that tile.
		return static_cast<float>(SideBotDstRect.h) / SideBotSrcRect.h;
	}
	return (float) render::DestinationRect.w / render::vscreen->Width;
}