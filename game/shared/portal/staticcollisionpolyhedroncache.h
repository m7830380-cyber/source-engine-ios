//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: Portals use polyhedrons to clip and carve their custom collision areas.
//			This file should provide caches of polyhedrons with the initial conversion 
//			processes already completed.
//
// $NoKeywords: $
//=====================================================================================//


#include "IGameSystem.h"
#include "mathlib/polyhedron.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"
#include "tier1/utlmap.h"



class CStaticCollisionPolyhedronCache : public CAutoGameSystem
{
public:
	CStaticCollisionPolyhedronCache( void );
	~CStaticCollisionPolyhedronCache( void );

	void LevelInitPreEntity( void );
	void Shutdown( void );

	const CPolyhedron *GetBrushPolyhedron( int iBrushNumber );
	int GetStaticPropPolyhedrons( ICollideable *pStaticProp, CPolyhedron **pOutputPolyhedronArray, int iOutputArraySize );

	// P2ASW's interface (its cache can also build polyhedrons on the fly, which callers
	// hand back here). This cache only ever returns cached polyhedrons, so the releases
	// only free what didn't come from it.
	int GetStaticPropPolyhedrons( ICollideable *pStaticProp, const CPolyhedron **pOutputPolyhedronArray, int iOutputArraySize )
	{
		return GetStaticPropPolyhedrons( pStaticProp, (CPolyhedron **)pOutputPolyhedronArray, iOutputArraySize );
	}
	void ReleaseBrushPolyhedron( int iBrushNumber, const CPolyhedron *pPolyhedron );
	void ReleaseStaticPropPolyhedrons( ICollideable *pStaticProp, const CPolyhedron **pPolyhedrons, int iPolyhedronCount ) {}
	void ForceRefreshOnMapLoad( void ) {}

private:
	// See comments in LevelInitPreEntity for why these members are commented out
//	CUtlString	m_CachedMap;

	CUtlVector<CPolyhedron *> m_BrushPolyhedrons;

	struct StaticPropPolyhedronCacheInfo_t
	{
		int iStartIndex;
		int iNumPolyhedrons;
		int iStaticPropIndex; //helps us remap ICollideable pointers when the map is restarted
	};

	CUtlVector<CPolyhedron *> m_StaticPropPolyhedrons;
	CUtlMap<ICollideable *, StaticPropPolyhedronCacheInfo_t> m_CollideableIndicesMap;


	void Clear( void );
	void Update( void );
};

extern CStaticCollisionPolyhedronCache g_StaticCollisionPolyhedronCache;
