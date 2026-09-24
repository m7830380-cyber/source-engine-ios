//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#if defined( DX_TO_GL_ABSTRACTION )
#include "tier1/keyvalues.h"
#endif
#include "shaderapi/ishaderapi.h"
 
// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

using namespace SF::GFx;
using namespace SF::Render;

#if (defined(_DEBUG) || defined(USE_MEM_DEBUG))
	#define	MEM_ALLOC_CREDIT_FORMATF( formatStr, slotNumber )				\
		char szName[64];													\
		V_snprintf( szName, ARRAYSIZE(szName), formatStr, slotNumber );		\
		MEM_ALLOC_CREDIT_( szName );
#else
	#define MEM_ALLOC_CREDIT_FORMATF (void)
#endif

#if defined( _PS3 ) || defined( _X360 )

ConVar scaleform_mesh_caching_enable( "scaleform_mesh_caching_enable", "1" );

static void scaleform_dump_mesh_caching_stats_f( const CCommand &args );
ConCommand scaleform_dump_mesh_caching_stats( "scaleform_dump_mesh_caching_stats", scaleform_dump_mesh_caching_stats_f, "Dumps stats about scaleform mesh caching" );

bool g_bScaleformMeshCaching = true; // Cache VBs & IBs for scaleform batches to reuse across frames

// Counters to track usage of scaleform mesh caching
int g_nScaleformCachedVBAlive = 0; // number of VBs currently allocated for mesh caching
int g_nScaleformCachedIBAlive = 0; // number of IBs currently allocated for mesh caching
int g_nScaleformCachedVBDead = 0; // number of cached VBs that have been cleaned up (add to # alive to get total ever allocated)
int g_nScaleformCachedIBDead = 0; // number of cached IBs that have been cleaned up (add to # alive to get total ever allocated)

static void scaleform_dump_mesh_caching_stats_f( const CCommand &args )
{
	Msg( "VBs alive: %d\nIBs alive: %d\nVBs dead: %d\nIBs dead: %d\n", g_nScaleformCachedVBAlive, g_nScaleformCachedIBAlive, g_nScaleformCachedVBDead, g_nScaleformCachedIBDead );
}

#endif

ConVar r_drawscaleform( "r_drawscaleform", "1", 
#if defined( DEVELOPMENT_ONLY ) || defined( ALLOW_TEXT_MODE )
	FCVAR_RELEASE 
#else
	0
#endif
);

void ScaleformUIImpl::InitMovieSlotImpl( void )
{
	V_memset( m_SlotPtrs, 0, sizeof( m_SlotPtrs ) );
	V_memset( m_SlotDeniesInputRefCount, 0, sizeof( m_SlotDeniesInputRefCount ) );
}

void ScaleformUIImpl::ShutdownMovieSlotImpl( void )
{
#if defined ( _DEBUG )
	for ( int i = 0; i < MAX_SLOTS; i++ )
	{
		const char* slotName;

		if ( m_SlotPtrs[i] != NULL )
		{
			switch( i )
			{
				case SF_RESERVED_CURSOR_SLOT:
					slotName = "Cursor";
					break;

				case SF_FULL_SCREEN_SLOT:
					slotName = "Full Screen";
					break;

				case SF_FIRST_SS_SLOT:
					slotName = "Base Client";
					break;

				default:
					slotName= "Other Split Screen";
					break;

			}

			LogPrintf( "Scaleform: UI slot \"%s Slot\" ( #%d ) not released\n", slotName, i );
		}
	}
#endif

}

BaseSlot* ScaleformUIImpl::LockSlotPtr( int slot )
{
	AssertMsg( slot >= 0 && slot < MAX_SLOTS, "Invalid slot index in LockSlotPtr" ); 

	// gurjeets - locks commented out, left here for reference
	
	// Currently we only queue a couple of SF-related functions to be called from the render thread (see cmatqueuedrendercontext.h) 
	// These are RenderSlot() and SetSlotViewport(). It's safe to call these in parallel along with whatever's 
	// happening on the main thread. They are also called from the main thread but only at times when when QMS is not enabled
	// The Lock/Unlock slot ptr functions are effectively just for ref counting, which is thread safe
	//m_SlotMutexes[slot].Lock();

	BaseSlot* presult = m_SlotPtrs[slot];

	if ( presult )
	{
		presult->AddRef();
	}

	return presult;
}


