//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal 2's streamlined level transitions.
//
// Maps end in an elevator whose script (transitions/sp_transition_list.nut)
// fires "ChangeLevel <next map>" at the map's point_changelevel. The departure
// elevator holds an info_landmark_exit and the arrival elevator of the next map
// an info_landmark_entry; CChangeLevel::BuildChangeLevelList (triggers.cpp)
// pairs them through the ChangeLevel_* names below, so the player and whatever
// rides along keep their place relative to the elevator across the load.
//
// The leaked tree lists this file in server_portal2.vpc but doesn't contain it.
//
//=============================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define POINT_CHANGELEVEL_LANDMARK_NAME	"__p2_transition_landmark"

ConVar sv_transition_landmark( "sv_transition_landmark", "1", FCVAR_NONE,
	"Carry the player across Portal 2 level transitions with save/restore (0: plain changelevel)" );

// Kept across the level change: the server DLL stays loaded, and the destination
// map's BuildChangeLevelList needs the same names the origin map used.
static char s_szOriginMapName[MAX_MAP_NAME];
static char s_szDestinationMapName[MAX_MAP_NAME];

const char *ChangeLevel_DestinationMapName( void )
{
	return s_szDestinationMapName;
}

const char *ChangeLevel_OriginMapName( void )
{
	return s_szOriginMapName;
}

const char *ChangeLevel_GetLandmarkName( void )
{
	return POINT_CHANGELEVEL_LANDMARK_NAME;
}

class CPointChangelevel : public CServerOnlyPointEntity
{
	DECLARE_CLASS( CPointChangelevel, CServerOnlyPointEntity );
	DECLARE_DATADESC();

public:
	void InputChangeLevel( inputdata_t &inputdata );

private:
	COutputEvent m_OnChangeLevel;
};

BEGIN_DATADESC( CPointChangelevel )
	DEFINE_INPUTFUNC( FIELD_STRING, "ChangeLevel", InputChangeLevel ),
	DEFINE_OUTPUT( m_OnChangeLevel, "OnChangeLevel" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( point_changelevel, CPointChangelevel );

// The landmarks only mark a position; the transition code finds them by classname and
// hands their edict to the engine, so like info_landmark (subs.cpp) they need one.
LINK_ENTITY_TO_CLASS( info_landmark_entry, CPointEntity );
LINK_ENTITY_TO_CLASS( info_landmark_exit, CPointEntity );

void CPointChangelevel::InputChangeLevel( inputdata_t &inputdata )
{
	const char *pszMapName = inputdata.value.String();
	if ( !pszMapName || !pszMapName[0] )
	{
		Warning( "%s: ChangeLevel without a map name\n", GetDebugName() );
		return;
	}

	if ( !engine->IsMapValid( pszMapName ) )
	{
		Warning( "%s: ChangeLevel to missing map '%s'\n", GetDebugName(), pszMapName );
		return;
	}

	m_OnChangeLevel.FireOutput( inputdata.pActivator, this );

	// Only ride along when this map really has the departure landmark to measure from.
	bool bLandmark = sv_transition_landmark.GetBool() &&
		gEntList.FindEntityByClassname( NULL, "info_landmark_exit" ) != NULL;

	if ( bLandmark )
	{
		V_strncpy( s_szOriginMapName, STRING( gpGlobals->mapname ), sizeof( s_szOriginMapName ) );
		V_strncpy( s_szDestinationMapName, pszMapName, sizeof( s_szDestinationMapName ) );
	}
	else
	{
		s_szOriginMapName[0] = '\0';
		s_szDestinationMapName[0] = '\0';
	}

	DevMsg( "%s: changelevel to %s%s\n", GetDebugName(), pszMapName, bLandmark ? " (landmark transition)" : "" );
	engine->ChangeLevel( pszMapName, bLandmark ? POINT_CHANGELEVEL_LANDMARK_NAME : NULL );
}
