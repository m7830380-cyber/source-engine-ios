#include "cbase.h"

#if defined( CLIENT_DLL ) && defined( PORTAL2 ) && defined( IOS )

// RubberWar server VPC omits some Portal 2 entity sources; client systems still
// look up these convars during init.
ConVar sv_portal2_pickup_hint_range( "sv_portal2_pickup_hint_range", "350.0", FCVAR_CLIENTDLL );
ConVar sv_portal2_button_hint_range( "sv_portal2_button_hint_range", "350.0", FCVAR_CLIENTDLL );

#endif
