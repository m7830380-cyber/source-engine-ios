//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS:GO Scaleform buy menu (buy-menu.swf). The original was not part
//          of the leaked source; this is the reconstruction from
//          github.com/Rileyboy1223/cstrike15-restoration, adapted for this port
//          (see the iOS notes in the .cpp).
//
//===========================================================================//
#ifndef SFBUYMENU_H
#define SFBUYMENU_H
#pragma once

#include "scaleformui/scaleformui.h"
#include "../VGUI/counterstrikeviewport.h"
#include "GameEventListener.h"
#include "cs_weapon_parse.h"

const int cSFBuyMaxEquipment = 6;  // if this # changes, bump version # cl_titledataversionblock3 and loadoutData in TitleData1 block for title data storageCCSSteamStats::SyncCSLoadoutsToTitleData
const int cSFBuyMaxLoadouts = 4;	// if this # changes, bump version # cl_titledataversionblock3 and loadoutData in TitleData1 block  for title data storageCCSSteamStats::SyncCSLoadoutsToTitleData

class CounterStrikeViewport;

struct SFBuyEquipmentLoadout
{
	int m_EquipmentID;
	int m_EquipmentPos;
	int m_Quantity;

	SFBuyEquipmentLoadout()
	{
		m_EquipmentID = 0;
		m_EquipmentPos = 0;
		m_Quantity = 0;
	}

	bool operator==(const SFBuyEquipmentLoadout &in_rhs) const
	{
		return m_EquipmentID == in_rhs.m_EquipmentID && m_EquipmentPos == in_rhs.m_EquipmentPos && m_Quantity == in_rhs.m_Quantity;
	}
};

struct SFBuyLoadout
{
	enum
	{
		HAS_TASER = 0x1,
		HAS_BOMB = 0x2,
		HAS_DEFUSE = 0x4,
	};

	int m_primaryWeaponID;
	int m_secondaryWeaponID;

	int m_primaryWeaponItemPos;
	int m_secondaryWeaponItemPos;

	// Data fields
	SFBuyEquipmentLoadout m_EquipmentArray[cSFBuyMaxEquipment];
	uint8 m_flags;

	SFBuyLoadout()
	{
		Q_memset(this, 0, sizeof(*this));
	}

	bool operator==(const SFBuyLoadout &in_rhs) const
	{
		return memcmp(this, &in_rhs, sizeof(SFBuyLoadout)) == 0;
	}

	bool operator!=(const SFBuyLoadout &in_rhs) const
	{
		return !(*this == in_rhs);
	}
};

struct BuyMenuLoadoutData
{
	int m_iWeaponID;
	int m_iCount;
};

struct SFBuyMenuLoadout
{
	SFBuyMenuLoadout()
	{
		ClearLoadout();
	}

	void ClearLoadout( void )
	{
		for ( int i = 0; i < ARRAYSIZE( m_WeaponID ); i++ )
		{
			m_WeaponID[ i ] = 0;
		}
	}

	SFBuyMenuLoadout& operator=( const SFBuyMenuLoadout& in_rhs )
	{
		for ( int i = 0; i < ARRAYSIZE( m_WeaponID ); i++ )
		{
			m_WeaponID[ i ] = in_rhs.m_WeaponID[ i ];
		}

		return *this;
	}

	bool operator==( const SFBuyMenuLoadout& in_rhs ) const
	{
		for ( int i = 0; i < ARRAYSIZE( m_WeaponID ); i++ )
		{
			if ( m_WeaponID[ i ] != in_rhs.m_WeaponID[ i ] )
			{
				return false;
			}
		}

		return true;
	}

	inline bool operator!=( const SFBuyMenuLoadout& in_rhs ) const
	{
		return !( *this == in_rhs );
	}

	// Data fields
	int m_iWeaponID;
	int m_iCount;
	int m_iPrice;

	bool m_bRestricted;

	BuyMenuLoadoutData m_LoadoutItems[ cSFBuyMaxLoadouts ];
	itemid_t          m_WeaponID[ LOADOUT_POSITION_COUNT ];
};

struct BuyMenuLoadoutEntry
{
	int weapon;
	int count;
};

extern SFBuyMenuLoadout s_LoadoutArray[2][4];

struct InputAnalogActionData_t
{
	bool bActive;

	int x;
	int y;
};

struct WeaponStatRange
{
	float m_flFirepowerMax;
	float m_flFirepowerMin;

	float m_flFireRateMax;
	float m_flFireRateMin;

	float m_flAccuracyMax;
	float m_flAccuracyMin;

	float m_flMoveRateMax;
	float m_flMoveRateMin;
};

class CCSBuyMenuScaleform : public ScaleformFlashInterface, public IViewPortPanel, public CGameEventListener
{
public:
	explicit CCSBuyMenuScaleform( CounterStrikeViewport *pViewport );
	virtual ~CCSBuyMenuScaleform();

	/************************************
	* callbacks from scaleform
	*/

	void OnCancel( SCALEFORM_CALLBACK_ARGS_DECL );
	void InitWeapon( SCALEFORM_CALLBACK_ARGS_DECL );