void ScaleformUIImpl::UnlockSlotPtr( int slot )
{
	AssertMsg( slot >= 0 && slot < MAX_SLOTS, "Invalid slot index in UnlockSlotPtr" ); 

	if ( m_SlotPtrs[slot] && m_SlotPtrs[slot]->Release() )
	{
		m_SlotPtrs[slot] = NULL;
	}

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//m_SlotMutexes[slot].Unlock();	
}

void ScaleformUIImpl::LockSlot( int slot )
{
	AssertMsg( slot >= 0 && slot < MAX_SLOTS, "Invalid slot index in LockSlot" ); 

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//m_SlotMutexes[slot].Lock();
}

void ScaleformUIImpl::UnlockSlot( int slot )
{
	AssertMsg( slot >= 0 && slot < MAX_SLOTS, "Invalid slot index in UnlockSlot" ); 

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//m_SlotMutexes[slot].Unlock();
}



void ScaleformUIImpl::SlotAddRef( int slot )
{
	AssertMsg( slot >= 0 && slot < MAX_SLOTS, "Invalid slot index in SlotAddRef" ); 

	LockSlotPtr( slot );

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//m_SlotMutexes[slot].Unlock();
}

void ScaleformUIImpl::SlotRelease( int slot )
{
	AssertMsg( slot >= 0 && slot < MAX_SLOTS, "Invalid slot index in SlotRelease" ); 

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//m_SlotMutexes[slot].Lock();
	UnlockSlotPtr( slot );
}

IScaleformSlotInitController *g_pExternalScaleformSlotInitController = NULL;
void ScaleformUIImpl::InitSlot( int slotID, const char* rootMovie, IScaleformSlotInitController *pController )
{
	g_pExternalScaleformSlotInitController = pController;
	MEM_ALLOC_CREDIT_FORMATF( "ScaleformUIImpl::InitSlot%d", slotID );

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//m_SlotMutexes[slotID].Lock();

	if ( !m_SlotPtrs[slotID] )
	{
		MovieSlot* slotptr = new MovieSlot();

		m_SlotPtrs[slotID] = slotptr;
		m_SlotDeniesInputRefCount[slotID] = 0;

		slotptr->Init( rootMovie, slotID );

		if ( pController )
			pController->ConfigureNewSlotPostInit( slotID );
	}
	else
	{
		m_SlotPtrs[slotID]->AddRef();
	}

	SFDevMsg("ScaleformUIImpl::InitSlot( %d, %s) refcount=%d\n", slotID, rootMovie, m_SlotPtrs[slotID]->m_iRefCount);

	// gurjeets - locks commented out, left here for reference, see comment in LockSlotPtr
	//m_SlotMutexes[slotID].Unlock();

}


void ScaleformUIImpl::RequestElement( int slot, const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject )
{
	MEM_ALLOC_CREDIT();
	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		SFDevMsg("ScaleformUIImpl::RequestElement( %d, %s)\n", slot, elementName);
		pslot->RequestElement( elementName, object, tableObject );
	}

	UnlockSlotPtr( slot );
}

void ScaleformUIImpl::RemoveElement( int slot, SFVALUE element )
{
	MEM_ALLOC_CREDIT();
	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		pslot->RemoveElement( (Scaleform::GFx::Value*)element );
	}

	UnlockSlotPtr( slot );
}

void ScaleformUIImpl::InstallGlobalObject( int slot, const char* elementName, ScaleformUIFunctionHandlerObject* object, const IScaleformUIFunctionHandlerDefinitionTable* tableObject, SFVALUE *pInstalledGlobalObjectResult )
{
	MEM_ALLOC_CREDIT();
	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		SFDevMsg("ScaleformUIImpl::InstallGlobalObject( %d, %s)\n", slot, elementName);
		pslot->InstallGlobalObject( elementName, object, tableObject, (Scaleform::GFx::Value* *) pInstalledGlobalObjectResult );
	}

	UnlockSlotPtr( slot );
}

