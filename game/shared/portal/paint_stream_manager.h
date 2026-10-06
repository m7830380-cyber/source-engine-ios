#ifndef PAINT_STREAM_MANAGER_H
#define PAINT_STREAM_MANAGER_H
#ifdef _WIN32
#pragma once
#endif

// Retail Portal 2 paint; not used in RubberWar iOS build.
class CPaintStreamManager
{
public:
	static CPaintStreamManager *GetInstance() { return NULL; }
};

#endif
