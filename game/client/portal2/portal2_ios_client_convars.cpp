#include "cbase.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

ConVar cl_countbones( "cl_countbones", "0", FCVAR_CHEAT );
ConVar cl_righthand( "cl_righthand", "1", FCVAR_ARCHIVE | FCVAR_SS );
ConVar fov_cs_debug( "fov_cs_debug", "0", FCVAR_REPLICATED | FCVAR_CHEAT );
ConVar sv_coaching_enabled( "sv_coaching_enabled", "0", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar sv_disable_motd( "sv_disable_motd", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY | FCVAR_CHEAT );
ConVar sv_max_allowed_net_graph( "sv_max_allowed_net_graph", "1", FCVAR_REPLICATED | FCVAR_RELEASE );

ConVar fps( "fps", "0", FCVAR_RELEASE );

#if defined( IOS )
// RubberWar server VPC omits some Portal 2 entity sources; client still looks these up.
ConVar sv_portal2_pickup_hint_range( "sv_portal2_pickup_hint_range", "350.0", FCVAR_CLIENTDLL );
ConVar sv_portal2_button_hint_range( "sv_portal2_button_hint_range", "350.0", FCVAR_CLIENTDLL );
#endif

bool g_bShowGhostedPortals = false;
bool g_bUpsideDown = false;
float g_fMaxViewModelLag = 0.0f;

#endif
