#include "cbase.h"
#include "radialmenu.h"
#include "viewport_panel_names.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

CRadialMenuPanel::CRadialMenuPanel( IViewPort *pViewPort )
	: BaseClass( NULL, PANEL_RADIAL_MENU )
{
	m_pViewPort = pViewPort;
	SetScheme( "ClientScheme" );
	SetMoveable( false );
	SetSizeable( false );
	SetCursor( NULL );
	SetPaintBackgroundEnabled( false );
}

void CRadialMenuPanel::ShowPanel( bool bShow )
{
	SetVisible( bShow );
}

#endif
