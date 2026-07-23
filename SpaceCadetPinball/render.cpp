#include "pch.h"
#include "render.h"

#include "fullscrn.h"
#include "GroupData.h"
#include "options.h"
#include "pb.h"
#include "score.h"
#include "TPinballTable.h"
#include "winmain.h"
#include "Background.h"
#include "DebugOverlay.h"
#include "proj.h"

std::vector<render_sprite*> render::sprite_list, render::ball_list;
zmap_header_type* render::background_zmap;
int render::zmap_offsetX, render::zmap_offsetY, render::offset_x, render::offset_y;
rectangle_type render::vscreen_rect;
gdrv_bitmap8 *render::vscreen, *render::background_bitmap, *render::ball_bitmap[20];
zmap_header_type* render::zscreen;
SDL_Rect render::DestinationRect{};

render_sprite::render_sprite(VisualTypes visualType, gdrv_bitmap8* bmp, zmap_header_type* zMap,
	int xPosition, int yPosition, rectangle_type* boundingRect)
{
	Bmp = bmp;
	ZMap = zMap;
	VisualType = visualType;
	DeleteFlag = false;
	OccludedSprites = nullptr;
	DirtyRect = rectangle_type{};
	DirtyFlag = visualType != VisualTypes::Ball;
	ZMapOffestX = 0;
	ZMapOffestY = 0;
	Depth = 0xffFF;

	if (boundingRect)
	{
		BoundingRect = *boundingRect;
	}
	else
	{
		BoundingRect.Width = -1;
		BoundingRect.Height = -1;
		BoundingRect.XPosition = 0;
		BoundingRect.YPosition = 0;
	}

	BmpRect.XPosition = xPosition;
	BmpRect.YPosition = yPosition;
	if (bmp)
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
	else
	{
		BmpRect.Width = 0;
		BmpRect.Height = 0;
	}
	DirtyRectPrev = BmpRect;

	if (!ZMap && VisualType != VisualTypes::Ball)
	{
		assertm(false, "Background zMap should not be used");
		ZMap = render::background_zmap;
		ZMapOffestY = xPosition - render::zmap_offsetX;
		ZMapOffestX = yPosition - render::zmap_offsetY;
	}

	render::AddSprite(*this);
}

render_sprite::~render_sprite()
{
	render::RemoveSprite(*this);
	delete OccludedSprites;
}

void render_sprite::set(gdrv_bitmap8* bmp, zmap_header_type* zMap, int xPos, int yPos)
{
	if (Bmp == bmp && ZMap == zMap && BmpRect.XPosition == xPos && BmpRect.YPosition == yPos)
		return;

	Bmp = bmp;
	ZMap = zMap;
	DirtyFlag = VisualType != VisualTypes::Ball;
	BmpRect.XPosition = xPos;
	BmpRect.YPosition = yPos;
	if (bmp) 
	{
		BmpRect.Width = bmp->Width;
		BmpRect.Height = bmp->Height;
	}
}

void render_sprite::ball_set(gdrv_bitmap8* bmp, float depth, int xPos, int yPos)
{
	set(bmp, ZMap,xPos, yPos);
	Depth = proj::NormalizeDepth(depth);
}

void render::init(gdrv_bitmap8* bmp, int width, int height)
{
	vscreen = new gdrv_bitmap8(width, height, false);
	zscreen = new zmap_header_type(width, height, width);
	zdrv::fill(zscreen, zscreen->Width, zscreen->Height, 0, 0, 0xFFFF);
	vscreen_rect.YPosition = 0;
	vscreen_rect.XPosition = 0;
	vscreen_rect.Width = width;
	vscreen_rect.Height = height;
	vscreen->YPosition = 0;
	vscreen->XPosition = 0;
	for (auto& ballBmp : ball_bitmap)
		ballBmp = new gdrv_bitmap8(64, 64, false);

	background_bitmap = bmp;
	if (bmp)
		gdrv::copy_bitmap(vscreen, width, height, 0, 0, bmp, 0, 0);
	else
		gdrv::fill_bitmap(vscreen, vscreen->Width, vscreen->Height, 0, 0, 0);

	recreate_screen_texture();
}