void ScaleformUIImpl::RemoveGlobalObject( int slot, SFVALUE element )
{
	MEM_ALLOC_CREDIT();
	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		pslot->RemoveGlobalObject( (Scaleform::GFx::Value*)element );
	}

	UnlockSlotPtr( slot );
}

void ScaleformUIImpl::SetSlotViewport( int slot, int x, int y, int width, int height )
{
	MEM_ALLOC_CREDIT();
	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		pslot->m_pMovieView->SetViewport( m_iScreenWidth, m_iScreenHeight, x, y, width, height );
	}

	UnlockSlotPtr( slot );

}

#if defined( SF_USE_ANGLE )
extern int SF_DebugFrameLog;
extern int SF_IOSBlendDirect, SF_IOSTextOnly, SF_IOSFilters;
extern int SF_StatPrimitives, SF_StatText, SF_StatComplex, SF_StatBlendPush,
           SF_StatBlendTargets, SF_StatRenderTargets, SF_StatFilters, SF_StatMasks;

// iOS Scaleform switches, changeable from the console without a rebuild
ConVar sf_ios_blend_direct( "sf_ios_blend_direct", "0", 0, "iOS: draw offscreen blend-mode content (layer/multiply/...) directly instead of compositing it" );
ConVar sf_ios_filters( "sf_ios_filters", "1", 0, "iOS: render Scaleform filters (glow/drop shadow/blur); 0 draws filtered content unfiltered" );
ConVar sf_ios_flip_menu( "sf_ios_flip_menu", "1", FCVAR_ARCHIVE, "iOS: menu orientation, 0 none, 1 flip vertical, 2 flip horizontal, 3 both" );
ConVar sf_ios_flip_hud( "sf_ios_flip_hud", "1", FCVAR_ARCHIVE, "iOS: in-game HUD orientation, 0 none, 1 flip vertical, 2 flip horizontal, 3 both" );
ConVar sf_ios_text_only( "sf_ios_text_only", "0", 0, "iOS visual test: draw only text primitives" );
ConVar sf_ios_record_frames( "sf_ios_record_frames", "0", 0, "iOS: record the draw order of the next N menu frames to the log" );


// Dump the display tree of the menu movie (names, visibility, alpha, position,
// size) to see which clip covers the menu: once ~20 s into the menu and on
// request (sf_ios_dump_ui). sf_ios_hide <path> hides a clip for testing.
static bool s_bIOSDumpUIRequested = false;
static char s_szIOSHidePath[256] = "";
CON_COMMAND( sf_ios_dump_ui, "iOS: log the display tree of the menu movie" )
{
	s_bIOSDumpUIRequested = true;
}
CON_COMMAND( sf_ios_hide, "iOS: hide a menu movie clip, e.g. sf_ios_hide _level31.Panel" )
{
	if ( args.ArgC() < 2 )
		return;
	V_strncpy( s_szIOSHidePath, args[1], sizeof( s_szIOSHidePath ) );
}

class CIOSDisplayTreeVisitor : public SF::GFx::Value::ObjectVisitor
{
public:
	CIOSDisplayTreeVisitor( int nDepth, int *pnLines ) : m_nDepth( nDepth ), m_pnLines( pnLines ) {}

	virtual void Visit( const char *name, const SF::GFx::Value &val )
	{
		if ( !val.IsDisplayObject() || *m_pnLines >= 600 )
			return;
		if ( !V_strcmp( name, "_parent" ) || !V_strcmp( name, "_root" ) || !V_strcmp( name, "this" ) || !V_strncmp( name, "_level", 6 ) )
			return;

		SF::GFx::Value::DisplayInfo info;
		val.GetDisplayInfo( &info );
		SF::GFx::Value width, height;
		val.GetMember( "_width", &width );
		val.GetMember( "_height", &height );
		++*m_pnLines;
		printf( "[sf-ui] %*s%s: visible %d, alpha %.0f, at %.0f,%.0f, size %.0fx%.0f\n", m_nDepth * 2, "", name,
				info.GetVisible() ? 1 : 0, info.GetAlpha(), info.GetX(), info.GetY(),
				width.IsNumber() ? width.GetNumber() : -1.0, height.IsNumber() ? height.GetNumber() : -1.0 );

		if ( m_nDepth < 3 && info.GetVisible() )
		{
			CIOSDisplayTreeVisitor child( m_nDepth + 1, m_pnLines );
			val.VisitMembers( &child );
		}
	}

private:
	int m_nDepth;
	int *m_pnLines;
};

