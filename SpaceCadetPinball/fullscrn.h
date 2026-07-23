#pragma once

struct resolution_info
{
	int16_t ScreenWidth;
	int16_t ScreenHeight;
	int16_t TableWidth;
	int16_t TableHeight;
	int16_t ResolutionMenuId;
};

class fullscrn
{
public:
	static int screen_mode;
	static int display_changed;
	static const resolution_info resolution_array[3];
	static float ScaleX;
	static float ScaleY;
	static int OffsetX;
	static int OffsetY;

	// Mobile portrait relayout: the vertical score/backbox panel is cut in half
	// and its two halves are placed side-by-side as a horizontal strip above a
	// full-width table. Both the blit (render::PresentVScreen) and the pinball->
	// screen mapping (GetScreenRectFromPinballRect) use these rects so graphics
	// and ImGui text stay aligned.
	static bool MobileLayout;
	static SDL_Rect BoardDstRect;   // table region -> lower area
	static SDL_Rect SideTopSrcRect; // vscreen source of the panel's top piece
	static SDL_Rect SideBotSrcRect; // vscreen source of the panel's bottom piece
	static SDL_Rect SideTopDstRect; // top piece -> left tile (logo/ball/score)
	static SDL_Rect SideBotDstRect; // bottom piece -> right tile (player/mission)
	static int SidebarSepX;         // vscreen x of table|sidebar boundary
	static int MobileScreenW;       // renderer output size used for the layout
	static int MobileScreenH;

	static void init();
	static void shutdown();
	static int set_screen_mode(int isFullscreen);
	static void activate(int flag);
	static int GetResolution();
	static void SetResolution(int value);
	static int GetMaxResolution();
	static void window_size_changed();
	static SDL_Rect GetScreenRectFromPinballRect(SDL_Rect rect);
	static float GetScreenToPinballRatio();
private :
	static int resolution;

	static int enableFullscreen();
	static int disableFullscreen();
};
