//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: togl / launcher manager calls for the iOS build of scaleformui.
//          Deliberately includes no Scaleform headers (see sf_togl_bridge.h).
//
//===========================================================================//

#include "togl/rendermechanism.h"
#include "appframework/ilaunchermgr.h"

#include "sf_togl_bridge.h"

// togl's gGL is not exported from libtogl; its accessor is (and returns the
// already created entry points)
static COpenGLEntryPoints *SFTogl_GL()
{
	return GetOpenGLEntryPoints( NULL );
}
#define gGL SFTogl_GL()

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// Scaleform shares togl's GL context. togl leaves state bound between batches
// that Scaleform's GL HAL does not reset: a pixel unpack buffer (Scaleform's
// texture uploads then read from togl's PBO -> garbage textures), unpack row
// length, enabled vertex attrib arrays, scissor/depth/stencil tests. Clear it
// before Scaleform renders and put back what togl's caches expect afterwards;
// togl's RestoreGLState (ForceFlushStates) re-flushes the rest.
static GLint s_nSavedUnpackBuffer = 0;
static GLint s_nSavedUnpackAlignment = 4;
static GLint s_nSavedUnpackRowLength = 0;

void SFTogl_SaveGLState( IDirect3DDevice9 *pDevice )
{
	if ( !pDevice )
		return;

	pDevice->SaveGLState();

	gGL->glGetIntegerv( GL_PIXEL_UNPACK_BUFFER_BINDING, &s_nSavedUnpackBuffer );
	gGL->glGetIntegerv( GL_UNPACK_ALIGNMENT, &s_nSavedUnpackAlignment );
	gGL->glGetIntegerv( GL_UNPACK_ROW_LENGTH, &s_nSavedUnpackRowLength );

	gGL->glBindBuffer( GL_PIXEL_UNPACK_BUFFER, 0 );
	gGL->glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
	gGL->glPixelStorei( GL_UNPACK_ALIGNMENT, 4 );

	for ( int i = 0; i < 16; i++ )
		gGL->glDisableVertexAttribArray( i );

	gGL->glDisable( GL_SCISSOR_TEST );
	gGL->glDisable( GL_DEPTH_TEST );
	gGL->glDisable( GL_STENCIL_TEST );
	gGL->glDisable( GL_CULL_FACE );
	gGL->glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
}

void SFTogl_RestoreGLState( IDirect3DDevice9 *pDevice )
{
	if ( !pDevice )
		return;

	gGL->glBindBuffer( GL_PIXEL_UNPACK_BUFFER, s_nSavedUnpackBuffer );
	gGL->glPixelStorei( GL_UNPACK_ALIGNMENT, s_nSavedUnpackAlignment );
	gGL->glPixelStorei( GL_UNPACK_ROW_LENGTH, s_nSavedUnpackRowLength );

	pDevice->RestoreGLState();
}

void SFTogl_GetViewportSize( IDirect3DDevice9 *pDevice, int *pWidth, int *pHeight )
{
	D3DVIEWPORT9 viewport = {};
	if ( pDevice )
		pDevice->GetViewport( &viewport );
	*pWidth = viewport.Width;
	*pHeight = viewport.Height;
}

ILauncherMgr *SFTogl_GetLauncherMgr( CreateInterfaceFn factory )
{
	return (ILauncherMgr *)factory( SDLMGR_INTERFACE_VERSION, NULL );
}

void SFTogl_SetMouseVisible( ILauncherMgr *pLauncherMgr, bool bVisible )
{
	if ( pLauncherMgr )
		pLauncherMgr->SetMouseVisible( bVisible );
}