	void GetWeaponPriceScript( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponPriceFromIDScript( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponShortNameFromPosition( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponFirepower( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponFireRate( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponMoveRate( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponHandling( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponArmorPen( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponClipSize( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponMaxCarry( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponPenetration( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponDMPoints( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponKillAward( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponItemID( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponFirepowerRaw( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponArmorPenRaw( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponRawMoveRatio( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetEffectiveRange( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetEffectiveRangeRaw( SCALEFORM_CALLBACK_ARGS_DECL );

	void GetPlayerLoadout( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetPlayerBuyMenuLoadout( SCALEFORM_CALLBACK_ARGS_DECL );

	void AreWeaponsFree( SCALEFORM_CALLBACK_ARGS_DECL );
	void Autobuy( SCALEFORM_CALLBACK_ARGS_DECL );
	void BuyWeapon( SCALEFORM_CALLBACK_ARGS_DECL );
	void BuyPrevious( SCALEFORM_CALLBACK_ARGS_DECL );
	void CanAcquire( SCALEFORM_CALLBACK_ARGS_DECL );
	void WeaponIsValid( SCALEFORM_CALLBACK_ARGS_DECL );

	// called by buy-menu.swf, missing from the restoration (iOS port)
	void SetShowWeaponModel( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponShortNameFromID( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWeaponName( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetCarryLimit( SCALEFORM_CALLBACK_ARGS_DECL );
	void ShouldCloseOnBuy( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetPredefinedLoadout( SCALEFORM_CALLBACK_ARGS_DECL );
	void BuyLoadout( SCALEFORM_CALLBACK_ARGS_DECL );
	void ReportLoadouts( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetLocalPlayerXuid( SCALEFORM_CALLBACK_ARGS_DECL );

	/****************************************
	* functionality
	*/

	bool FillInPlayerLoadout( SFBuyLoadout &loadout );
	bool FillInPlayerBuyMenuLoadout( SFBuyMenuLoadout *pLoadout );
	bool UpdateTimeLeft( bool bForce );
	bool SetBuyMenuWeaponSliceEntry( const SFBuyMenuLoadout &loadout, int weaponPosition, SFVALUE flashArray, uint32 arrayIndex );

	void SetPlayerIsCT( const bool isCT );
	void CalculateBestStats();
	void UpdatePlayerLoadout( bool bForceUpdate, bool bUpdateFlash );
	void UpdateRadialSelection();
	void SetRadialSelection( int radialSlot, int categorySlot );
	void InvalidateWeapon( int weaponID );
	void UpdatePlayerCash( bool bForce );
	void SetHostageMatch( bool bHostageMatch );

	uint32 GetWeaponFirepowerRawValue( uint32 position );

	const CCSWeaponInfo *GetWeaponInfoForPosition( int position );

	SFVALUE CreateFlashLoadout( const SFBuyLoadout &loadout );
	SFVALUE CreateFlashBuyMenuLoadout( const SFBuyMenuLoadout &loadout );

	/************************************************************
	*  Flash Interface methods
	*/

	virtual void FlashReady( void );
	virtual void FlashLoaded( void );

	void Show( void );

	// if bRemove, then remove all elements after hide animation completes
	void Hide( void );

	bool PreUnloadFlash( void );

	/*************************************************************
	* IViewPortPanel interface
	*/

	virtual const char *GetName( void ) { return PANEL_BUY; }
	virtual void SetData( KeyValues *data ) {};

	virtual void Reset( void ) {}  // hibernate
	virtual void Update( void ) {}	// updates all ( size, position, content, etc )
	virtual bool NeedsUpdate( void ) { return false; } // query panel if content needs to be updated
	virtual bool HasInputElements( void ) { return true; }
	virtual void ReloadScheme( void ) {}
	virtual bool CanReplace( const char *panelName ) const { return true; } // returns true if this panel can appear on top of the given panel
	virtual bool CanBeReopened( void ) const { return true; } // returns true if this panel can be re-opened after being hidden by another panel
	virtual void ViewportThink( void );

	void ShowPanel( bool state );

	// VGUI functions:
	virtual vgui::VPANEL GetVPanel( void ) { return 0; } // returns VGUI panel handle
	virtual bool IsVisible( void ) { return m_bVisible; }  // true if panel is visible
	virtual void SetParent( vgui::VPANEL parent ) {}

	virtual bool WantsBackgroundBlurred( void ) { return false; }

	/********************************************
	* CGameEventListener methods
	*/

	virtual void FireGameEvent( IGameEvent *event );

protected:
	CounterStrikeViewport *m_pViewport;

	SFBuyLoadout m_PlayerLoadout;
	SFBuyMenuLoadout m_PlayerBuyMenuLoadout;
	WeaponStatRange m_WeaponStatRange;

private:
	int   m_iSelectedWeapon;
	int   m_iSplitScreenSlot;
	int   m_iCachedArmorState;
	int   m_iCurrentRadialSelection;
	int   m_iSelectedCategory;
	int   m_iWheelSelection;
	int	  m_nLastTimeLeft;
	int   m_fGuardianBuyUntilTime;
	int   m_nLastAccount;

	float m_flNextLoadoutUpdate;

	bool  m_bVisible;
	bool  m_bNeedUpdate;
	bool  m_bLoading;
	bool  m_bRegisteredEvents;
	bool  m_bShowOnReady;
	bool  m_bIsLoaded;
	bool  m_bInitialized;


	vgui::EditablePanel *m_pItemPanelParent;
};

#endif // SFBUYMENU_H