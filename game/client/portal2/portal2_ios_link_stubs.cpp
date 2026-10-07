#include "cbase.h"
#include "tier1/strtools.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

#include "vgui/IPanel.h"

bool IsRadialMenuOpen( void )
{
	return false;
}

void IOS_CloseChat() {}
bool IOS_IsChatOpen() { return false; }
bool IOS_IsBuyMenuVisible() { return false; }

void OpenGammaDialog( vgui::VPANEL parent )
{
	(void)parent;
}

void LoadEquipmentData() {}

bool GameModeHasDifficulty( const char *pszMode )
{
	(void)pszMode;
	return false;
}

const char *GameModeGetDefaultDifficulty( const char *pszMode )
{
	(void)pszMode;
	return "normal";
}

void PrecacheLoadingTipIcons() {}
DWORD InitHudAllowTextChatFlag( void ) { return 0; }
DWORD InitUiAllowProperTintFlag( void ) { return 0; }

class C_BaseEntity;
C_BaseEntity *GetPlayerHoldingEntity( const C_BaseEntity *pHeld )
{
	(void)pHeld;
	return NULL;
}

Vector Pickup_DefaultPhysGunLaunchVelocity( const Vector &vecForward, float flMass )
{
	(void)flMass;
	return vecForward * 200.0f;
}

void MoveUnpredictedPhysicsNearPlayerToNetworkedPosition( C_BasePlayer *pPlayer )
{
	(void)pPlayer;
}

void IOS_HapticViewModelSound( C_BasePlayer *pPlayer, const char *pszSound )
{
	(void)pPlayer;
	(void)pszSound;
}

void CS_FreezePanel_OnHltvReplayButtonStateChanged() {}
void CS_FreezePanel_ResetDamageText( int iPlayerIndexKiller, int iPlayerIndexVictim )
{
	(void)iPlayerIndexKiller;
	(void)iPlayerIndexVictim;
}

#endif
