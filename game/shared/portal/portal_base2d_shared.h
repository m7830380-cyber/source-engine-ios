#ifndef PORTAL_BASE2D_SHARED_H
#define PORTAL_BASE2D_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "prop_portal_shared.h"

typedef CProp_Portal CPortal_Base2D;
#define CPortal_Base2D_Shared CProp_Portal_Shared

#ifdef CLIENT_DLL
void ProcessPortalTeleportations( void );
#endif

#endif