void render::uninit()
{
	delete vscreen;
	delete zscreen;

	// Sprite destructor removes it from the list.
	while (!sprite_list.empty())
		delete sprite_list[0];
	while (!ball_list.empty())
		delete ball_list[0];
	for (auto& ballBmp : ball_bitmap)
		delete ballBmp;
	DebugOverlay::UnInit();
}

void render::recreate_screen_texture()
{
	vscreen->CreateTexture(options::Options.LinearFiltering ? "linear" : "nearest", SDL_TEXTUREACCESS_STREAMING);
}

void render::update()
{
	unpaint_balls();

	// Clip dirty sprites with vScreen, clear clipping (dirty) rectangles
	for (const auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		bool clearSprite = false;
		switch (sprite->VisualType)
		{
		case VisualTypes::Sprite:
			if (sprite->DirtyRectPrev.Width > 0)
				maths::enclosing_box(sprite->DirtyRectPrev, sprite->BmpRect, sprite->DirtyRect);

			if (maths::rectangle_clip(sprite->DirtyRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = true;
			else
				sprite->DirtyRect.Width = -1;
			break;
		case VisualTypes::Background:
			if (maths::rectangle_clip(sprite->BmpRect, vscreen_rect, &sprite->DirtyRect))
				clearSprite = !sprite->Bmp;
			else
				sprite->DirtyRect.Width = -1;
			break;
		default: break;
		}

		if (clearSprite)
		{
			auto yPos = sprite->DirtyRect.YPosition;
			auto width = sprite->DirtyRect.Width;
			auto xPos = sprite->DirtyRect.XPosition;
			auto height = sprite->DirtyRect.Height;
			zdrv::fill(zscreen, width, height, xPos, yPos, 0xFFFF);
			if (background_bitmap)
				gdrv::copy_bitmap(vscreen, width, height, xPos, yPos, background_bitmap, xPos, yPos);
			else
				gdrv::fill_bitmap(vscreen, width, height, xPos, yPos, 0);
		}
	}

	// Paint dirty rectangles of dirty sprites
	for (auto sprite : sprite_list)
	{
		if (!sprite->DirtyFlag)
			continue;

		repaint(*sprite);
		sprite->DirtyFlag = false;
		sprite->DirtyRectPrev = sprite->DirtyRect;
		if (sprite->DeleteFlag)
			delete sprite;
	}

	paint_balls();
}

void render::AddSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	list.push_back(&sprite);
}

void render::RemoveSprite(render_sprite& sprite)
{
	auto& list = sprite.VisualType == VisualTypes::Ball ? ball_list : sprite_list;
	auto it = std::find(list.begin(), list.end(), &sprite);
	if (it != list.end())
		list.erase(it);
}

void render::set_background_zmap(zmap_header_type* zMap, int offsetX, int offsetY)
{
	background_zmap = zMap;
	zmap_offsetX = offsetX;
	zmap_offsetY = offsetY;
}

void render::repaint(const render_sprite& sprite)
{
	rectangle_type clipRect{};
	if (!sprite.OccludedSprites || sprite.VisualType == VisualTypes::Ball || sprite.DirtyRect.Width <= 0)
		return;

	for (auto refSprite : *sprite.OccludedSprites)
	{
		if (!refSprite->DeleteFlag && refSprite->Bmp)
		{
			if (maths::rectangle_clip(refSprite->BmpRect, sprite.DirtyRect, &clipRect))
				zdrv::paint(
					clipRect.Width,
					clipRect.Height,
					vscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					zscreen,
					clipRect.XPosition,
					clipRect.YPosition,
					refSprite->Bmp,
					clipRect.XPosition - refSprite->BmpRect.XPosition,
					clipRect.YPosition - refSprite->BmpRect.YPosition,
					refSprite->ZMap,
					clipRect.XPosition + refSprite->ZMapOffestY - refSprite->BmpRect.XPosition,
					clipRect.YPosition + refSprite->ZMapOffestX - refSprite->BmpRect.YPosition);
		}
	}
}


void render::paint_balls()
{
	// Sort ball sprites by ascending depth
	std::sort(ball_list.begin(), ball_list.end(), [](const render_sprite* lhs, const render_sprite* rhs)
	{
		return lhs->Depth < rhs->Depth;
	});

	// For balls that clip vScreen: save original vScreen contents and paint ball bitmap.
	for (auto index = 0u; index < ball_list.size(); ++index)
	{
		auto ball = ball_list[index];
		auto dirty = &ball->DirtyRect;
		if (ball->Bmp && maths::rectangle_clip(ball->BmpRect, vscreen_rect, &ball->DirtyRect))
		{
			int xPos = dirty->XPosition;
			int yPos = dirty->YPosition;
			gdrv::copy_bitmap(ball_bitmap[index], dirty->Width, dirty->Height, 0, 0, vscreen, xPos, yPos);
			zdrv::paint_flat(
				dirty->Width,
				dirty->Height,
				vscreen,
				xPos,
				yPos,
				zscreen,
				xPos,
				yPos,
				ball->Bmp,
				xPos - ball->BmpRect.XPosition,
				yPos - ball->BmpRect.YPosition,
				ball->Depth);
		}
		else
		{
			dirty->Width = -1;
		}
	}
}

void render::unpaint_balls()
{
	// Restore portions of vScreen saved during previous paint_balls call.
	for (int index = static_cast<int>(ball_list.size()) - 1; index >= 0; index--)
	{
		auto curBall = ball_list[index];
		if (curBall->DirtyRect.Width > 0)
			gdrv::copy_bitmap(
				vscreen,
				curBall->DirtyRect.Width,
				curBall->DirtyRect.Height,
				curBall->DirtyRect.XPosition,
				curBall->DirtyRect.YPosition,
				ball_bitmap[index],
				0,
				0);

		curBall->DirtyRectPrev = curBall->DirtyRect;
	}
}

void render::shift(int offsetX, int offsetY)
{
	offset_x += offsetX;
	offset_y += offsetY;
}

void render::build_occlude_list()
{
	std::vector<render_sprite*>* spriteArr = nullptr;
	for (auto mainSprite : sprite_list)
	{
		if (mainSprite->OccludedSprites)
		{
			delete mainSprite->OccludedSprites;
			mainSprite->OccludedSprites = nullptr;
		}

		if (!mainSprite->DeleteFlag && mainSprite->BoundingRect.Width != -1)
		{
			if (!spriteArr)
				spriteArr = new std::vector<render_sprite*>();

			for (auto refSprite : sprite_list)
			{
				if (!refSprite->DeleteFlag
					&& refSprite->BoundingRect.Width != -1
					&& maths::rectangle_clip(mainSprite->BoundingRect, refSprite->BoundingRect, nullptr))
				{
					spriteArr->push_back(refSprite);
				}
			}

			if (mainSprite->Bmp && spriteArr->size() < 2)
				spriteArr->clear();
			if (!spriteArr->empty())
			{
				mainSprite->OccludedSprites = spriteArr;
				spriteArr = nullptr;
			}
		}
	}

	delete spriteArr;
}

void render::SpriteViewer(bool* show)
{
	static const char* BitmapTypes[] =
	{
		"None",
		"RawBitmap",
		"DibBitmap",
		"Spliced",
	};
	static float scale = 1.0f;
	auto uv_min = ImVec2(0.0f, 0.0f); // Top-left
	auto uv_max = ImVec2(1.0f, 1.0f); // Lower-right
	auto tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
	auto border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white

	if (ImGui::Begin("Sprite viewer", show, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_MenuBar))
	{
		if (ImGui::BeginMenuBar())
		{
			ImGui::SliderFloat("Sprite scale", &scale, 0.1f, 10.0f, "scale = %.3f");
			ImGui::EndMenuBar();
		}

		for (const auto group : pb::record_table->Groups)
		{
			bool emptyGroup = true;
			for (int i = 0; i <= 2; i++)
			{
				auto bmp = group->GetBitmap(i);
				if (bmp)
				{
					emptyGroup = false;
					break;
				}
			}
			if (emptyGroup)
				continue;

			ImGui::Text("Group: %d, name:%s", group->GroupId, group->GroupName.c_str());
			for (int i = 0; i <= 2; i++)
			{
				auto bmp = group->GetBitmap(i);
				if (!bmp)
					continue;

				auto type = BitmapTypes[static_cast<uint8_t>(bmp->BitmapType)];
				ImGui::Text("type:%s, size:%d, resolution: %dx%d, offset:%dx%d", type,
				            bmp->Resolution,
				            bmp->Width, bmp->Height, bmp->XPosition, bmp->YPosition);
			}

			for (int same = 0, i = 0; i <= 2; i++)
			{
				auto bmp = group->GetBitmap(i);
				if (!bmp)
					continue;

				gdrv::CreatePreview(*bmp);
				if (bmp->Texture)
				{
					if (!same)
						same = true;
					else
						ImGui::SameLine();

					ImGui::Image(bmp->Texture, ImVec2(bmp->Width * scale, bmp->Height * scale),
					             uv_min, uv_max, tint_col, border_col);
				}
			}

			for (int same = 0, i = 0; i <= 2; i++)
			{
				auto zMap = group->GetZMap(i);
				if (!zMap)
					continue;

				zdrv::CreatePreview(*zMap);
				if (zMap->Texture)
				{
					if (!same)
						same = true;
					else
						ImGui::SameLine();
					ImGui::Image(zMap->Texture, ImVec2(zMap->Width * scale, zMap->Height * scale),
					             uv_min, uv_max, tint_col, border_col);
				}
			}
		}
	}
	ImGui::End();
}

void render::PresentVScreen()
{
	vscreen->BlitToTexture();

	if (fullscrn::MobileLayout)
	{
		// Portrait relayout: table below, score panel split into two tiles on top.
		// The panel sources are cropped to their grey frames (no black padding).
		SDL_Rect srcBoard{0, 0, fullscrn::SidebarSepX, vscreen->Height};
		const SDL_Rect& srcSideTop = fullscrn::SideTopSrcRect;
		const SDL_Rect& srcSideBot = fullscrn::SideBotSrcRect;

		// Apply nudge offset to the table only. The engine shifts by ~2 vscreen
		// pixels, which is imperceptible on a phone, so amplify it purely as
		// visual feedback - ball physics is unaffected.
		constexpr float NudgeShakeGain = 3.5f;
		SDL_Rect boardDst = fullscrn::BoardDstRect;
		if (offset_x != 0 || offset_y != 0)
		{
			boardDst.x += static_cast<int>(round(offset_x * fullscrn::ScaleX * NudgeShakeGain));
			boardDst.y += static_cast<int>(round(offset_y * fullscrn::ScaleY * NudgeShakeGain));
		}

		SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcBoard, &boardDst);

		// Same for the left/right edges: bleed the outermost columns sideways and
		// dissolve them, so the table doesn't read as a rectangle cut out and
		// pasted over the backdrop.
		int boardRight = boardDst.x + boardDst.w;
		if (boardDst.x > 0)
		{
			SDL_Rect srcEdge{0, 0, 3, vscreen->Height};
			SDL_Rect dstEdge{0, boardDst.y, boardDst.x, boardDst.h};
			SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcEdge, &dstEdge);
			// Full backdrop at the screen edge, none where the table starts.
			Background::RenderFadeBandX(dstEdge, 1.0f, 0.0f);
		}
		if (boardRight < fullscrn::MobileScreenW)
		{
			SDL_Rect srcEdge{fullscrn::SidebarSepX - 3, 0, 3, vscreen->Height};
			SDL_Rect dstEdge{
				boardRight, boardDst.y, fullscrn::MobileScreenW - boardRight, boardDst.h
			};
			SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcEdge, &dstEdge);
			Background::RenderFadeBandX(dstEdge, 0.0f, 1.0f);
		}

		// Dissolve only the black margins beside the cabinet, never the cabinet
		// itself - so the table's top edge stays perfectly crisp. The inner edge
		// of each wedge tracks the cabinet's perspective taper (measured from the
		// art), and the dissolve strength falls off with depth: full at the top,
		// none by the bottom. Everything is a fraction of the board rect, so it
		// scales to any device.
		{
			struct WedgeStop
			{
				float Y;     // fraction down the board
				float Width; // black margin width, fraction of board width
				float Alpha; // dissolve strength
			};
			static const WedgeStop profile[] =
			{
				{0.00f, 0.200f, 1.00f}, // hard dissolve beside the top edge
				{0.30f, 0.145f, 0.62f},
				{0.60f, 0.095f, 0.30f},
				{1.00f, 0.088f, 0.00f}, // no dissolve at the bottom
			};

			float bx = static_cast<float>(boardDst.x);
			float bw = static_cast<float>(boardDst.w);
			float rx = bx + bw;
			for (size_t i = 0; i + 1 < sizeof profile / sizeof profile[0]; i++)
			{
				const WedgeStop& a = profile[i];
				const WedgeStop& b = profile[i + 1];
				float y0 = boardDst.y + boardDst.h * a.Y;
				float y1 = boardDst.y + boardDst.h * b.Y;
				Background::RenderFadeWedge(bx, bx + bw * a.Width, y0,
				                            bx, bx + bw * b.Width, y1, a.Alpha, b.Alpha);
				Background::RenderFadeWedge(rx, rx - bw * a.Width, y0,
				                            rx, rx - bw * b.Width, y1, a.Alpha, b.Alpha);
			}
		}

		// The table art ends on a hard horizontal cut. Bleed its bottom rows
		// down into the empty space below, then dissolve that into the backdrop
		// so the playfield fades out instead of stopping abruptly.
		int boardBottom = boardDst.y + boardDst.h;
		if (boardBottom < fullscrn::MobileScreenH)
		{
			SDL_Rect srcEdge{0, vscreen->Height - 3, fullscrn::SidebarSepX, 3};
			SDL_Rect dstEdge{
				boardDst.x, boardBottom, boardDst.w, fullscrn::MobileScreenH - boardBottom
			};
			SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcEdge, &dstEdge);
			Background::RenderFadeBand(boardBottom, fullscrn::MobileScreenH);
		}

		SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcSideTop, &fullscrn::SideTopDstRect);
		SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcSideBot, &fullscrn::SideBotDstRect);
		return;
	}

	if (offset_x == 0 && offset_y == 0)
	{
		SDL_RenderCopy(winmain::Renderer, vscreen->Texture, nullptr, &DestinationRect);
	}
	else
	{
		auto tableWidthCoef = static_cast<float>(pb::MainTable->Width) / vscreen->Width;
		auto srcSeparationX = static_cast<int>(round(vscreen->Width * tableWidthCoef));
		auto srcBoardRect = SDL_Rect
		{
			0, 0,
			srcSeparationX, vscreen->Height
		};
		auto srcSidebarRect = SDL_Rect
		{
			srcSeparationX, 0,
			vscreen->Width - srcSeparationX, vscreen->Height
		};

#if SDL_VERSION_ATLEAST(2, 0, 10)
		// SDL_RenderCopyF was added in 2.0.10
		auto dstSeparationX = DestinationRect.w * tableWidthCoef;
		auto dstBoardRect = SDL_FRect
		{
			DestinationRect.x + offset_x * fullscrn::ScaleX,
			DestinationRect.y + offset_y * fullscrn::ScaleY,
			dstSeparationX, static_cast<float>(DestinationRect.h)
		};
		auto dstSidebarRect = SDL_FRect
		{
			DestinationRect.x + dstSeparationX, static_cast<float>(DestinationRect.y),
			DestinationRect.w - dstSeparationX, static_cast<float>(DestinationRect.h)
		};

		SDL_RenderCopyF(winmain::Renderer, vscreen->Texture, &srcBoardRect, &dstBoardRect);
		SDL_RenderCopyF(winmain::Renderer, vscreen->Texture, &srcSidebarRect, &dstSidebarRect);
#else
		// SDL_RenderCopy cannot express sub pixel offset.
		// Vscreen shift is required for that.
		auto dstSeparationX = static_cast<int>(DestinationRect.w * tableWidthCoef);
		auto scaledOffX = static_cast<int>(round(offset_x * fullscrn::ScaleX));
		if (offset_x != 0 && scaledOffX == 0)
			scaledOffX = Sign(offset_x);
		auto scaledOffY = static_cast<int>(round(offset_y * fullscrn::ScaleY));
		if (offset_y != 0 && scaledOffX == 0)
			scaledOffY = Sign(offset_y);

		auto dstBoardRect = SDL_Rect
		{
			DestinationRect.x + scaledOffX, DestinationRect.y + scaledOffY,
			dstSeparationX, DestinationRect.h
		};
		auto dstSidebarRect = SDL_Rect
		{
			DestinationRect.x + dstSeparationX, DestinationRect.y,
			DestinationRect.w - dstSeparationX, DestinationRect.h
		};

		SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcBoardRect, &dstBoardRect);
		SDL_RenderCopy(winmain::Renderer, vscreen->Texture, &srcSidebarRect, &dstSidebarRect);
#endif
	}

	if (options::Options.DebugOverlay)
	{
		DebugOverlay::DrawOverlay();
	}
}
