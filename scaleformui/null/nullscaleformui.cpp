//===========================================================================//
//
// Purpose: IScaleformUI that does nothing, for platforms without Scaleform.
//
// CS:GO's menus and HUD are Scaleform GFx movies. GFx is a prebuilt,
// proprietary library with no iOS build, but the game calls IScaleformUI in
// many places. This module is loaded in place of scaleformui: movies never
// load, so Scaleform panels never become ready and stay inert, and the game
// runs without them. The method bodies are generated from the interface by
// scripts/gen_null_scaleform.py.
//
//===========================================================================//

#include "tier1/interface.h"
#include "tier1/convar.h"
#include "appframework/iappsystem.h"
#include "scaleformui/scaleformui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNullScaleformUI : public CBaseAppSystem< IScaleformUI >
{
public:
#include "nullscaleformui.inl"
};

static CNullScaleformUI g_NullScaleformUI;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CNullScaleformUI, IScaleformUI, SCALEFORMUI_INTERFACE_VERSION, g_NullScaleformUI );
