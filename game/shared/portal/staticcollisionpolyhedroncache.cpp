//========= Copyright  1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=====================================================================================//

#include "cbase.h"
#include "StaticCollisionPolyhedronCache.h"
#include "engine/IEngineTrace.h"
#include "edict.h"

#include "tier0/memdbgon.h"


class CPolyhedron_LumpedMemory : public CPolyhedron //we'll be allocating one big chunk of memory for all our polyhedrons. No individual will own any memory.
{
public:
	virtual void Release( void ) { };
	static CPolyhedron_LumpedMemory *AllocateAt( void *pMemory, int iVertices, int iLines, int iIndices, int iPolygons )
	{
#include "tier0/memdbgoff.h" //the following placement new doesn't compile with memory debugging
		CPolyhedron_LumpedMemory *pAllocated = new ( pMemory ) CPolyhedron_LumpedMemory;
#include "tier0/memdbgon.h"

		pAllocated->iVertexCount = iVertices;
		pAllocated->iLineCount = iLines;
		pAllocated->iIndexCount = iIndices;
		pAllocated->iPolygonCount = iPolygons;
		pAllocated->pVertices = (Vector *)(pAllocated + 1); //start vertex memory at the end of the class
		pAllocated->pLines = (Polyhedron_IndexedLine_t *)(pAllocated->pVertices + iVertices);
		pAllocated->pIndices = (Polyhedron_IndexedLineReference_t *)(pAllocated->pLines + iLines);
		pAllocated->pPolygons = (Polyhedron_IndexedPolygon_t *)(pAllocated->pIndices + iIndices);

		return pAllocated;
	}
};

static void *s_BrushPolyhedronMemory = NULL;
static void *s_StaticPropPolyhedronMemory = NULL;

CStaticCollisionPolyhedronCache g_StaticCollisionPolyhedronCache;

// A closed convex polyhedron has V - E + F == 2, every point on 3+ lines and every line on
// exactly 2 polygons. ClipPolyhedron can't order the lines around a point of anything else.
static bool BrushPolyhedronIsClosed( const CPolyhedron *pPolyhedron )
{
	if( (pPolyhedron->iVertexCount - pPolyhedron->iLineCount + pPolyhedron->iPolygonCount) != 2 )
		return false;

	CUtlVector<int> pointLines, linePolygons;
	pointLines.SetCount( pPolyhedron->iVertexCount );
	linePolygons.SetCount( pPolyhedron->iLineCount );
	for( int i = 0; i != pointLines.Count(); ++i )
		pointLines[i] = 0;
	for( int i = 0; i != linePolygons.Count(); ++i )
		linePolygons[i] = 0;

	for( int i = 0; i != pPolyhedron->iLineCount; ++i )
	{
		for( int j = 0; j != 2; ++j )
		{
			int iPoint = pPolyhedron->pLines[i].iPointIndices[j];
			if( iPoint >= pPolyhedron->iVertexCount )
				return false;
			++pointLines[iPoint];
		}
	}

	for( int i = 0; i != pPolyhedron->iPolygonCount; ++i )
	{
		const Polyhedron_IndexedPolygon_t &polygon = pPolyhedron->pPolygons[i];
		for( int j = 0; j != polygon.iIndexCount; ++j )
		{
			int iIndex = polygon.iFirstIndex + j;
			if( iIndex >= pPolyhedron->iIndexCount )
				return false;
			int iLine = pPolyhedron->pIndices[iIndex].iLineIndex;
			if( iLine >= pPolyhedron->iLineCount )
				return false;
			++linePolygons[iLine];
		}
	}

	for( int i = 0; i != pointLines.Count(); ++i )
	{
		if( pointLines[i] < 3 )
			return false;
	}
	for( int i = 0; i != linePolygons.Count(); ++i )
	{
		if( linePolygons[i] != 2 )
			return false;
	}
	return true;
}

