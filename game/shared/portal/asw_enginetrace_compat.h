//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Alien Swarm-style brush queries on the CS:GO engine.
//
// The P2ASW Portal 2 code was written against Alien Swarm's IEngineTrace, which
// returns brush lists and brush sides in CUtlVectors. CS:GO's returns them in a
// CBrushQuery and a caller-sized array; these wrappers keep the P2ASW call sites.
//
//=============================================================================//

#ifndef ASW_ENGINETRACE_COMPAT_H
#define ASW_ENGINETRACE_COMPAT_H
#ifdef _WIN32
#pragma once
#endif

#include "engine/IEngineTrace.h"
#include "tier1/utlvector.h"

// cmodelIndex picks the brush model to search: 0 is the world, N is inline model "*N"
// (Alien Swarm could only search the world).
inline void EngineTrace_GetBrushesInAABB( IEngineTrace *pTrace, const Vector &vMins, const Vector &vMaxs, CUtlVector<int> *pOutput, int iContentsMask = 0xFFFFFFFF, int cmodelIndex = 0 )
{
	CBrushQuery query;
	pTrace->GetBrushesInAABB( vMins, vMaxs, query, iContentsMask, cmodelIndex );
	pOutput->SetCount( query.Count() );
	for ( int i = 0; i < query.Count(); ++i )
	{
		(*pOutput)[i] = (int)query[i];
	}
}

// Returns false if the brush doesn't exist.
inline bool EngineTrace_GetBrushInfo( IEngineTrace *pTrace, int iBrush, CUtlVector<BrushSideInfo_t> *pSidesOut, int *pContentsOut )
{
	int nContents = 0;
	// with no array, a negative count says how many sides the brush has
	int nSides = pTrace->GetBrushInfo( iBrush, nContents, NULL, 0 );
	if ( nSides == 0 )
	{
		if ( pSidesOut )
			pSidesOut->RemoveAll();
		return false;
	}

	if ( pSidesOut )
	{
		pSidesOut->SetCount( nSides < 0 ? -nSides : nSides );
		nSides = pTrace->GetBrushInfo( iBrush, nContents, pSidesOut->Base(), pSidesOut->Count() );
		pSidesOut->SetCount( nSides > 0 ? nSides : 0 );
	}

	if ( pContentsOut )
		*pContentsOut = nContents;
	return true;
}

#endif // ASW_ENGINETRACE_COMPAT_H
