#include "cbase.h"
#include "OptionsSubMultiplayer.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

COptionsSubMultiplayer::COptionsSubMultiplayer( vgui::Panel *parent )
	: BaseClass( parent, NULL )
{
}

COptionsSubMultiplayer::~COptionsSubMultiplayer()
{
}

vgui::Panel *COptionsSubMultiplayer::CreateControlByName( const char *controlName )
{
	(void)controlName;
	return NULL;
}

void COptionsSubMultiplayer::OnResetData()
{
}

void COptionsSubMultiplayer::OnApplyChanges()
{
}

void COptionsSubMultiplayer::OnCommand( const char *command )
{
	(void)command;
}

#endif
