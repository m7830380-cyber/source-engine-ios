//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Engine/client bridge: portal stencil views skip displacement draws on iOS.
//
//=============================================================================//

#ifndef IOS_PORTAL_DISP_H
#define IOS_PORTAL_DISP_H

#if defined( IOS )

// Set from client when CPortalRender view recursion changes (portal stencil path).
extern int g_iPortalStencilViewRecursionLevel;

inline void IOS_SetPortalStencilViewRecursionLevel( int nLevel )
{
	g_iPortalStencilViewRecursionLevel = nLevel;
}

#else

inline void IOS_SetPortalStencilViewRecursionLevel( int nLevel )
{
	(void)nLevel;
}

#endif

#endif // IOS_PORTAL_DISP_H
