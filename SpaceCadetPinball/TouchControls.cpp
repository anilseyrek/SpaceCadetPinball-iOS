#include "pch.h"
#include "TouchControls.h"

#include "options.h"
#include "pb.h"
#include "fullscrn.h"

namespace
{
	// Of the table area (below the score strip): the top fraction is the plunger,
	// the rest is the flippers. Flippers get the larger share (used far more).
	constexpr float PlungerFraction = 0.28f;

	// FingerId -> the binding it is currently holding down.
	std::unordered_map<SDL_FingerID, GameBindings> ActiveFingers;

	// Screen-normalized Y where the table starts (below the score strip).
	float BoardTopNorm()
	{
		if (fullscrn::MobileScreenH > 0)
			return static_cast<float>(fullscrn::BoardDstRect.y) / fullscrn::MobileScreenH;
		return 0.0f;
	}

	// Map a normalized touch point to a control. Returns false for the score
	// strip (no input there).
	bool ClassifyZone(float x, float y, GameBindings& binding)
	{
		float boardTop = BoardTopNorm();
		if (y < boardTop)
			return false; // score strip: no game input

		float plungerBottom = boardTop + PlungerFraction * (1.0f - boardTop);
		if (y < plungerBottom)
		{
			binding = GameBindings::Plunger; // upper table: plunger
			return true;
		}
		// lower table: left / right flipper
		binding = x < 0.5f ? GameBindings::LeftFlipper : GameBindings::RightFlipper;
		return true;
	}

	// Feed the control through the same path as a real key press.
	void SendInput(GameBindings binding, bool down)
	{
		const GameInput& gi = options::Options.Key[~binding].Inputs[0];
		if (gi.Type == InputTypes::None)
			return;
		if (down)
			pb::InputDown(gi);
		else
			pb::InputUp(gi);
	}
}

bool TouchControls::Enabled = true;
// Zone hints are off by default (toggle from the mobile menu).
bool TouchControls::ShowOverlay = false;

void TouchControls::HandleFingerEvent(const SDL_Event* event)
{
	if (!Enabled)
		return;

	const SDL_TouchFingerEvent& f = event->tfinger;
	switch (event->type)
	{
	case SDL_FINGERDOWN:
		{
			GameBindings binding;
			if (!ClassifyZone(f.x, f.y, binding))
				break; // score strip: ignore
			ActiveFingers[f.fingerId] = binding;
			SendInput(binding, true);
			break;
		}
	case SDL_FINGERMOTION:
		{
			auto it = ActiveFingers.find(f.fingerId);
			GameBindings newBinding;
			bool valid = ClassifyZone(f.x, f.y, newBinding);
			if (it == ActiveFingers.end())
			{
				// Finger slid out of the score strip into a control zone.
				if (valid)
				{
					ActiveFingers[f.fingerId] = newBinding;
					SendInput(newBinding, true);
				}
				break;
			}
			if (!valid)
			{
				// Slid up into the score strip: release.
				SendInput(it->second, false);
				ActiveFingers.erase(it);
			}
			else if (newBinding != it->second)
			{
				SendInput(it->second, false);
				SendInput(newBinding, true);
				it->second = newBinding;
			}
			break;
		}
	case SDL_FINGERUP:
		{
			auto it = ActiveFingers.find(f.fingerId);
			if (it == ActiveFingers.end())
				break;
			SendInput(it->second, false);
			ActiveFingers.erase(it);
			break;
		}
	default:
		break;
	}
}

void TouchControls::ReleaseAll()
{
	for (auto& kv : ActiveFingers)
		SendInput(kv.second, false);
	ActiveFingers.clear();
}

void TouchControls::RenderOverlay()
{
	if (!ShowOverlay)
		return;

	auto* vp = ImGui::GetMainViewport();
	ImVec2 origin = vp->Pos;
	ImVec2 size = vp->Size;
	auto* dl = ImGui::GetBackgroundDrawList();

	auto rect = [&](float y0, float y1, float x0, float x1, ImU32 col)
	{
		dl->AddRectFilled(
			ImVec2(origin.x + x0 * size.x, origin.y + y0 * size.y),
			ImVec2(origin.x + x1 * size.x, origin.y + y1 * size.y),
			col);
	};

	float boardTop = BoardTopNorm();
	float plungerBottom = boardTop + PlungerFraction * (1.0f - boardTop);

	const ImU32 plunge = IM_COL32(120, 255, 120, 40);
	const ImU32 flipL = IM_COL32(80, 160, 255, 40);
	const ImU32 flipR = IM_COL32(255, 160, 80, 40);

	// Upper table: plunger (full width).
	rect(boardTop, plungerBottom, 0.0f, 1.0f, plunge);
	// Lower table: flippers.
	rect(plungerBottom, 1.0f, 0.0f, 0.5f, flipL);
	rect(plungerBottom, 1.0f, 0.5f, 1.0f, flipR);
}
