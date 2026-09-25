//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Touch-friendly buy menu (VGUI). CS:GO's Scaleform buy menu is not
//          part of this source, so PANEL_BUY had nothing behind it. The item
//          list comes from the weapon database, like Kisak-Strike's buy menu:
//          what the player's team/map/mode can buy, with prices, greyed out
//          when it can't be bought right now.
//
//===========================================================================//

#include "cbase.h"
#include "ios_buymenu.h"

#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>

#include "game/client/iviewport.h"
#include "viewport_panel_names.h"
#include "c_cs_player.h"
#include "cs_gamerules.h"
#include "cs_weapon_parse.h"
#include "cs_shareddefs.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar ios_buymenu_font( "ios_buymenu_font", "DefaultLarge", FCVAR_ARCHIVE, "Scheme font used by the touch buy menu" );

enum BuyCategory_t
{
	BUYCAT_PISTOLS,
	BUYCAT_SMGS,
	BUYCAT_RIFLES,
	BUYCAT_HEAVY,
	BUYCAT_GEAR,
	BUYCAT_GRENADES,
	BUYCAT_COUNT
};

static const char *s_pszCategoryNames[BUYCAT_COUNT] =
{
	"Pistols", "SMGs", "Rifles", "Heavy", "Gear", "Grenades"
};

static int CategoryForWeaponType( int nType )
{
	switch ( nType )
	{
	case WEAPONTYPE_PISTOL:			return BUYCAT_PISTOLS;
	case WEAPONTYPE_SUBMACHINEGUN:	return BUYCAT_SMGS;
	case WEAPONTYPE_RIFLE:
	case WEAPONTYPE_SNIPER_RIFLE:	return BUYCAT_RIFLES;
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_MACHINEGUN:		return BUYCAT_HEAVY;
	case WEAPONTYPE_EQUIPMENT:		return BUYCAT_GEAR;
	case WEAPONTYPE_GRENADE:		return BUYCAT_GRENADES;
	default:						return -1;
	}
}

// the team/map/mode can never buy it: don't list it at all
static bool IsHiddenResult( AcquireResult::Type result )
{
	switch ( result )
	{
	case AcquireResult::InvalidItem:
	case AcquireResult::NotAllowedByTeam:
	case AcquireResult::NotAllowedByMap:
	case AcquireResult::NotAllowedByMode:
	case AcquireResult::NotAllowedForPurchase:
	case AcquireResult::NotAllowedByProhibition:
		return true;
	default:
		return false;
	}
}

class CIOSBuyMenu : public EditablePanel, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE( CIOSBuyMenu, EditablePanel );

public:
	enum { MAX_ITEMS = 24 };

	CIOSBuyMenu( IViewPort *pViewPort );

	// IViewPortPanel
	virtual const char *GetName( void ) { return PANEL_BUY; }
	virtual void SetData( KeyValues *data ) {}
	virtual void Reset( void ) {}
	virtual void Update( void ) { RebuildItems(); }
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return true; }
	virtual void ShowPanel( bool bShow );
	virtual vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( vgui::VPANEL parent ) { BaseClass::SetParent( parent ); }
	virtual bool WantsBackgroundBlurred( void ) { return false; }

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void PerformLayout();
	virtual void PaintBackground();
	virtual void OnCommand( const char *command );
	virtual void OnThink();

private:
	void RebuildItems();
	void StyleButton( Button *pButton, bool bSelected );
	void Close();

	IViewPort	*m_pViewPort;
	Label		*m_pTitle;
	Label		*m_pMoney;
	Button		*m_pClose;
	Button		*m_pCategories[BUYCAT_COUNT];
	Button		*m_pItems[MAX_ITEMS];
	int			m_nItemCount;
	int			m_nCategory;
	int			m_nLastAccount;
	float		m_flNextRefresh;
	HFont		m_hFont;
};

CIOSBuyMenu::CIOSBuyMenu( IViewPort *pViewPort ) : BaseClass( NULL, PANEL_BUY )
{
	m_pViewPort = pViewPort;
	m_nItemCount = 0;
	m_nCategory = BUYCAT_RIFLES;
	m_nLastAccount = -1;
	m_flNextRefresh = 0.0f;
	m_hFont = INVALID_FONT;

	SetScheme( "ClientScheme" );
	SetProportional( false );
	SetMouseInputEnabled( true );
	SetKeyBoardInputEnabled( false );
	SetVisible( false );

	m_pTitle = new Label( this, "Title", "Buy Menu" );
	m_pMoney = new Label( this, "Money", "" );
	m_pClose = new Button( this, "Close", "Close", this, "close" );

	for ( int i = 0; i < BUYCAT_COUNT; i++ )
	{
		char szCommand[32];
		Q_snprintf( szCommand, sizeof( szCommand ), "category %d", i );
		m_pCategories[i] = new Button( this, "Category", s_pszCategoryNames[i], this, szCommand );
	}

	for ( int i = 0; i < MAX_ITEMS; i++ )
	{
		m_pItems[i] = new Button( this, "Item", "", this, "" );
		m_pItems[i]->SetVisible( false );
	}
}

