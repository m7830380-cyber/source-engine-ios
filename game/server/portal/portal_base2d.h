#ifndef PORTAL_BASE2D_H
#define PORTAL_BASE2D_H
#ifdef _WIN32
#pragma once
#endif

#include "prop_portal.h"

#if defined( PORTAL2 )
#define CPortal_Base2D CProp_Portal
#else
typedef CProp_Portal CPortal_Base2D;
#endif

void AddPortalVisibilityToPVS( CPortal_Base2D *pPortal, int pvssize, unsigned char *pvs );
bool IsPlayerNearTargetPortal( CPortal_Base2D *pPortal );

#endif