// Builds a brush polyhedron, retrying with other on-plane epsilons when the result isn't closed
// (sp_a1_intro3 had a wall brush come out as 8 points / 11 lines / 6 polygons, and portals placed
// on it lost that wall's collision). 1/16 is what Portal 2's own cache uses.
static CPolyhedron *GenerateClosedBrushPolyhedron( int iBrush, int iContents, const BrushSideInfo_t *pSides, const float *pPlanes, int iPlaneCount, int &iBrokenBrushes )
{
	static const float s_fEpsilons[] = { 0.01f, 1.0f / 16.0f, 0.1f, 0.001f };
	static int s_iLoggedBrushes = 0;
	bool bLog = false;

	for( int iTry = 0; iTry != ARRAYSIZE( s_fEpsilons ); ++iTry )
	{
		CPolyhedron *pPolyhedron = GeneratePolyhedronFromPlanes( pPlanes, iPlaneCount, s_fEpsilons[iTry], true );
		if( pPolyhedron == NULL )
			return NULL;

		if( BrushPolyhedronIsClosed( pPolyhedron ) )
		{
			if( bLog )
				Msg( "CStaticCollisionPolyhedronCache: brush %d rebuilt closed with epsilon %g (%d verts, %d lines, %d polygons)\n",
					iBrush, s_fEpsilons[iTry], pPolyhedron->iVertexCount, pPolyhedron->iLineCount, pPolyhedron->iPolygonCount );
			return pPolyhedron;
		}

		if( (iTry == 0) && (s_iLoggedBrushes < 10) )
		{
			++s_iLoggedBrushes;
			bLog = true;
		}

		if( bLog )
		{
			Warning( "CStaticCollisionPolyhedronCache: brush %d (contents 0x%x, %d sides) isn't closed with epsilon %g: %d verts, %d lines, %d polygons\n",
				iBrush, iContents, iPlaneCount, s_fEpsilons[iTry], pPolyhedron->iVertexCount, pPolyhedron->iLineCount, pPolyhedron->iPolygonCount );
			if( iTry == 0 )
			{
				for( int i = 0; i != iPlaneCount; ++i )
				{
					Warning( "    side %d: normal (%.9g %.9g %.9g) dist %.9g%s%s\n", i,
						pPlanes[(i * 4) + 0], pPlanes[(i * 4) + 1], pPlanes[(i * 4) + 2], pPlanes[(i * 4) + 3],
						pSides[i].bevel ? " bevel" : "", pSides[i].thin ? " thin" : "" );
				}
			}
		}
		pPolyhedron->Release();
	}

	if( bLog )
	{
		Warning( "CStaticCollisionPolyhedronCache: brush %d has no closed polyhedron, leaving it out of portal collision\n", iBrush );
	}
	++iBrokenBrushes;
	return NULL;
}

typedef ICollideable *ICollideablePtr; //needed for key comparison function syntax
static bool CollideablePtr_KeyCompareFunc( const ICollideablePtr &a, const ICollideablePtr &b )
{ 
	return a < b;
};

CStaticCollisionPolyhedronCache::CStaticCollisionPolyhedronCache( void )
: m_CollideableIndicesMap( CollideablePtr_KeyCompareFunc )
{

}

CStaticCollisionPolyhedronCache::~CStaticCollisionPolyhedronCache( void )
{
	Clear();
}

void CStaticCollisionPolyhedronCache::LevelInitPreEntity( void )
{

	// FIXME: Fast updates would be nice but this method doesn't work with the recent changes to standard containers.
	// For now we're going with the quick fix of always doing a full update. -Jeep

//	if( Q_stricmp( m_CachedMap, MapName() ) != 0 )
//	{
//		// New map or the last load was a transition, fully update the cache
//		m_CachedMap.Set( MapName() );

		Update();
//	}
//	else
//	{
//		// No need for a full update, but we need to remap static prop ICollideable's in the old system to the new system
//		for( int i = m_CollideableIndicesMap.Count(); --i >= 0; )
//		{
//#ifdef _DEBUG
//			StaticPropPolyhedronCacheInfo_t cacheInfo = m_CollideableIndicesMap.Element(i);
//#endif
//			m_CollideableIndicesMap.Reinsert( staticpropmgr->GetStaticPropByIndex( m_CollideableIndicesMap.Element(i).iStaticPropIndex ), i );
//
//			Assert( (m_CollideableIndicesMap.Element(i).iStartIndex == cacheInfo.iStartIndex) &&
//					(m_CollideableIndicesMap.Element(i).iNumPolyhedrons == cacheInfo.iNumPolyhedrons) &&
//					(m_CollideableIndicesMap.Element(i).iStaticPropIndex == cacheInfo.iStaticPropIndex) ); //I'm assuming this doesn't cause a reindex of the unordered list, if it does then this needs to be rewritten
//		}
//	}
}

void CStaticCollisionPolyhedronCache::Shutdown( void )
{
	Clear();
}