void CIOSBuyMenu::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hFont = pScheme->GetFont( ios_buymenu_font.GetString(), false );
	if ( m_hFont == INVALID_FONT )
		m_hFont = pScheme->GetFont( "Default", false );

	m_pTitle->SetFont( m_hFont );
	m_pMoney->SetFont( m_hFont );
	m_pTitle->SetFgColor( Color( 255, 255, 255, 255 ) );
	m_pMoney->SetFgColor( Color( 140, 220, 120, 255 ) );
	StyleButton( m_pClose, false );
	for ( int i = 0; i < BUYCAT_COUNT; i++ )
		StyleButton( m_pCategories[i], i == m_nCategory );
	for ( int i = 0; i < MAX_ITEMS; i++ )
		StyleButton( m_pItems[i], false );
}

void CIOSBuyMenu::StyleButton( Button *pButton, bool bSelected )
{
	if ( m_hFont != INVALID_FONT )
		pButton->SetFont( m_hFont );
	pButton->SetContentAlignment( Label::a_center );
	pButton->SetPaintBorderEnabled( false );
	pButton->SetDefaultColor( Color( 255, 255, 255, 255 ), bSelected ? Color( 60, 110, 170, 230 ) : Color( 40, 44, 52, 220 ) );
	pButton->SetArmedColor( Color( 255, 255, 255, 255 ), Color( 70, 130, 200, 240 ) );
	pButton->SetDepressedColor( Color( 255, 255, 255, 255 ), Color( 90, 150, 220, 255 ) );
	pButton->SetDisabledFgColor1( Color( 110, 110, 110, 255 ) );
	pButton->SetDisabledFgColor2( Color( 0, 0, 0, 0 ) );
}

void CIOSBuyMenu::PaintBackground()
{
	surface()->DrawSetColor( Color( 0, 0, 0, 190 ) );
	surface()->DrawFilledRect( 0, 0, GetWide(), GetTall() );
}

void CIOSBuyMenu::PerformLayout()
{
	BaseClass::PerformLayout();

	int sw, sh;
	surface()->GetScreenSize( sw, sh );
	SetBounds( 0, 0, sw, sh );

	// everything in fractions of the screen so the buttons stay finger sized
	const int margin = sh / 40;
	const int headerH = sh / 9;
	const int catW = sw / 5;
	const int catH = ( sh - headerH - margin * ( BUYCAT_COUNT + 1 ) ) / BUYCAT_COUNT;

	m_pTitle->SetBounds( margin, margin, sw / 3, headerH - margin );
	m_pMoney->SetBounds( sw / 3, margin, sw / 3, headerH - margin );
	m_pClose->SetBounds( sw - margin - sw / 7, margin, sw / 7, headerH - margin );

	for ( int i = 0; i < BUYCAT_COUNT; i++ )
		m_pCategories[i]->SetBounds( margin, headerH + margin + i * ( catH + margin ), catW, catH );

	const int columns = 3;
	const int gridX = margin * 2 + catW;
	const int gridW = sw - gridX - margin;
	const int tileW = ( gridW - margin * ( columns - 1 ) ) / columns;
	const int rows = MAX( ( m_nItemCount + columns - 1 ) / columns, 4 );
	const int tileH = MIN( catH, ( sh - headerH - margin * ( rows + 1 ) ) / rows );
	for ( int i = 0; i < MAX_ITEMS; i++ )
	{
		int col = i % columns, row = i / columns;
		m_pItems[i]->SetBounds( gridX + col * ( tileW + margin ), headerH + margin + row * ( tileH + margin ), tileW, tileH );
	}
}

