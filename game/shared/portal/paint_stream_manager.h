#ifndef PAINT_STREAM_MANAGER_H
#define PAINT_STREAM_MANAGER_H
#ifdef _WIN32
#pragma once
#endif

// Retail Portal 2 paint; not used in RubberWar iOS build.
class CPaintStreamManager
{
public:
	static CPaintStreamManager &Instance()
	{
		static CPaintStreamManager s_Instance;
		return s_Instance;
	}
	void AllocatePaintBlobPool( int nMaxBlobCount ) { (void)nMaxBlobCount; }
};

#define PaintStreamManager ( CPaintStreamManager::Instance() )

#endif
