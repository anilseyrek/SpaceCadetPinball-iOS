#include "pch.h"
#include "MotionNudge.h"

#include "options.h"
#include "pb.h"

namespace
{
	SDL_Sensor* Accel = nullptr;

	// Low-pass estimate of gravity, subtracted to leave the shove impulse.
	float GravX = 0.0f, GravY = 0.0f, GravZ = 0.0f;
	bool GravityPrimed = false;

	// A bump is momentary: press, hold briefly, release.
	bool Holding = false;
	GameBindings HeldBump = GameBindings::LeftTableBump;
	float HoldMsLeft = 0.0f;
	float CooldownMsLeft = 0.0f;

	// --- Tuning -------------------------------------------------------------
	// Impulse needed to count as a nudge, in m/s^2 once gravity is removed.
	// Roughly "a deliberate flick of the wrist" rather than normal handling.
	constexpr float TriggerAccel = 3.4f;
	// How fast the gravity estimate tracks. Low enough to ignore a quick shove,
	// high enough to follow you turning the phone.
	constexpr float GravityBlend = 0.12f;
	// How long the bump key is held, and the gap before another can fire.
	constexpr float HoldMs = 60.0f;
	constexpr float CooldownMs = 280.0f;

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

bool MotionNudge::Enabled = true;

bool MotionNudge::Available() { return Accel != nullptr; }

void MotionNudge::Init()
{
	if (Accel)
		return;
	if (!SDL_WasInit(SDL_INIT_SENSOR))
		return;

	for (int i = 0; i < SDL_NumSensors(); i++)
	{
		if (SDL_SensorGetDeviceType(i) == SDL_SENSOR_ACCEL)
		{
			Accel = SDL_SensorOpen(i);
			break;
		}
	}
	printf("Motion nudge: %s\n", Accel ? "accelerometer opened" : "no accelerometer");
}

void MotionNudge::Shutdown()
{
	if (Accel)
	{
		if (Holding)
		{
			SendInput(HeldBump, false);
			Holding = false;
		}
		SDL_SensorClose(Accel);
		Accel = nullptr;
	}
	GravityPrimed = false;
}

void MotionNudge::Update(float elapsedMs)
{
	// Release a bump that is still held, even if the feature was just disabled.
	if (Holding)
	{
		HoldMsLeft -= elapsedMs;
		if (HoldMsLeft <= 0.0f)
		{
			SendInput(HeldBump, false);
			Holding = false;
		}
	}
	if (CooldownMsLeft > 0.0f)
		CooldownMsLeft -= elapsedMs;

	if (!Enabled || !Accel || Holding || CooldownMsLeft > 0.0f)
		return;

	float data[3]{};
	if (SDL_SensorGetData(Accel, data, 3) < 0)
		return;

	if (!GravityPrimed)
	{
		GravX = data[0];
		GravY = data[1];
		GravZ = data[2];
		GravityPrimed = true;
		return;
	}

	GravX += (data[0] - GravX) * GravityBlend;
	GravY += (data[1] - GravY) * GravityBlend;
	GravZ += (data[2] - GravZ) * GravityBlend;

	// What is left after gravity is the shove.
	float impulseX = data[0] - GravX;
	float impulseY = data[1] - GravY;

	GameBindings bump;
	if (std::abs(impulseX) > TriggerAccel && std::abs(impulseX) >= std::abs(impulseY))
	{
		// Portrait: +x is to the right of the screen.
		bump = impulseX > 0.0f ? GameBindings::RightTableBump : GameBindings::LeftTableBump;
	}
	else if (impulseY > TriggerAccel)
	{
		// Shoved away from you - the classic "bump from the bottom".
		bump = GameBindings::BottomTableBump;
	}
	else
	{
		return;
	}

	SendInput(bump, true);
	HeldBump = bump;
	Holding = true;
	HoldMsLeft = HoldMs;
	CooldownMsLeft = CooldownMs;
}