void CIOSBuyMenu::RebuildItems()
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	m_nItemCount = 0;
	if ( pPlayer )
	{
		const int nAccount = pPlayer->GetAccount();
		m_nLastAccount = nAccount;

		wchar_t wszMoney[64];
		V_snwprintf( wszMoney, ARRAYSIZE( wszMoney ), L"$%d", nAccount );
		m_pMoney->SetText( wszMoney );

		for ( int i = WEAPON_FIRST; i < WEAPON_MAX && m_nItemCount < MAX_ITEMS; i++ )
		{
			CSWeaponID id = (CSWeaponID)i;
			const CCSWeaponInfo *pInfo = GetWeaponInfo( id );
			const char *pszAlias = WeaponIDToAlias( id );
			if ( !pInfo || !pszAlias )
				continue;
			if ( CategoryForWeaponType( pInfo->GetWeaponType() ) != m_nCategory )
				continue;

			AcquireResult::Type result = pPlayer->CanAcquire( id, AcquireMethod::Buy );
			if ( IsHiddenResult( result ) )
				continue;

			const int nPrice = pInfo->GetWeaponPrice();

			wchar_t wszName[64];
			const wchar_t *pwszLocalized = pInfo->szPrintName[0] ? g_pVGuiLocalize->Find( pInfo->szPrintName ) : NULL;
			if ( pwszLocalized )
				V_wcsncpy( wszName, pwszLocalized, sizeof( wszName ) );
			else
				g_pVGuiLocalize->ConvertANSIToUnicode( pszAlias, wszName, sizeof( wszName ) );

			wchar_t wszText[128];
			V_snwprintf( wszText, ARRAYSIZE( wszText ), L"%ls   $%d", wszName, nPrice );

			char szCommand[64];
			Q_snprintf( szCommand, sizeof( szCommand ), "buy %s", pszAlias );

			Button *pButton = m_pItems[m_nItemCount++];
			pButton->SetText( wszText );
			pButton->SetCommand( szCommand );
			pButton->SetEnabled( result == AcquireResult::Allowed && nPrice <= nAccount );
			pButton->SetVisible( true );
		}
	}

	for ( int i = m_nItemCount; i < MAX_ITEMS; i++ )
		m_pItems[i]->SetVisible( false );
	for ( int i = 0; i < BUYCAT_COUNT; i++ )
		StyleButton( m_pCategories[i], i == m_nCategory );

	InvalidateLayout();
}

void CIOSBuyMenu::ShowPanel( bool bShow )
{
	if ( BaseClass::IsVisible() == bShow )
		return;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( bShow )
	{
		InvalidateLayout( true, true );
		RebuildItems();
		SetVisible( true );
		SetMouseInputEnabled( true );
		MoveToFront();
		m_flNextRefresh = gpGlobals->curtime + 0.25f;
	}
	else
	{
		SetVisible( false );
		SetMouseInputEnabled( false );
	}

	if ( pPlayer )
		pPlayer->SetBuyMenuOpen( bShow );

	printf( "[buymenu] %s\n", bShow ? "open" : "closed" );
	fflush( stdout );
}

void CIOSBuyMenu::Close()
{
	if ( m_pViewPort )
		m_pViewPort->ShowPanel( this, false );
	else
		ShowPanel( false );
}

void CIOSBuyMenu::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "close" ) )
	{
		Close();
		return;
	}
	if ( !Q_strnicmp( command, "category ", 9 ) )
	{
		m_nCategory = clamp( atoi( command + 9 ), 0, BUYCAT_COUNT - 1 );
		RebuildItems();
		return;
	}
	if ( !Q_strnicmp( command, "buy ", 4 ) )
	{
		printf( "[buymenu] %s\n", command );
		fflush( stdout );
		engine->ClientCmd_Unrestricted( command );
		// money and owned items come back from the server; refresh shortly
		m_flNextRefresh = gpGlobals->curtime + 0.15f;
		return;
	}
	BaseClass::OnCommand( command );
}

void CIOSBuyMenu::OnThink()
{
	BaseClass::OnThink();

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer || !pPlayer->IsAlive() || !pPlayer->IsInBuyPeriod() || !pPlayer->IsInBuyZone() )
	{
		Close();
		return;
	}

	if ( gpGlobals->curtime >= m_flNextRefresh || pPlayer->GetAccount() != m_nLastAccount )
	{
		RebuildItems();
		m_flNextRefresh = gpGlobals->curtime + 0.5f;
	}
}

IViewPortPanel *IOS_CreateBuyMenu( IViewPort *pViewPort )
{
	return new CIOSBuyMenu( pViewPort );
}

bool IOS_IsBuyMenuVisible()
{
	IViewPort *pViewPort = GetViewPortInterface();
	IViewPortPanel *pPanel = pViewPort ? pViewPort->FindPanelByName( PANEL_BUY ) : NULL;
	return pPanel && pPanel->IsVisible();
}
