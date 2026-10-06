#ifndef PORTAL_BASE2D_H
#define PORTAL_BASE2D_H
#ifdef _WIN32
#pragma once
#endif

#include "prop_portal.h"

typedef CProp_Portal CPortal_Base2D;

void AddPortalVisibilityToPVS( CPortal_Base2D *pPortal, int pvssize, unsigned char *pvs );
bool IsPlayerNearTargetPortal( CPortal_Base2D *pPortal );

#endif
