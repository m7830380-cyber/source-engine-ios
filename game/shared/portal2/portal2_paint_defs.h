#ifndef PORTAL2_PAINT_DEFS_H
#define PORTAL2_PAINT_DEFS_H
#ifdef _WIN32
#pragma once
#endif

enum PaintPowerType
{
	NO_POWER = 0,
	PAINT_POWER_TYPE_COUNT = 1,
};

enum StickCameraState
{
	STICK_CAMERA_UPRIGHT = 0,
};

enum InAirState
{
	ON_GROUND = 0,
};

struct CachedPaintPowerChoiceResult
{
	void Initialize() {}
};

#endif
