#pragma once

/// <summary>
/// Nudging the table by physically shoving the phone.
///
/// Uses the accelerometer rather than the gyroscope: a nudge is a linear
/// impulse, whereas the gyro measures rotation and would fire every time you
/// changed your grip. The accelerometer reading includes gravity, so gravity is
/// tracked with a low-pass filter and subtracted, leaving just the shove.
///
/// Detected nudges are sent through the engine's existing LeftTableBump /
/// RightTableBump / BottomTableBump bindings, so the game's own TILT penalty
/// applies exactly as it does with a keyboard.
/// </summary>
class MotionNudge
{
public:
	// User toggle (menu). Independent of whether hardware exists.
	static bool Enabled;

	// True once an accelerometer has been opened.
	static bool Available();

	// Opens the accelerometer if the platform has one. Safe to call when the
	// sensor subsystem is missing - it simply stays unavailable.
	static void Init();
	static void Shutdown();

	// Poll the sensor and fire bumps. elapsedMs is the frame time.
	static void Update(float elapsedMs);
};
