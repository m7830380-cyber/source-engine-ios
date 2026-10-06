//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// Purpose: Contains everything needed to create different gamestats tracking
//			systems.
//
//===============================================================================
#if !defined( PORTAL_GAMESTATS_SHARED_H ) && !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
#define PORTAL_GAMESTATS_SHARED_H
#ifdef _WIN32
#pragma once
#endif
#include "cbase.h"
#include "tier1/utlvector.h"
#include "tier1/utldict.h"
#include "shareddefs.h"
#include "fmtstr.h"
#include "steamworks_gamestats.h"

// Portal 2 had one shared uploader; this tree splits it per side
#ifdef CLIENT_DLL
#include "steamworks_gamestats_client.h"
inline CSteamWorksGameStatsUploader& GetSteamWorksSGameStatsUploader() { return GetSteamWorksGameStatsClient(); }
#else
#include "steamworks_gamestats_server.h"
inline CSteamWorksGameStatsUploader& GetSteamWorksSGameStatsUploader() { return GetSteamWorksGameStatsServer(); }
#endif

//=============================================================================
//
// Helper functions for creating key values
//
void AddDataToKV( KeyValues* pKV, const char* name, int data );
void AddDataToKV( KeyValues* pKV, const char* name, uint64 data );
void AddDataToKV( KeyValues* pKV, const char* name, float data );
void AddDataToKV( KeyValues* pKV, const char* name, bool data );
void AddDataToKV( KeyValues* pKV, const char* name, const char* data );
void AddDataToKV( KeyValues* pKV, const char* name, const Color& data );
void AddDataToKV( KeyValues* pKV, const char* name, short data );
void AddDataToKV( KeyValues* pKV, const char* name, unsigned data );
void AddDataToKV( KeyValues* pKV, const char* name, const Vector& data );
void AddPositionDataToKV( KeyValues* pKV, const char* name, const Vector &data );
//=============================================================================

//=============================================================================
//
// Helper functions for creating key values from arrays
//
void AddArrayDataToKV( KeyValues* pKV, const char* name, const short *data, unsigned size );
void AddArrayDataToKV( KeyValues* pKV, const char* name, const byte *data, unsigned size );
void AddArrayDataToKV( KeyValues* pKV, const char* name, const unsigned *data, unsigned size );
void AddStringDataToKV( KeyValues* pKV, const char* name, const char *data );

//=============================================================================

#endif // PORTAL_GAMESTATS_SHARED_H