static void IOS_DumpDisplayTree( SF::GFx::Movie *pMovie, int slot )
{
	if ( !pMovie )
		return;
	printf( "[sf-ui] ===== display tree of slot %d =====\n", slot );
	int nLines = 0;
	for ( int nLevel = 0; nLevel < 64; nLevel++ )
	{
		char szLevel[16];
		V_snprintf( szLevel, sizeof( szLevel ), "_level%d", nLevel );
		SF::GFx::Value level;
		if ( !pMovie->GetVariable( &level, szLevel ) || !level.IsDisplayObject() )
			continue;
		SF::GFx::Value::DisplayInfo info;
		level.GetDisplayInfo( &info );
		printf( "[sf-ui] %s: visible %d, alpha %.0f\n", szLevel, info.GetVisible() ? 1 : 0, info.GetAlpha() );
		CIOSDisplayTreeVisitor visitor( 1, &nLines );
		level.VisitMembers( &visitor );
	}
	printf( "[sf-ui] ===== end (%d clips) =====\n", nLines );
	fflush( stdout );
}
#endif

static bool s_bScaleformInFrame = false;
void ScaleformUIImpl::RenderSlot( int slot )
{
	if  ( !r_drawscaleform.GetBool() )
		return;

	MEM_ALLOC_CREDIT_FORMATF( "ScaleformUIImpl::RenderSlot%d", slot );

#if defined( SF_USE_ANGLE )
	{
		static int s_nRenderCalls = 0;
		if ( ( s_nRenderCalls++ % 600 ) == 0 )
		{
			printf( "[sf] RenderSlot %d (call %d), renderer2D %p, HAL %p, in frame %d\n", slot, s_nRenderCalls,
					(void *)m_pRenderer2D.GetPtr(), (void *)m_pRenderHAL.GetPtr(), s_bScaleformInFrame ? 1 : 0 );
			fflush( stdout );
		}
	}
#endif

	if (slot == SF_RESERVED_BEGINFRAME_SLOT)
	{
		m_pShaderAPI->ResetRenderState( false );
#if defined( DX_TO_GL_ABSTRACTION )
// Removed for Linux merge (trunk in 2002 -> //console/csgo/trunk in 2001) - do we need these
//		m_pDevice->FlushStates( 0xFFFFFFFF );
//		m_pDevice->FlushSamplers();
#endif
		s_bScaleformInFrame = m_pRenderer2D->BeginFrame();
		return;
	}

	if (slot == SF_RESERVED_ENDFRAME_SLOT)
	{
		m_pRenderer2D->EndFrame();
		m_pShaderAPI->ResetRenderState( false );
#if defined( DX_TO_GL_ABSTRACTION )
// Removed for Linux merge (trunk in 2002 -> //console/csgo/trunk in 2001) - do we need these
//		m_pDevice->FlushStates( 0xFFFFFFFF );
//		m_pDevice->FlushSamplers();
#endif
		return;
	}

	if ( !s_bScaleformInFrame )
	{
		// Device lost, but still need to call NextCapture to avoid leaking memory
		BaseSlot* pslot = LockSlotPtr( slot );

		if (pslot)
		{
			MovieDisplayHandle hMovieDisplay = ((Movie*)pslot->m_pMovieView)->GetDisplayHandle();
			hMovieDisplay.NextCapture( m_pRenderer2D->GetContextNotify() );
		}

		UnlockSlotPtr( slot );
		
		return;
	}

	SaveRenderingState();
    
	if ( m_pRenderHAL )
	{
		m_pRenderHAL->GetTextureManager()->SetRenderThreadIdToCurrentThread();

		if ( m_bClearMeshCacheQueued )
		{
			// Clear the mesh cache to recover memory.  The mesh cache will clear itself after hitting a threshold,
			//	but in practice this threshold can large enough that it bleeds a lot of memory on console.
			//	We also want to avoid performance spikes caused by clearing and refilling this cache in the middle
			//	of gameplay.  So this accessor allows us to reset the cache on demand at a less noticeable point,
			//	e.g. the end of a round, or when we transition between maps.

			MeshCache& meshCache = m_pRenderHAL->GetMeshCache();
			meshCache.ClearCache();

			m_bClearMeshCacheQueued = false;
		}

#if defined( SF_USE_ANGLE )
		// iOS: orientation per slot, switchable on the device
		// (bit 0 flips vertically like Linux, bit 1 flips horizontally)
		{
			int nFlip = ( slot == SF_FULL_SCREEN_SLOT || slot == SF_RESERVED_CURSOR_SLOT ) ? sf_ios_flip_menu.GetInt() : sf_ios_flip_hud.GetInt();
			SF::Render::Matrix2F matrix;
			if ( nFlip & 1 )
			{
				matrix.Sy() = -1.0f;
				matrix.Ty() = m_iScreenHeight;
			}
			if ( nFlip & 2 )
			{
				matrix.Sx() = -1.0f;
				matrix.Tx() = m_iScreenWidth;
			}
			m_pRenderHAL->SetUserMatrix(matrix);
		}
#elif defined( DX_TO_GL_ABSTRACTION )
		// On Linux, we have to flip the display.
		SF::Render::Matrix2F matrix;
		matrix.Sy() = -1.0f;
		matrix.Ty() = m_iScreenHeight;
		m_pRenderHAL->SetUserMatrix(matrix);
#endif
	}
	
	BaseSlot* pslot = LockSlotPtr( slot );

#if defined( SF_USE_ANGLE )
	// per slot: the first 3 renders, then every 600th
	static int s_nSlotRenders[64] = {};
	int nSlotIndex = ( slot >= 0 && slot < 64 ) ? slot : 63;
	int nSlotRender = s_nSlotRenders[nSlotIndex]++;
	bool bLogSlot = nSlotRender < 3 || ( nSlotRender % 600 ) == 0;
	if ( bLogSlot )
	{
		printf( "[sf] slot %d render %d: movie %p, screen %dx%d\n", slot, nSlotRender,
				pslot ? (void *)pslot->m_pMovieView : NULL, m_iScreenWidth, m_iScreenHeight );
		SFTogl_LogGLState( "before display", slot );
	}
#endif

#if defined( SF_USE_ANGLE )
	SF_IOSBlendDirect = sf_ios_blend_direct.GetBool() ? 1 : 0;
	SF_IOSTextOnly = sf_ios_text_only.GetBool() ? 1 : 0;
	SF_IOSFilters = sf_ios_filters.GetBool() ? 1 : 0;

	// record the draw order of menu frames: automatically at ~10s, 20s and 40s
	// of menu rendering, and on request (sf_ios_record_frames N)
	bool bRecord = false;
	if ( slot == 1 || slot == 2 )	// main menu movie is slot 1, the HUD slot 2
	{
		if ( nSlotRender == 600 || nSlotRender == 1200 || nSlotRender == 2400 )
			bRecord = true;
		else if ( sf_ios_record_frames.GetInt() > 0 )
		{
			bRecord = true;
			sf_ios_record_frames.SetValue( sf_ios_record_frames.GetInt() - 1 );
		}
	}
	SF_DebugFrameLog = bRecord ? 1 : 0;
	if ( SF_DebugFrameLog )
		printf( "[sf-frame] ===== slot %d render %d (blend direct %d, text only %d) =====\n", slot, nSlotRender, SF_IOSBlendDirect, SF_IOSTextOnly );

	SF_StatPrimitives = SF_StatText = SF_StatComplex = SF_StatBlendPush = 0;
	SF_StatBlendTargets = SF_StatRenderTargets = SF_StatFilters = SF_StatMasks = 0;
#endif

#if defined( SF_USE_ANGLE )
	if ( pslot && slot == 1 && s_szIOSHidePath[0] )
	{
		char szVar[300];
		V_snprintf( szVar, sizeof( szVar ), "%s._visible", s_szIOSHidePath );
		bool bOk = ( (SF::GFx::Movie *)pslot->m_pMovieView )->SetVariable( szVar, SF::GFx::Value( false ) );
		printf( "[sf-ui] hide %s: %s\n", s_szIOSHidePath, bOk ? "done" : "not found" );
		fflush( stdout );
		s_szIOSHidePath[0] = 0;
	}
	if ( pslot && slot == 1 && ( nSlotRender == 1200 || s_bIOSDumpUIRequested ) )
	{
		s_bIOSDumpUIRequested = false;
		IOS_DumpDisplayTree( (SF::GFx::Movie *)pslot->m_pMovieView, slot );
	}
#endif

	if ( pslot )
	{
		MovieView_Display( ToSFMOVIE( pslot->m_pMovieView ) );
	}

#if defined( SF_USE_ANGLE )
	if ( m_pRenderHAL && slot >= 0 && slot < 64 )
	{
		static int s_nLastFlip[64] = { -1, -1, -1, -1, -1, -1, -1, -1 };
		int nFlip = ( slot == SF_FULL_SCREEN_SLOT || slot == SF_RESERVED_CURSOR_SLOT ) ? sf_ios_flip_menu.GetInt() : sf_ios_flip_hud.GetInt();
		if ( nFlip != s_nLastFlip[slot] || ( nSlotRender % 600 ) == 0 )
		{
			s_nLastFlip[slot] = nFlip;
			const SF::Render::MatrixState *pm = m_pRenderHAL->GetMatrices();
			printf( "[sf-orient] slot %d flip %d: user [%.2f %.2f %.0f / %.2f %.2f %.0f], view [%.5f %.5f %.3f / %.5f %.5f %.3f], orient [%.2f %.2f / %.2f %.2f], final [%.5f %.5f %.3f / %.5f %.5f %.3f]\n",
					slot, nFlip,
					pm->User.Sx(), pm->User.Shx(), pm->User.Tx(), pm->User.Shy(), pm->User.Sy(), pm->User.Ty(),
					pm->View2D.Sx(), pm->View2D.Shx(), pm->View2D.Tx(), pm->View2D.Shy(), pm->View2D.Sy(), pm->View2D.Ty(),
					pm->Orient2D.Sx(), pm->Orient2D.Shx(), pm->Orient2D.Shy(), pm->Orient2D.Sy(),
					pm->UserView.Sx(), pm->UserView.Shx(), pm->UserView.Tx(), pm->UserView.Shy(), pm->UserView.Sy(), pm->UserView.Ty() );
			fflush( stdout );
		}
	}
#endif

#if defined( SF_USE_ANGLE )
	if ( SF_DebugFrameLog || ( nSlotRender % 600 ) == 0 )
	{
		printf( "[sf-stats] slot %d render %d: primitives %d (text %d), shapes %d, blend modes %d (offscreen %d), render targets %d, filters %d, masks %d, blend direct %d, text only %d, filters on %d\n",
				slot, nSlotRender, SF_StatPrimitives, SF_StatText, SF_StatComplex, SF_StatBlendPush, SF_StatBlendTargets,
				SF_StatRenderTargets, SF_StatFilters, SF_StatMasks, SF_IOSBlendDirect, SF_IOSTextOnly, SF_IOSFilters );
		if ( SF_DebugFrameLog )
			printf( "[sf-frame] ===== end =====\n" );
		fflush( stdout );
	}
	SF_DebugFrameLog = 0;
#endif

#if defined( SF_USE_ANGLE )
	if ( bLogSlot )
		SFTogl_LogGLState( "after display", slot );
#endif

	UnlockSlotPtr( slot );
	RestoreRenderingState();

}

