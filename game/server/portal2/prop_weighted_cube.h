#ifndef PROP_WEIGHTED_CUBE_H
#define PROP_WEIGHTED_CUBE_H
#ifdef _WIN32
#pragma once
#endif

class CBaseEntity;

void PropWeightedCube_SetActivated( CBaseEntity *pEnt, bool bState );
bool PropWeightedCube_WasTouchedByPlayer( CBaseEntity *pEnt );

#endif
