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

void COptionsSubMultiplayer::OnTextChanged( vgui::Panel *panel )
{
	(void)panel;
}

void COptionsSubMultiplayer::OnSliderMoved( KeyValues *data )
{
	(void)data;
}

void COptionsSubMultiplayer::OnApplyButtonEnable()
{
}

void COptionsSubMultiplayer::OnFileSelected( const char *fullpath )
{
	(void)fullpath;
}

#endif