void ScaleformUIImpl::ForkRenderSlot( int slot )
{
}

void ScaleformUIImpl::JoinRenderSlot( int slot )
{
}

void ScaleformUIImpl::AdvanceSlot( int slot )
{	
	if ( !r_drawscaleform.GetBool() ) 
		return;

	MEM_ALLOC_CREDIT_FORMATF( "ScaleformUIImpl::AdvanceSlot%d", slot );

	BaseSlot* pslot = LockSlotPtr( slot );
	
	if ( pslot )
	{		
			// Using m_fTime set in RunFrame
			pslot->Advance( m_fTime );
	}

	UnlockSlotPtr( slot );
}

bool ScaleformUIImpl::SlotConsumesInputEvents( int slot )
{
	MEM_ALLOC_CREDIT_FORMATF( "ScaleformUIImpl::SlotConsumesInputEvents%d", slot );

	bool result = false;

	BaseSlot* pslot = m_SlotPtrs[ slot ];

	if ( pslot )
	{
		result = pslot->ConsumesInputEvents();
	}

	return result;
}

bool ScaleformUIImpl::SlotDeniesInputToGame( int slot )
{
	MEM_ALLOC_CREDIT_FORMATF( "ScaleformUIImpl::SlotDeniesInputToGame%d", slot );

	if ( m_bDenyAllInputToGame )
		return true;

	if ( slot < MAX_SLOTS )
		return ( m_SlotDeniesInputRefCount[slot] > 0 );
	else
		return false;
}