void CStaticCollisionPolyhedronCache::Clear( void )
{
	//The uses one big lump of memory to store polyhedrons. No need to Release() the polyhedrons.
	
	//Brushes
	{
		m_BrushPolyhedrons.RemoveAll();
		if( s_BrushPolyhedronMemory != NULL )
		{
			delete []s_BrushPolyhedronMemory;
			s_BrushPolyhedronMemory = NULL;
		}
	}

	//Static props
	{
		m_CollideableIndicesMap.RemoveAll();
		m_StaticPropPolyhedrons.RemoveAll();
		if( s_StaticPropPolyhedronMemory != NULL )
		{
			delete []s_StaticPropPolyhedronMemory;
			s_StaticPropPolyhedronMemory = NULL;
		}
	}
}

void CStaticCollisionPolyhedronCache::Update( void )
{
	Clear();

	Msg( "CStaticCollisionPolyhedronCache: Update begin\n" );

	//There's no efficient way to know exactly how much memory we'll need to cache off all these polyhedrons.
	//So we're going to allocated temporary workspaces as we need them and consolidate into one allocation at the end.
	const size_t workSpaceSize = 1024 * 1024; //1MB. Fairly arbitrary size for a workspace. Brushes usually use 1-3MB in the end. Static props usually use about half as much as brushes.
	const unsigned int kMaxWorkSpaces = 256;

	uint8 *workSpaceAllocations[256];
	size_t usedSpaceInWorkspace[256];
	unsigned int workSpacesAllocated = 0;
	uint8 *pCurrentWorkSpace = new uint8 [workSpaceSize];
	size_t roomLeftInWorkSpace = workSpaceSize;
	workSpaceAllocations[workSpacesAllocated] = pCurrentWorkSpace;
	usedSpaceInWorkspace[workSpacesAllocated] = 0;
	++workSpacesAllocated;
	

	//brushes
	int iBrokenBrushes = 0;
	{
		int iBrush = 0;
		float fStackPlanes[4 * 400]; //400 is a crapload of planes in my opinion

		for (;;)
		{
			int brushContents = 0;
			BrushSideInfo_t sides[400];
			const int iSideCount = enginetrace->GetBrushInfo( iBrush, brushContents, sides, ARRAYSIZE( sides ) );
			if ( iSideCount == 0 )
				break;
			if ( iSideCount < 0 )
			{
				// more sides than fit; keep m_BrushPolyhedrons indexed by brush number
				m_BrushPolyhedrons.AddToTail( NULL );
				++iBrush;
				continue;
			}

			const int iPlaneCount = iSideCount;
			AssertMsg( iPlaneCount != 0, "A brush with no planes???????" );

			const BrushSideInfo_t *pReturnedPlanes = sides;

			CPolyhedron *pTempPolyhedron;

			if( iPlaneCount > 400 )
			{
				// o_O, we'll have to get more memory to transform this brush
				float *pNonstackPlanes = new float [4 * iPlaneCount];

				for( int i = 0; i != iPlaneCount; ++i )
				{
					pNonstackPlanes[(i * 4) + 0] = pReturnedPlanes[i].plane.normal.x;
					pNonstackPlanes[(i * 4) + 1] = pReturnedPlanes[i].plane.normal.y;
					pNonstackPlanes[(i * 4) + 2] = pReturnedPlanes[i].plane.normal.z;
					pNonstackPlanes[(i * 4) + 3] = pReturnedPlanes[i].plane.dist;
				}

				pTempPolyhedron = GenerateClosedBrushPolyhedron( iBrush, brushContents, pReturnedPlanes, pNonstackPlanes, iPlaneCount, iBrokenBrushes );

				delete []pNonstackPlanes;
			}
			else
			{
				for( int i = 0; i != iPlaneCount; ++i )
				{
					fStackPlanes[(i * 4) + 0] = pReturnedPlanes[i].plane.normal.x;
					fStackPlanes[(i * 4) + 1] = pReturnedPlanes[i].plane.normal.y;
					fStackPlanes[(i * 4) + 2] = pReturnedPlanes[i].plane.normal.z;
					fStackPlanes[(i * 4) + 3] = pReturnedPlanes[i].plane.dist;
				}

				pTempPolyhedron = GenerateClosedBrushPolyhedron( iBrush, brushContents, pReturnedPlanes, fStackPlanes, iPlaneCount, iBrokenBrushes );
			}

			if( pTempPolyhedron )
			{
				size_t memRequired = (sizeof( CPolyhedron_LumpedMemory )) +
					(sizeof( Vector ) * pTempPolyhedron->iVertexCount) +
					(sizeof( Polyhedron_IndexedLine_t ) * pTempPolyhedron->iLineCount) +
					(sizeof( Polyhedron_IndexedLineReference_t ) * pTempPolyhedron->iIndexCount) +
					(sizeof( Polyhedron_IndexedPolygon_t ) * pTempPolyhedron->iPolygonCount);

				if( memRequired >= workSpaceSize || workSpacesAllocated >= kMaxWorkSpaces )
				{
					Warning( "CStaticCollisionPolyhedronCache: skipping brush polyhedron (mem=%u workspaces=%u)\n",
						(unsigned)memRequired, workSpacesAllocated );
					pTempPolyhedron->Release();
					m_BrushPolyhedrons.AddToTail( NULL );
				}
				else
				{
					if( roomLeftInWorkSpace < memRequired )
					{
						usedSpaceInWorkspace[workSpacesAllocated - 1] = workSpaceSize - roomLeftInWorkSpace;

						pCurrentWorkSpace = new uint8 [workSpaceSize];
						roomLeftInWorkSpace = workSpaceSize;
						workSpaceAllocations[workSpacesAllocated] = pCurrentWorkSpace;
						usedSpaceInWorkspace[workSpacesAllocated] = 0;
						++workSpacesAllocated;
					}

					CPolyhedron *pWorkSpacePolyhedron = CPolyhedron_LumpedMemory::AllocateAt( pCurrentWorkSpace, 
																								pTempPolyhedron->iVertexCount,
																								pTempPolyhedron->iLineCount,
																								pTempPolyhedron->iIndexCount,
																								pTempPolyhedron->iPolygonCount );

					pCurrentWorkSpace += memRequired;
					roomLeftInWorkSpace -= memRequired;

					memcpy( pWorkSpacePolyhedron->pVertices, pTempPolyhedron->pVertices, pTempPolyhedron->iVertexCount * sizeof( Vector ) );
					memcpy( pWorkSpacePolyhedron->pLines, pTempPolyhedron->pLines, pTempPolyhedron->iLineCount * sizeof( Polyhedron_IndexedLine_t ) );
					memcpy( pWorkSpacePolyhedron->pIndices, pTempPolyhedron->pIndices, pTempPolyhedron->iIndexCount * sizeof( Polyhedron_IndexedLineReference_t ) );
					memcpy( pWorkSpacePolyhedron->pPolygons, pTempPolyhedron->pPolygons, pTempPolyhedron->iPolygonCount * sizeof( Polyhedron_IndexedPolygon_t ) );

					m_BrushPolyhedrons.AddToTail( pWorkSpacePolyhedron );

					pTempPolyhedron->Release();
				}
			}
			else
			{
				m_BrushPolyhedrons.AddToTail( NULL );
			}

			++iBrush;
		}

		usedSpaceInWorkspace[workSpacesAllocated - 1] = workSpaceSize - roomLeftInWorkSpace;
		
		if( usedSpaceInWorkspace[0] != 0 ) //At least a little bit of memory was used.
		{
			//consolidate workspaces into a single memory chunk
			size_t totalMemoryNeeded = 0;
			for( unsigned int i = 0; i != workSpacesAllocated; ++i )
			{
				totalMemoryNeeded += usedSpaceInWorkspace[i];
			}

			uint8 *pFinalDest = new uint8 [totalMemoryNeeded];
			s_BrushPolyhedronMemory = pFinalDest;

			DevMsg( 2, "CStaticCollisionPolyhedronCache: Used %.2f KB to cache %d brush polyhedrons.\n", ((float)totalMemoryNeeded) / 1024.0f, m_BrushPolyhedrons.Count() );

			int iCount = m_BrushPolyhedrons.Count();
			for( int i = 0; i != iCount; ++i )
			{
				CPolyhedron_LumpedMemory *pSource = (CPolyhedron_LumpedMemory *)m_BrushPolyhedrons[i];

				if( pSource == NULL )
					continue;
				
				size_t memRequired = (sizeof( CPolyhedron_LumpedMemory )) +
										(sizeof( Vector ) * pSource->iVertexCount) +
										(sizeof( Polyhedron_IndexedLine_t ) * pSource->iLineCount) +
										(sizeof( Polyhedron_IndexedLineReference_t ) * pSource->iIndexCount) +
										(sizeof( Polyhedron_IndexedPolygon_t ) * pSource->iPolygonCount);

				CPolyhedron_LumpedMemory *pDest = (CPolyhedron_LumpedMemory *)pFinalDest;
				m_BrushPolyhedrons[i] = pDest;
				pFinalDest += memRequired;

				// intp: arm64 pointer deltas must not truncate (nillerusr / Source SDK 64-bit fix)
				intp memoryOffset = ((uint8 *)pDest) - ((uint8 *)pSource);

				memcpy( pDest, pSource, memRequired );
				//move all the pointers to their new location.
				pDest->pVertices = (Vector *)(((uint8 *)(pDest->pVertices)) + memoryOffset);
				pDest->pLines = (Polyhedron_IndexedLine_t *)(((uint8 *)(pDest->pLines)) + memoryOffset);
				pDest->pIndices = (Polyhedron_IndexedLineReference_t *)(((uint8 *)(pDest->pIndices)) + memoryOffset);
				pDest->pPolygons = (Polyhedron_IndexedPolygon_t *)(((uint8 *)(pDest->pPolygons)) + memoryOffset);
			}
		}
	}

	unsigned int iBrushWorkSpaces = workSpacesAllocated;
	workSpacesAllocated = 1;
	pCurrentWorkSpace = workSpaceAllocations[0];
	usedSpaceInWorkspace[0] = 0;
	roomLeftInWorkSpace = workSpaceSize;

	//static props
	{
		CUtlVector<ICollideable *> StaticPropCollideables;
		staticpropmgr->GetAllStaticProps( &StaticPropCollideables );

		if( StaticPropCollideables.Count() != 0 )
		{
			ICollideable **pCollideables = StaticPropCollideables.Base();
			ICollideable **pStop = pCollideables + StaticPropCollideables.Count();

			int iStaticPropIndex = 0;
			bool bStopStaticProps = false;
			do
			{
				ICollideable *pProp = *pCollideables;
				StaticPropPolyhedronCacheInfo_t cacheInfo;
				cacheInfo.iStartIndex = m_StaticPropPolyhedrons.Count();
				cacheInfo.iNumPolyhedrons = 0;
				cacheInfo.iStaticPropIndex = iStaticPropIndex;

				if( !pProp )
				{
					++iStaticPropIndex;
					++pCollideables;
					continue;
				}

				const model_t *pCollisionModel = pProp->GetCollisionModel();
				vcollide_t *pCollide = pCollisionModel ? modelinfo->GetVCollide( pCollisionModel ) : NULL;

				if( pCollide != NULL && pCollide->solids != NULL )
				{
					VMatrix matToWorldPosition( pProp->CollisionToWorldTransform() );

					for( int i = 0; i != pCollide->solidCount && !bStopStaticProps; ++i )
					{
						CPhysConvex *ConvexesArray[1024];
						int iConvexes = physcollision->GetConvexesUsedInCollideable( pCollide->solids[i], ConvexesArray, 1024 );

						for( int j = 0; j != iConvexes; ++j )
						{
							CPolyhedron *pTempPolyhedron = physcollision->PolyhedronFromConvex( ConvexesArray[j], true );
							if( pTempPolyhedron )
							{
								for( int iPointCounter = 0; iPointCounter != pTempPolyhedron->iVertexCount; ++iPointCounter )
									pTempPolyhedron->pVertices[iPointCounter] = matToWorldPosition * pTempPolyhedron->pVertices[iPointCounter];

								for( int iPolyCounter = 0; iPolyCounter != pTempPolyhedron->iPolygonCount; ++iPolyCounter )
									pTempPolyhedron->pPolygons[iPolyCounter].polyNormal = matToWorldPosition.ApplyRotation( pTempPolyhedron->pPolygons[iPolyCounter].polyNormal );

								
								size_t memRequired = (sizeof( CPolyhedron_LumpedMemory )) +
									(sizeof( Vector ) * pTempPolyhedron->iVertexCount) +
									(sizeof( Polyhedron_IndexedLine_t ) * pTempPolyhedron->iLineCount) +
									(sizeof( Polyhedron_IndexedLineReference_t ) * pTempPolyhedron->iIndexCount) +
									(sizeof( Polyhedron_IndexedPolygon_t ) * pTempPolyhedron->iPolygonCount);

								if( memRequired >= workSpaceSize )
								{
									Warning( "CStaticCollisionPolyhedronCache: skipping oversized static-prop polyhedron (%u bytes)\n", (unsigned)memRequired );
									pTempPolyhedron->Release();
									continue;
								}

								if( roomLeftInWorkSpace < memRequired )
								{
									usedSpaceInWorkspace[workSpacesAllocated - 1] = workSpaceSize - roomLeftInWorkSpace;
									
									if( workSpacesAllocated < iBrushWorkSpaces )
									{
										//re-use a workspace already allocated during brush polyhedron conversion
										pCurrentWorkSpace = workSpaceAllocations[workSpacesAllocated];
										usedSpaceInWorkspace[workSpacesAllocated] = 0;
									}
									else if( workSpacesAllocated < kMaxWorkSpaces )
									{
										//allocate a new workspace
										pCurrentWorkSpace = new uint8 [workSpaceSize];
										workSpaceAllocations[workSpacesAllocated] = pCurrentWorkSpace;
										usedSpaceInWorkspace[workSpacesAllocated] = 0;
									}
									else
									{
										Warning( "CStaticCollisionPolyhedronCache: workspace cap hit, skipping remaining static-prop polyhedrons\n" );
										pTempPolyhedron->Release();
										bStopStaticProps = true;
										break;
									}

									roomLeftInWorkSpace = workSpaceSize;
									++workSpacesAllocated;
								}

								CPolyhedron *pWorkSpacePolyhedron = CPolyhedron_LumpedMemory::AllocateAt( pCurrentWorkSpace, 
																											pTempPolyhedron->iVertexCount,
																											pTempPolyhedron->iLineCount,
																											pTempPolyhedron->iIndexCount,
																											pTempPolyhedron->iPolygonCount );

								pCurrentWorkSpace += memRequired;
								roomLeftInWorkSpace -= memRequired;

								memcpy( pWorkSpacePolyhedron->pVertices, pTempPolyhedron->pVertices, pTempPolyhedron->iVertexCount * sizeof( Vector ) );
								memcpy( pWorkSpacePolyhedron->pLines, pTempPolyhedron->pLines, pTempPolyhedron->iLineCount * sizeof( Polyhedron_IndexedLine_t ) );
								memcpy( pWorkSpacePolyhedron->pIndices, pTempPolyhedron->pIndices, pTempPolyhedron->iIndexCount * sizeof( Polyhedron_IndexedLineReference_t ) );
								memcpy( pWorkSpacePolyhedron->pPolygons, pTempPolyhedron->pPolygons, pTempPolyhedron->iPolygonCount * sizeof( Polyhedron_IndexedPolygon_t ) );

								m_StaticPropPolyhedrons.AddToTail( pWorkSpacePolyhedron );

#ifdef _DEBUG
								CPhysConvex *pConvex = physcollision->ConvexFromConvexPolyhedron( *pTempPolyhedron );
								AssertMsg( pConvex != NULL, "Conversion from Convex to Polyhedron was unreversable" );
								if( pConvex )
								{
									physcollision->ConvexFree( pConvex );
								}
#endif

								pTempPolyhedron->Release();
							}
						}
					}

					cacheInfo.iNumPolyhedrons = m_StaticPropPolyhedrons.Count() - cacheInfo.iStartIndex;
					cacheInfo.iStaticPropIndex = iStaticPropIndex;
					Assert( staticpropmgr->GetStaticPropByIndex( iStaticPropIndex ) == pProp );
					
					m_CollideableIndicesMap.InsertOrReplace( pProp, cacheInfo );
				}

				++iStaticPropIndex;
				++pCollideables;
			} while( pCollideables != pStop && !bStopStaticProps );


			usedSpaceInWorkspace[workSpacesAllocated - 1] = workSpaceSize - roomLeftInWorkSpace;

			if( usedSpaceInWorkspace[0] != 0 ) //At least a little bit of memory was used.
			{
				//consolidate workspaces into a single memory chunk
				size_t totalMemoryNeeded = 0;
				for( unsigned int i = 0; i != workSpacesAllocated; ++i )
				{
					totalMemoryNeeded += usedSpaceInWorkspace[i];
				}

				uint8 *pFinalDest = new uint8 [totalMemoryNeeded];
				s_StaticPropPolyhedronMemory = pFinalDest;

				DevMsg( 2, "CStaticCollisionPolyhedronCache: Used %.2f KB to cache %d static prop polyhedrons.\n", ((float)totalMemoryNeeded) / 1024.0f, m_StaticPropPolyhedrons.Count() );

				int iCount = m_StaticPropPolyhedrons.Count();
				for( int i = 0; i != iCount; ++i )
				{
					CPolyhedron_LumpedMemory *pSource = (CPolyhedron_LumpedMemory *)m_StaticPropPolyhedrons[i];

					size_t memRequired = (sizeof( CPolyhedron_LumpedMemory )) +
											(sizeof( Vector ) * pSource->iVertexCount) +
											(sizeof( Polyhedron_IndexedLine_t ) * pSource->iLineCount) +
											(sizeof( Polyhedron_IndexedLineReference_t ) * pSource->iIndexCount) +
											(sizeof( Polyhedron_IndexedPolygon_t ) * pSource->iPolygonCount);

					CPolyhedron_LumpedMemory *pDest = (CPolyhedron_LumpedMemory *)pFinalDest;
					m_StaticPropPolyhedrons[i] = pDest;
					pFinalDest += memRequired;

					intp memoryOffset = ((uint8 *)pDest) - ((uint8 *)pSource);

					memcpy( pDest, pSource, memRequired );
					//move all the pointers to their new location.
					pDest->pVertices = (Vector *)(((uint8 *)(pDest->pVertices)) + memoryOffset);
					pDest->pLines = (Polyhedron_IndexedLine_t *)(((uint8 *)(pDest->pLines)) + memoryOffset);
					pDest->pIndices = (Polyhedron_IndexedLineReference_t *)(((uint8 *)(pDest->pIndices)) + memoryOffset);
					pDest->pPolygons = (Polyhedron_IndexedPolygon_t *)(((uint8 *)(pDest->pPolygons)) + memoryOffset);
				}
			}
		}
	}

	if( iBrushWorkSpaces > workSpacesAllocated )
		workSpacesAllocated = iBrushWorkSpaces;

	for( unsigned int i = 0; i != workSpacesAllocated; ++i )
	{
		delete []workSpaceAllocations[i];
	}

	Msg( "CStaticCollisionPolyhedronCache: Update done (%d brushes, %d not closed, %d static-prop polyhedrons)\n",
		m_BrushPolyhedrons.Count(), iBrokenBrushes, m_StaticPropPolyhedrons.Count() );
}



