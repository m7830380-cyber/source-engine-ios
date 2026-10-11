#include "cbase.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

#include "UIAvatarImage.h"
#include "VAchievements.h"
#include "VAddons.h"
#include "VAddonAssociation.h"

#ifndef _X360

// CGameUiAvatarImage is built from P2ASW's uiavatarimage.cpp

#endif // !_X360

using namespace BaseModUI;

Achievements::Achievements( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

Achievements::~Achievements()
{
}

void Achievements::Activate()
{
}

void Achievements::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}

void Achievements::OnKeyCodePressed( vgui::KeyCode code )
{
	BaseClass::OnKeyCodePressed( code );
}

void Achievements::PaintBackground( void )
{
}

void Achievements::ToggleDisplayType( bool bDisplayType )
{
	(void)bDisplayType;
}

void Achievements::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}

Addons::Addons( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

Addons::~Addons()
{
}

void Addons::Activate()
{
}

void Addons::PaintBackground( void )
{
}

void Addons::OnCommand( const char *command )
{
	BaseClass::OnCommand( command );
}

void Addons::OnMessage( const KeyValues *params, vgui::VPANEL ifromPanel )
{
	BaseClass::OnMessage( params, ifromPanel );
}

void Addons::OnThink()
{
	BaseClass::OnThink();
}

void Addons::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
}

// AddonAssociation is built from P2ASW's vaddonassociation.cpp

#endif