void ScaleformUIImpl::DenyInputToGameFromFlash( int slot, bool value )
{
	if ( value )
	{
		m_SlotDeniesInputRefCount[slot]++;
	}
	else
	{
		Assert( m_SlotDeniesInputRefCount[slot] > 0 );
		m_SlotDeniesInputRefCount[slot]--;
	}
	SFDevMsg( "ScaleformUIImpl::DenyInputToGameFromFlash(%d,%d) m_SlotDeniesInputRefCount[%d]=%d \n", slot, value?1:0, slot, m_SlotDeniesInputRefCount[slot] );
}

bool ScaleformUIImpl::AnalogStickNavigationDisabled( int slot )
{
	MEM_ALLOC_CREDIT_FORMATF( "ScaleformUIImpl::AnalogStickNavigationDisabled%d", slot );	

	bool result = false;

	if ( slot < MAX_SLOTS )
	{
		BaseSlot* pslot = m_SlotPtrs[ slot ];

		if ( pslot )
		{
			MovieSlot* pMovieSlot = dynamic_cast<MovieSlot*>( pslot );
			if ( pMovieSlot )
			{
				result = pMovieSlot->AnalogStickNavigationDisabled();
			}
		}
	}

	return result;

}

void ScaleformUIImpl::UpdateSafeZone( void )
{
	MEM_ALLOC_CREDIT();

	for ( int i = SF_FIRST_UNRESERVED_SLOT; i < MAX_SLOTS; i++ )
	{
		BaseSlot* pslot = LockSlotPtr( i );

		if ( pslot )
		{
			pslot->UpdateSafeZone();
		}

		UnlockSlotPtr( i );

	}
}

