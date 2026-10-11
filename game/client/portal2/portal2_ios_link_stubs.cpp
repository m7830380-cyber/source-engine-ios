#include "cbase.h"
#include "tier1/strtools.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

#include "vgui/IPanel.h"

void IOS_CloseChat() {}
bool IOS_IsChatOpen() { return false; }
bool IOS_IsBuyMenuVisible() { return false; }

void LoadEquipmentData() {}
DWORD InitHudAllowTextChatFlag( void ) { return 0; }
DWORD InitUiAllowProperTintFlag( void ) { return 0; }

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
