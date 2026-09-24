//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: togl / launcher manager calls for the iOS build of scaleformui.
//          Deliberately includes no Scaleform headers (see sf_togl_bridge.h).
//
//===========================================================================//

#include "togl/rendermechanism.h"
#include "appframework/ilaunchermgr.h"

#include "sf_togl_bridge.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

void SFTogl_SaveGLState( IDirect3DDevice9 *pDevice )
{
	if ( pDevice )
		pDevice->SaveGLState();
}

void SFTogl_RestoreGLState( IDirect3DDevice9 *pDevice )
{
	if ( pDevice )
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
