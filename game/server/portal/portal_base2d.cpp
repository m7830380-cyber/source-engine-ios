#include "cbase.h"
#include "portal_base2d.h"

void AddPortalVisibilityToPVS( CPortal_Base2D *pPortal, int pvssize, unsigned char *pvs )
{
	(void)pPortal;
	(void)pvssize;
	(void)pvs;
}

bool IsPlayerNearTargetPortal( CPortal_Base2D *pPortal )
{
	(void)pPortal;
	return true;
}