void ScaleformUIImpl::UpdateTint( void )
{
	MEM_ALLOC_CREDIT();

	for ( int i = SF_FIRST_UNRESERVED_SLOT; i < MAX_SLOTS; i++ )
	{
		BaseSlot* pslot = LockSlotPtr( i );

		if ( pslot )
		{
			pslot->UpdateTint();
		}

		UnlockSlotPtr( i );
	}
}

bool ScaleformUIImpl::ConsumesInputEvents( void )
{
	MEM_ALLOC_CREDIT();

	if ( m_bDenyAllInputToGame )
		return true;

	for ( int i = SF_FIRST_UNRESERVED_SLOT; i < MAX_SLOTS; i++ )
	{
		if ( SlotConsumesInputEvents( i ) )
		{
			return true;
		}
	}

	return false;
}

void ScaleformUIImpl::ForceUpdateImages()
{
	MEM_ALLOC_CREDIT();
	
	for ( int i = SF_FIRST_UNRESERVED_SLOT; i < MAX_SLOTS; i++ )
	{
		BaseSlot* pSlot = LockSlotPtr( i );

		if ( pSlot && pSlot->m_pMovieView )
		{
			pSlot->m_pMovieView->ForceUpdateImages();
		}

		UnlockSlotPtr( i );
	}
}