void CStaticCollisionPolyhedronCache::ReleaseBrushPolyhedron( int iBrushNumber, const CPolyhedron *pPolyhedron )
{
	// only a polyhedron that isn't the cached one for this brush was built for the caller
	if( pPolyhedron && ( (iBrushNumber < 0) || (iBrushNumber >= m_BrushPolyhedrons.Count()) || (pPolyhedron != m_BrushPolyhedrons[iBrushNumber]) ) )
	{
		((CPolyhedron *)pPolyhedron)->Release();
	}
}

const CPolyhedron *CStaticCollisionPolyhedronCache::GetBrushPolyhedron( int iBrushNumber )
{
	Assert( iBrushNumber < m_BrushPolyhedrons.Count() );

	if( (iBrushNumber < 0) || (iBrushNumber >= m_BrushPolyhedrons.Count()) )
		return NULL;

	return m_BrushPolyhedrons[iBrushNumber];
}

int CStaticCollisionPolyhedronCache::GetStaticPropPolyhedrons( ICollideable *pStaticProp, CPolyhedron **pOutputPolyhedronArray, int iOutputArraySize )
{
	unsigned short iPropIndex = m_CollideableIndicesMap.Find( pStaticProp );
	if( !m_CollideableIndicesMap.IsValidIndex( iPropIndex ) ) //static prop never made it into the cache for some reason (specifically no collision data when this workaround was written)
		return 0;

	StaticPropPolyhedronCacheInfo_t cacheInfo = m_CollideableIndicesMap.Element( iPropIndex );

	if( cacheInfo.iNumPolyhedrons < iOutputArraySize )
		iOutputArraySize = cacheInfo.iNumPolyhedrons;

	for( int i = cacheInfo.iStartIndex, iWriteIndex = 0; iWriteIndex != iOutputArraySize; ++i, ++iWriteIndex )
	{
		pOutputPolyhedronArray[iWriteIndex] = m_StaticPropPolyhedrons[i];
	}

	return iOutputArraySize;
}






