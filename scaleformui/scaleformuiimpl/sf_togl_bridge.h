//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: iOS (ANGLE) build of scaleformui: togl and the launcher manager
//          pull in the desktop GL headers, which clash with the GLES2
//          headers Scaleform's GL renderer is built against. The few togl /
//          launcher calls the integration makes go through this bridge,
//          implemented in sf_togl_bridge.cpp without any Scaleform headers.
//
//===========================================================================//

#ifndef SF_TOGL_BRIDGE_H
#define SF_TOGL_BRIDGE_H

#include "tier1/interface.h"

struct IDirect3DDevice9;
class ILauncherMgr;

void SFTogl_SaveGLState( IDirect3DDevice9 *pDevice );
void SFTogl_RestoreGLState( IDirect3DDevice9 *pDevice );
void SFTogl_GetViewportSize( IDirect3DDevice9 *pDevice, int *pWidth, int *pHeight );
// diagnostics: print the bound framebuffer, viewport and pending GL error
void SFTogl_LogGLState( const char *pszWhere, int nSlot );

ILauncherMgr *SFTogl_GetLauncherMgr( CreateInterfaceFn factory );
void SFTogl_SetMouseVisible( ILauncherMgr *pLauncherMgr, bool bVisible );

#endif // SF_TOGL_BRIDGE_H