void ScaleformUIImpl::DenyInputToGame( bool value )
{
	m_bDenyAllInputToGame = value;
	SFDevMsg( "ScaleformUIImpl::DenyInputToGame(%d)\n", value?1:0 );
}

SFVALUE ScaleformUIImpl::CreateNewObject( int slot )
{
	MEM_ALLOC_CREDIT();

	SFVALUE result = NULL;

	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		Movie* pmovie = pslot->m_pMovieView;

		Value* pResult = (Value*)CreateGFxValue();

		pmovie->CreateObject( pResult );

		result = ( SFVALUE )pResult;
	}

	UnlockSlotPtr( slot );

	return result;
}

SFVALUE ScaleformUIImpl::CreateNewArray( int slot, int size )
{
	MEM_ALLOC_CREDIT();
	SFVALUE result = NULL;

	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		Movie* pmovie = pslot->m_pMovieView;

		Value* pResult = (Value*)CreateGFxValue();

		pmovie->CreateArray( pResult );

		if ( size != -1 )
			pResult->SetArraySize( size );

		result = ( SFVALUE )pResult;
	}

	UnlockSlotPtr( slot );

	return result;
}

SFVALUE ScaleformUIImpl::CreateNewString( int slot, const char* value )
{
	MEM_ALLOC_CREDIT();
	SFVALUE result = NULL;

	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		Movie* pmovie = pslot->m_pMovieView;

		Value* pResult = (Value*)CreateGFxValue();

		pmovie->CreateString( pResult, value );

		result = ( SFVALUE )pResult;
	}

	UnlockSlotPtr( slot );

	return result;
}

SFVALUE ScaleformUIImpl::CreateNewString( int slot, const wchar_t* value )
{
	MEM_ALLOC_CREDIT();
	SFVALUE result = NULL;

	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		Movie* pmovie = pslot->m_pMovieView;

		Value* pResult = (Value*)CreateGFxValue();

		pmovie->CreateStringW( pResult, value );

		result = ( SFVALUE )pResult;
	}

	UnlockSlotPtr( slot );

	return result;
}

void ScaleformUIImpl::LockInputToSlot( int slot )
{
	MEM_ALLOC_CREDIT_FORMATF( "ScaleformUIImpl::LockInputToSlot%d", slot );

	BaseSlot* pslot = LockSlotPtr( SF_FULL_SCREEN_SLOT );

	if ( pslot )
	{
		pslot->LockInputToSlot( slot );
	}

	UnlockSlotPtr( SF_FULL_SCREEN_SLOT );


}

void ScaleformUIImpl::UnlockInput( void )
{
	MEM_ALLOC_CREDIT();

	BaseSlot* pslot = LockSlotPtr( SF_FULL_SCREEN_SLOT );

	if ( pslot )
	{
		pslot->UnlockInput();
	}

	UnlockSlotPtr( SF_FULL_SCREEN_SLOT );
}

void ScaleformUIImpl::ForceCollectGarbage( int slot )
{
	BaseSlot* pslot = m_SlotPtrs[ slot ];

	if ( pslot )
	{
		pslot->ForceCollectGarbage(  );
	}
}

void ScaleformUIImpl::SetToControllerUI( int slot, bool value )
{
	BaseSlot* pslot = LockSlotPtr( slot );

	if ( pslot )
	{
		pslot->SetToControllerUI( value, true );
	}

	UnlockSlotPtr( slot );
}


void ScaleformUIImpl::LockMostRecentInputDevice( int slot )
{
	BaseSlot* pslot = m_SlotPtrs[ slot ];

	if ( pslot )
	{
		pslot->LockMostRecentInputDevice();
	}
}


bool ScaleformUIImpl::IsSetToControllerUI( int slot )
{
	bool result = false;

	BaseSlot* pslot = m_SlotPtrs[ slot ];

	if ( pslot == NULL || !pslot->ConsumesInputEvents() )
	{
		// Specified slot does not consume input events, or does not exist. Test the full screen slot instead.
		slot = SF_FULL_SCREEN_SLOT;
		pslot = m_SlotPtrs[ slot ];
	}

	if ( pslot )
	{
		result = pslot->IsSetToControllerUI();
	}

	return result;
}
