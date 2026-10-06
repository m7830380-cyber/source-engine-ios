//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal 2 on the CS:GO engine: player decals.
//
//			c_te_playerdecal.cpp is CS:GO's graffiti (sprays from the item
//			economy); Portal 2 has neither, so the Portal 2 client leaves it
//			and server/te_playerdecal.cpp out and keeps their entry points.
//
//=============================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void TE_PlayerDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, const Vector* right, int player, int entity, int hitbox, int nAdditionalDecalFlags )
{
}

void OnPlayerDecalsLevelShutdown()
{
}

void OnPlayerDecalsUpdate()
{
}
