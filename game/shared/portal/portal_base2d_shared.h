#ifndef PORTAL_BASE2D_SHARED_H
#define PORTAL_BASE2D_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "prop_portal_shared.h"

typedef CProp_Portal CPortal_Base2D;
#define CPortal_Base2D_Shared CProp_Portal_Shared

inline float UTIL_Portal_DistanceThroughPortalSqr( const CPortal_Base2D *pPortal, const Vector &vPoint1, const Vector &vPoint2 );

#ifdef CLIENT_DLL
void ProcessPortalTeleportations( void );
#endif

inline float UTIL_Portal_DistanceThroughPortalSqr( const CPortal_Base2D *pPortal, const Vector &vPoint1, const Vector &vPoint2 )
{
	extern float UTIL_Portal_DistanceThroughPortalSqr( const CProp_Portal *pPortal, const Vector &vPoint1, const Vector &vPoint2 );
	return UTIL_Portal_DistanceThroughPortalSqr( static_cast<const CProp_Portal *>( pPortal ), vPoint1, vPoint2 );
}

#endif
