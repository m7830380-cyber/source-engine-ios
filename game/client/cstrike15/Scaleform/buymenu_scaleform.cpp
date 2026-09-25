//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: CS:GO Scaleform buy menu (buy-menu.swf). The original was not part
//          of the leaked source; this is the reconstruction from
//          github.com/Rileyboy1223/cstrike15-restoration, adapted for this port
//          (see the iOS notes in the .cpp).
//
//===========================================================================//
// RILEY WALLIS

#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "ienginevgui.h"
#include "buymenu_scaleform.h"
#include "gameui_util.h"
#include "inputsystem/iinputsystem.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"

#include <keyvalues.h>

#include <vgui_controls/EditablePanel.h>

#include "HUD/sfhudinfopanel.h"

#include "c_cs_player.h"
#include "cs_ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnCancel ),
	SFUI_DECL_METHOD( InitWeapon ),

	SFUI_DECL_METHOD( GetWeaponPriceScript ),
	SFUI_DECL_METHOD( GetWeaponPriceFromIDScript ),
	SFUI_DECL_METHOD( GetWeaponShortNameFromPosition ),
	SFUI_DECL_METHOD( GetWeaponFirepower ),
	SFUI_DECL_METHOD( GetWeaponFireRate ),
	SFUI_DECL_METHOD( GetWeaponMoveRate ),
	SFUI_DECL_METHOD( GetWeaponHandling ),
	SFUI_DECL_METHOD( GetWeaponArmorPen ),
	SFUI_DECL_METHOD( GetWeaponClipSize ),
	SFUI_DECL_METHOD( GetWeaponMaxCarry ),
	SFUI_DECL_METHOD( GetWeaponPenetration ),
	SFUI_DECL_METHOD( GetWeaponDMPoints ),
	SFUI_DECL_METHOD( GetWeaponKillAward ),
	SFUI_DECL_METHOD( GetWeaponItemID ),
	SFUI_DECL_METHOD( GetWeaponFirepowerRaw ),
	SFUI_DECL_METHOD( GetWeaponArmorPenRaw ),
	SFUI_DECL_METHOD( GetWeaponRawMoveRatio ),
	SFUI_DECL_METHOD( GetEffectiveRange ),
	SFUI_DECL_METHOD( GetEffectiveRangeRaw ),

	SFUI_DECL_METHOD( GetPlayerLoadout ),
	SFUI_DECL_METHOD( GetPlayerBuyMenuLoadout ),
	SFUI_DECL_METHOD( AreWeaponsFree ),
	SFUI_DECL_METHOD( Autobuy ),
	SFUI_DECL_METHOD( BuyWeapon ),
	SFUI_DECL_METHOD( BuyPrevious ),
	SFUI_DECL_METHOD( CanAcquire ),
	SFUI_DECL_METHOD( WeaponIsValid ),

	// called by buy-menu.swf, missing from the restoration (iOS port)
	SFUI_DECL_METHOD( SetShowWeaponModel ),
	SFUI_DECL_METHOD( GetWeaponShortNameFromID ),
	SFUI_DECL_METHOD( GetWeaponName ),
	SFUI_DECL_METHOD( GetCarryLimit ),
	SFUI_DECL_METHOD( ShouldCloseOnBuy ),
	SFUI_DECL_METHOD( GetPredefinedLoadout ),
	SFUI_DECL_METHOD( BuyLoadout ),
	SFUI_DECL_METHOD( ReportLoadouts ),
	SFUI_DECL_METHOD( GetLocalPlayerXuid ),
SFUI_END_GAME_API_DEF( CCSBuyMenuScaleform, BuyMenu );

struct BuyMenuLoadoutEntry s_defaultLoadoutWeapons[2][4] =
{
	{

	},

	{

	}
};

SFBuyMenuLoadout s_LoadoutArray[2][4];

bool m_bLoadoutInitialized = false;

void InitBuyMenuLoadoutData()
{
	Msg("Init BuyMenu Loadout Data");
	for ( int team = 0; team < 2; ++team )
	{
		for ( int slot = 0; slot < 4; ++slot )
		{
			SFBuyMenuLoadout &entry = s_LoadoutArray[team][slot];

			entry.m_iWeaponID = s_defaultLoadoutWeapons[team][slot].weapon;
			entry.m_iCount = s_defaultLoadoutWeapons[team][slot].count;

			entry.m_iPrice = 0;
			entry.m_bRestricted = false;

			entry.m_LoadoutItems[0].m_iWeaponID = WEAPON_HEGRENADE;
			entry.m_LoadoutItems[0].m_iCount = 1;

			entry.m_LoadoutItems[1].m_iWeaponID = WEAPON_FLASHBANG;
			entry.m_LoadoutItems[1].m_iCount = 1;

			entry.m_LoadoutItems[2].m_iWeaponID = WEAPON_SMOKEGRENADE;
			entry.m_LoadoutItems[2].m_iCount = 1;

			entry.m_LoadoutItems[3].m_iWeaponID = 0;
			entry.m_LoadoutItems[3].m_iCount = 1;
		}
	}

	m_bLoadoutInitialized = true;
}

CCSBuyMenuScaleform::CCSBuyMenuScaleform( CounterStrikeViewport* pViewport ) :
	m_bVisible( false ),
	m_bInitialized( false ),
	m_bLoading( true ),
	m_bIsLoaded( false ),
	m_bRegisteredEvents( false ),
	m_pViewport( pViewport ),
	m_iSelectedWeapon( -1 )
	//m_iUnknownState( -1 )
{
	m_iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();


	if ( !s_LoadoutArray )
	{
		bool shouldInit = false;

		for ( int i = 0; i < 2; ++i )
		{
			for ( int s = 0; s < 4; ++s )
			{
				if ( s_defaultLoadoutWeapons[i][s].weapon && s_defaultLoadoutWeapons[i][s].count )
				{
					shouldInit = true;
					break;
				}
			}
		}

		if ( !shouldInit )
		{
			InitBuyMenuLoadoutData();
		}
	}

	vgui::VPANEL pRoot = enginevgui->GetPanel( PANEL_CLIENTDLL );

	m_pItemPanelParent = new vgui::EditablePanel( pViewport, "buymenu_itempanel_parent" );
	m_pItemPanelParent->MakeReadyForUse();
	m_pItemPanelParent->LoadControlSettings( "Resource/UI/econ/buymenu_itempanel_parent.res" );

	vgui::Panel* m_pItemModelPanel = m_pItemPanelParent->FindChildByName( "buymenu_itempanel", false );

	if ( m_pItemModelPanel )
	{
		m_pItemModelPanel->SetParent( pRoot );
		m_pItemModelPanel->SetVisible( false );
		m_pItemModelPanel->SetMouseInputEnabled( false );
	}

	ListenForGameEvent( "round_start" );
}

CCSBuyMenuScaleform::~CCSBuyMenuScaleform()
{
	if ( m_bRegisteredEvents )
	{
		if ( gameeventmanager )
		{
			gameeventmanager->RemoveListener( this );
		}

		m_bRegisteredEvents = false;
	}

	// Release the movie if it is still loaded (the restoration set the slot
	// to 13 here, a value copied from disassembly, not a Scaleform slot)
	if ( m_bIsLoaded && FlashAPIIsValid() )
	{
		g_pScaleformUI->RemoveElement( SF_SS_SLOT( m_iSplitScreenSlot ), m_FlashAPI );
	}
}

void CCSBuyMenuScaleform::SetPlayerIsCT( const bool isCT )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( FlashAPIIsValid() )
	{
		SFVALUE value = m_pScaleformUI->CreateValue( 0 );

		m_pScaleformUI->Value_SetValue( value, isCT );
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setPlayerIsCT", value, 1 );
		m_pScaleformUI->ReleaseValue( value );
	}
}

void CCSBuyMenuScaleform::FlashReady()
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *LocalCSPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( LocalCSPlayer )
	{
		SetPlayerIsCT(LocalCSPlayer->GetTeamNumber() == 3);
	}

	CalculateBestStats();

	ListenForGameEvent( "cs_game_disconnected" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "item_equip" );

	m_bLoading = false;

	// The restoration uses m_bShowOnReady as "the movie is loaded" (money, time
	// left and wheel selection updates are skipped without it) but never set it:
	// the wheel always thought the player had $0.
	m_bShowOnReady = true;
	m_bIsLoaded = true;	// same story (hostage map setup)

	if ( m_bVisible )
	{
		Show();
	}
	else
	{
		Hide();
	}
}

void CCSBuyMenuScaleform::Show()
{
	// Register for radial input
	g_pInputSystem->SetSteamControllerMode( "RadialControls", this );

	// If the movie hasn't been loaded yet, start loading it.
	if ( m_bLoading )
	{
		if ( !FlashAPIIsValid() )
		{
			m_bLoading = true;
			SFUI_REQUEST_ELEMENT( SF_SS_SLOT( m_iSplitScreenSlot ), g_pScaleformUI, CCSBuyMenuScaleform, this, BuyMenu );
		}

		m_bVisible = true;
		return;
	}

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	m_bNeedUpdate = false;
	m_iSelectedCategory = 0;
	m_iCurrentRadialSelection = 0;

	UpdatePlayerCash( true );
	UpdateTimeLeft( true );

#if defined( CEG_ALLOW_BUYMENU )
	if (true)
#endif
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", 0, NULL );
		// SWF may skip ShowCursor/AddInputConsumer on the second open; force the cursor visible from C++
		g_pScaleformUI->ShowCursor();

		m_iWheelSelection = -1;

		GetHud().DisableHud();

		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pPlayer )
			pPlayer->SetBuyMenuOpen( true );

		bool bHostageMap = false;
		if ( CSGameRules() )
			bHostageMap = CSGameRules()->IsHostageRescueMap();

		SetHostageMatch( bHostageMap );

		m_bVisible = true;
	}
}

void CCSBuyMenuScaleform::Hide()
{
	// Unregister radial controls
	g_pInputSystem->SetSteamControllerMode( NULL, this );

	if ( !m_bLoading && FlashAPIIsValid() && m_bVisible )
	{
		SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", nullptr, 0 );

		GetHud().EnableHud();
	}

	m_bVisible = false;

	// Clear selected weapon model
	//if ( m_pWeaponModelPanel )
		//m_pWeaponModelPanel->SetWeapon( nullptr );

	{
		SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pPlayer )
			pPlayer->SetBuyMenuOpen( false );
	}
}

void CCSBuyMenuScaleform::FlashLoaded( void )
{

}

void CCSBuyMenuScaleform::CalculateBestStats()
{
	// The restoration wrote these at raw offsets copied from Valve's binary
	// (this + 0x264 / 0x284), which is not our layout; the stat callbacks read
	// m_WeaponStatRange, so compute that (one range for all guns).
	m_WeaponStatRange.m_flFirepowerMax = m_WeaponStatRange.m_flFireRateMax = 0.0f;
	m_WeaponStatRange.m_flAccuracyMax = m_WeaponStatRange.m_flMoveRateMax = 0.0f;
	m_WeaponStatRange.m_flFirepowerMin = m_WeaponStatRange.m_flFireRateMin = FLT_MAX;
	m_WeaponStatRange.m_flAccuracyMin = m_WeaponStatRange.m_flMoveRateMin = FLT_MAX;
	WeaponStatRange *pSecondaryStats = &m_WeaponStatRange;
	WeaponStatRange *pPrimaryStats = &m_WeaponStatRange;

	for ( int i = 1; i < WEAPON_MAX; ++i )
	{
		// Assembly skips weapon id 31
		if ( i == 31 )
			continue;

		const CCSWeaponInfo *pInfo = GetWeaponInfo( (CSWeaponID)i );
		if ( !pInfo )
			continue;

		if ( !IsGunWeapon( (CSWeaponType)pInfo->GetWeaponType( nullptr ) ) )
		{
			continue;
		}

		bool bKnife = pInfo->GetWeaponType( nullptr ) == WEAPONTYPE_KNIFE;

		WeaponStatRange *pStats = IsPrimaryWeapon( (CSWeaponID)i ) ? pPrimaryStats : pSecondaryStats;

		float damage = (float)pInfo->GetDamage( nullptr, 0, 1.0f );
		float armorDamage = pInfo->GetArmorRatio( nullptr, 0, 1.0f ) * damage;

		pStats->m_flFirepowerMax = MAX( pStats->m_flFirepowerMax, armorDamage );
		pStats->m_flFirepowerMin = MIN( pStats->m_flFirepowerMin, armorDamage );

		float cycleTime = pInfo->GetCycleTime( nullptr, 0, 1.0f );

		if ( cycleTime > 0.0f )
		{
			float rof = 1.0f / cycleTime;

			pStats->m_flFireRateMax = MAX( pStats->m_flFireRateMax, rof );
			pStats->m_flFireRateMin = MIN( pStats->m_flFireRateMin, rof );
		}

		float maxSpeed;

		if ( bKnife )
		{
			maxSpeed = pInfo->GetMaxSpeed( nullptr, 1, 1.0f );
		}
		else
		{
			maxSpeed = pInfo->GetMaxSpeed( nullptr, 0, 1.0f );
		}

		pStats->m_flMoveRateMax = MAX( pStats->m_flMoveRateMax, maxSpeed );
		pStats->m_flMoveRateMin = MIN( pStats->m_flMoveRateMin, maxSpeed );

		float inaccuracy;

		if ( bKnife )
		{
			inaccuracy = pInfo->GetInaccuracyStand( nullptr, 1, 0.0f );
		}
		else
		{
			inaccuracy = pInfo->GetInaccuracyStand( nullptr, 0, 0.0f );
		}

		float spread;

		if ( bKnife )
		{
			spread = pInfo->GetSpread( nullptr, 1, 0.0f );
		}
		else
		{
		spread = pInfo->GetSpread( nullptr, 0, 	0.0f );}

		float accuracy = 100.0f / ( spread + inaccuracy );
		pStats->m_flAccuracyMax = MAX( pStats->m_flAccuracyMax, accuracy );
		pStats->m_flAccuracyMin = MIN( pStats->m_flAccuracyMin, accuracy );
	}
}

bool CCSBuyMenuScaleform::PreUnloadFlash()
{
	m_bShowOnReady = false;
	m_bIsLoaded = false;

	// WIP
	return true;
}

void CCSBuyMenuScaleform::SetHostageMatch( bool bHostageMatch )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( m_bIsLoaded )
	{
		// the restoration wrote element "true" (1) of a 1-element array, which
		// is a fatal Error(), and never released the array
		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, bHostageMatch );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setIsHostageMatch", args, 1 );
		}
	}
}

void CCSBuyMenuScaleform::ShowPanel( bool state )
{
	if ( state == m_bVisible )
	{
		return;
	}

	if ( state )
	{
		Show();
	}
	else
	{
		Hide();
	}
}

void CCSBuyMenuScaleform::OnCancel( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_BasePlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	int nUserID = -1;
	if ( pPlayer )
	{
		nUserID = pPlayer->GetUserID();
	}

	CSGameRules()->CloseBuyMenu( nUserID );
}

void CCSBuyMenuScaleform::InitWeapon( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	SFBuyMenuLoadout loadout;
	C_CSPlayer* pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int loadoutSlot = -1;
	
	if ( pui->Params_GetArgAsNumber( obj ) != 0.0 )
		loadoutSlot = (int)pui->Params_GetArgAsNumber( obj );

	int team = pPlayer->GetTeamNumber();

	const C_EconItemView* pItem = nullptr;

	// Resolve inventory item for loadout slot
	if ( loadoutSlot >= 0 && loadoutSlot < 57 )
	{
		uint64 itemID = loadout.m_WeaponID[loadoutSlot];
	
		if ( itemID != INVALID_ITEM_ID )
		{
			CCSInventoryManager *pInv = CSInventoryManager();
	
			if ( pInv && pInv->GetLocalInventory() )
			{
				pItem = pInv->GetLocalInventory()->GetInventoryItemByItemID( itemID );
	
				if ( !pItem || !pItem->IsValid() )
				{
					if ( ( itemID >> 60 ) == 15 )
					{
						pItem = pInv->FindOrCreateReferenceEconItem( itemID );
					}
					else
					{
						pItem = pInv->GetItemInLoadoutForTeam( team, loadoutSlot );
					}
				}
			}
		}
	}

	// Translate buy-menu slot -> weapon ID
	//if ( (unsigned)(loadoutSlot - 26) > 11 )
		//return;

	CSWeaponID weaponID = WEAPON_NONE;

	switch ( loadoutSlot )
	{
	case LOADOUT_POSITION_EQUIPMENT0: weaponID = CSGameRules()->IsPlayingCoopMission() ? ITEM_ASSAULTSUIT : ITEM_KEVLAR;
		break;

	case LOADOUT_POSITION_EQUIPMENT1: weaponID = CSGameRules()->IsPlayingCoopMission() ? ITEM_HEAVYASSAULTSUIT : ITEM_ASSAULTSUIT;
		break;
	
	case LOADOUT_POSITION_EQUIPMENT2: weaponID = WEAPON_TASER;
		break;
	
	case LOADOUT_POSITION_EQUIPMENT3: weaponID = CSGameRules()->IsHostageRescueMap() ? ITEM_CUTTERS : ITEM_DEFUSER;
		break;
	}

	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponID );

	if ( !pWeaponInfo )
		return;

	CSWeaponType weaponType = pWeaponInfo->GetWeaponType( pItem );

	int ammoType = pWeaponInfo->GetPrimaryAmmoType( pItem );
	int clipSize = pWeaponInfo->GetPrimaryClipSize( pItem );
	int maxCarry = GetCSAmmoDef()->MaxCarry( pWeaponInfo->GetPrimaryAmmoType(), pPlayer );

	// Create Scaleform object
	int weaponObj = 0;
	SFVALUE weaponData = m_pScaleformUI->CreateNewObject( weaponObj );

	/*const wchar_t* localizedName = g_pVGuiLocalize->Find( pWeaponInfo->szPrintName );*/

	//if ( pItem && pItem->IsValid() )
	//	localizedName = pItem->GetItemName();

	/*SFVALUE nameValue = m_pScaleformUI->CreateNewString( weaponObj, localizedName );*/
	SFVALUE nameValue = m_pScaleformUI->CreateNewString( weaponObj, pItem->GetItemName() );

	// Weapon type string
	m_pScaleformUI->Value_SetMember( weaponData, "maxCarry", 1 );

	if ( weaponID == WEAPON_TASER )
	{
		m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "taser" );
	}
	else
	{
		switch ( weaponType )
		{
		case WEAPONTYPE_PISTOL:
			m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "secondary" );
			break;

		case WEAPONTYPE_GRENADE:
			m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "equipment" );

			m_pScaleformUI->Value_SetMember( weaponData, "maxCarry", maxCarry );
			break;

		case WEAPONTYPE_C4:
			m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "equipment" );
			break;

		default:
			m_pScaleformUI->Value_SetMember( weaponData, "weaponType", "primary" );
			break;
		}
	}

	// Item ID
	uint64 itemID = (uint64)-1;

	if ( pItem && pItem->IsValid() )
		itemID = pItem->GetItemID();

	char itemIDBuf[256];
	V_sprintf_safe(itemIDBuf, "%llu", itemID);

	m_pScaleformUI->Value_SetMember( weaponData, "itemid", itemIDBuf );
	m_pScaleformUI->Value_SetMember( weaponData, "wepid", weaponID );

	// Price
	int price = -1;

	if ( pItem || weaponID )
	{
		price = 0;

		if ( C_CSPlayer* local = C_CSPlayer::GetLocalCSPlayer() )
		{
			price = local->GetWeaponPrice( weaponID );
		}
	}

	m_pScaleformUI->Value_SetMember( weaponData, "price", price );
	m_pScaleformUI->Value_SetMember( weaponData, "name", nameValue );
	m_pScaleformUI->Value_SetMember( weaponData, "clipSize", clipSize );
	m_pScaleformUI->Value_SetMember( weaponData, "maxRounds", GetAmmoDef()->MaxCarry( ammoType, pPlayer ) );

	// Stat bars
	m_pScaleformUI->Value_SetMember( weaponData, "armorPen", pWeaponInfo->GetArmorRatio( pItem ) * 50.0f );
	m_pScaleformUI->Value_SetMember( weaponData, "penetration", pWeaponInfo->GetPenetration( pItem ) );

	// Deathmatch points
	int dmScore = CSGameRules()->GetWeaponScoreForDeathmatch( weaponID );

	char dmBuf[64];
	V_snprintf( dmBuf, sizeof(dmBuf), " %d", dmScore );

	m_pScaleformUI->Value_SetMember( weaponData, "DMPoints", dmBuf );

	// Kill reward
	int killAward = pWeaponInfo->GetKillAward( pItem );
	int baseCash = 300;

	if ( CSGameRules() )
	{
		//baseCash = CSGameRules()->PlayerCashAwardValue( killAward );
	}

	char killBuf[64];
	V_snprintf( killBuf, sizeof( killBuf ), " %.2f", (float)killAward / (float)baseCash );

	m_pScaleformUI->Value_SetMember( weaponData, "KillAward", killBuf );

	// Fire rate
	float cycleTime = pWeaponInfo->GetCycleTime();
	float roundsPerSecond = cycleTime > 0.0f ? 1.0f / cycleTime : 0.0f;

	//m_pScaleformUI->Value_SetMember( weaponData, "fireRate", ComputeStatBar( roundsPerSecond, statTable ) );

	// Movement + accuracy
	float maxSpeed;
	float spread;
	float inaccuracy;

	if (weaponType == WEAPONTYPE_SNIPER_RIFLE)
	{
		maxSpeed = pWeaponInfo->GetMaxSpeed( pItem, true );
		inaccuracy = pWeaponInfo->GetInaccuracyStand( pItem, true );
		spread = pWeaponInfo->GetSpread( pItem, true );
	}
	else
	{
		maxSpeed = pWeaponInfo->GetMaxSpeed( pItem, false );
		inaccuracy = pWeaponInfo->GetInaccuracyStand( pItem, false );
		spread = pWeaponInfo->GetSpread( pItem, false );
	}

	//m_pScaleformUI->Value_SetMember( weaponData, "moveRate", ComputeStatBar( maxSpeed, statTable ) );
	//m_pScaleformUI->Value_SetMember( weaponData, "inaccuracy", ComputeStatBar( 100.0f / ( inaccuracy + spread ), statTable ) );

	// Store object in Scaleform array
	m_pScaleformUI->Params_SetResult( obj, weaponData );

	if ( nameValue )
		SafeReleaseSFVALUE( nameValue );

	if ( weaponData )
		SafeReleaseSFVALUE( weaponData );

	return;
}

void CCSBuyMenuScaleform::GetWeaponPriceScript( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int nLoadoutPos = ( int )pui->Params_GetArgAsNumber( obj );

	int nWeaponID = WEAPON_NONE;
	bool bInvalid = true;

	int nTeam = pPlayer->GetTeamNumber();

	if ( nLoadoutPos <= 56 )
	{
		CCSPlayerInventory *pInventory = CSInventoryManager() ? CSInventoryManager()->GetLocalCSInventory() : nullptr;

		if ( pInventory )
		{
			itemid_t itemID = m_PlayerBuyMenuLoadout.m_WeaponID[nLoadoutPos];

			if ( itemID != INVALID_ITEM_ID )
			{
				CEconItemView *pItem = pInventory->GetInventoryItemByItemID( itemID );

				if ( !pItem || !pItem->IsValid() )
				{
					if ( CombinedItemIdIsDefIndexAndPaint( itemID ) )
					{
						pItem = CSInventoryManager()->FindOrCreateReferenceEconItem( itemID );
					}
					else
					{
						pItem = CSInventoryManager()->GetItemInLoadoutForTeam( nTeam, nLoadoutPos );
					}
				}

				if ( pItem && pItem->IsValid() )
				{
					const CEconItemDefinition *pDef = pItem->GetStaticData();

					nWeaponID = WeaponIdFromString( pDef->GetDefinitionName() );

					bInvalid = false;
				}
			}
		}
	}

	int nPrice = -1;

	if ( !bInvalid || nWeaponID )
	{
		if ( C_CSPlayer *pLocal = C_CSPlayer::GetLocalCSPlayer() )
		{
			nPrice = pLocal->GetWeaponPrice(static_cast<CSWeaponID>( nWeaponID ));
		}
		else
		{
			nPrice = 0;
		}
	}

	m_pScaleformUI->Params_SetResult( obj, nPrice );
}

void CCSBuyMenuScaleform::GetWeaponPriceFromIDScript( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( !C_CSPlayer::GetLocalCSPlayer() )
		return;

	CSWeaponID weaponID = static_cast<CSWeaponID>( static_cast<int>( m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) ) );

	int nPrice = -1;

	if ( weaponID != WEAPON_NONE )
	{
		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

		nPrice = 0;

		if ( pPlayer )
		{
			nPrice = pPlayer->GetWeaponPrice( weaponID );
		}
	}

	m_pScaleformUI->Params_SetResult( obj, nPrice );
}

void CCSBuyMenuScaleform::GetWeaponShortNameFromPosition( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int iLoadoutPosition = -1;

	if ( m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) != 0.0 )
	{
		iLoadoutPosition = ( int )m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	}

	int iWeaponID = ( int )m_pScaleformUI->Params_GetArgAsNumber( obj, 1 );

	int iTeam = pPlayer->GetTeamNumber();

	if ( iLoadoutPosition > 56 )
		return;

	if ( !CSInventoryManager() )
		return;

	CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

	if ( !pInventory )
		return;

	itemid_t itemID = m_PlayerBuyMenuLoadout.m_WeaponID[ iLoadoutPosition ];

	if ( itemID == INVALID_ITEM_ID )
		return;

	C_EconItemView *pItem = pInventory->GetInventoryItemByItemID( itemID );

	// If inventory lookup failed, try fallback methods.
	if ( !pItem || !pItem->GetStaticData() )
	{
		if ( ( itemID >> 60 ) == 0xF )
		{
			pItem = CSInventoryManager()->FindOrCreateReferenceEconItem( itemID );
		}
		else
		{
			pItem = CSInventoryManager()->GetItemInLoadoutForTeam( iTeam, iLoadoutPosition );
		}
	}

	if ( !pItem )
		return;

	const CEconItemDefinition *pDef = pItem->GetStaticData();

	if ( !pDef )
		return;

	const char *pszClass = pDef->GetItemClass();

	// Preferred path:
	// "weapon_ak47" -> "ak47"
	if ( pszClass && IsWeaponClassname( pszClass ) )
	{
		SFVALUE result = CreateFlashString( pszClass + 7 );

		m_pScaleformUI->Params_SetResult( obj, result );

		SafeReleaseSFVALUE(result);
		return;
	}

	// Fallback:
	const char *pszAlias = WeaponIDToAlias( ( CSWeaponID )iWeaponID );

	if ( pszAlias && pszAlias[0] )
	{
		SFVALUE result = CreateFlashString( pszAlias );

		m_pScaleformUI->Params_SetResult( obj, result );

		SafeReleaseSFVALUE( result );
	}
}

void CCSBuyMenuScaleform::GetWeaponFirepower( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	int weaponID = (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );

	float firepower = GetWeaponFirepowerRawValue( weaponID );

	m_pScaleformUI->Params_SetResult( obj, firepower );
}

void CCSBuyMenuScaleform::GetWeaponFireRate( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	WeaponStatRange *stats;
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int position = (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );

	int team = pPlayer->GetTeamNumber();

	C_EconItemView *pItem = NULL;

	if ( position < LOADOUT_POSITION_COUNT )
	{
		itemid_t itemID = m_PlayerBuyMenuLoadout.m_WeaponID[position];

		if ( itemID != INVALID_ITEM_ID )
		{
			CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

			if ( pInventory )
			{
				pItem = pInventory->GetInventoryItemByItemID( itemID );

				if ( !pItem || !pItem->GetStaticData() )
				{
					if ( ( itemID >> 60 ) == 0xF )
					{
						pItem = CSInventoryManager()->FindOrCreateReferenceEconItem( itemID );
					}
					else
					{
						pItem = CSInventoryManager()->GetItemInLoadoutForTeam( team, position );
					}
				}
			}
		}
	}

	const CCSWeaponInfo *pWeaponInfo = GetWeaponInfoForPosition( position );

	float fireRate = 0.0f;

	if ( pWeaponInfo )
	{
		float cycleTime = pWeaponInfo->GetCycleTime( nullptr, 0, 1.0f );

		if ( cycleTime > 0.0f )
		{
			fireRate = 1.0f / cycleTime;
		}
	}

	CSWeaponID weaponID = WEAPON_NONE;

	if ( pItem && pItem->GetStaticData() )
	{
		weaponID = WeaponIdFromString( pItem->GetStaticData()->GetItemClass() );
	}

	float minRate;
	float maxRate;

	if ( IsPrimaryWeapon( weaponID ) )
	{
		minRate = m_WeaponStatRange.m_flFireRateMin;
		maxRate = m_WeaponStatRange.m_flFireRateMax;
	}
	else
	{
		minRate = m_WeaponStatRange.m_flFireRateMin;
		maxRate = m_WeaponStatRange.m_flFireRateMax;
	}

	float normalized = ( ( fireRate - minRate ) * 100.0f ) / ( maxRate - minRate );

	normalized = clamp( normalized + 0.5f, 0.0f, 100.0f );

	m_pScaleformUI->Params_SetResult( obj, (int)floorf( normalized ) );
}

void CCSBuyMenuScaleform::GetPlayerBuyMenuLoadout( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SFBuyMenuLoadout tempLoadout;
	memset( &tempLoadout, 0, sizeof( tempLoadout ) );

	if ( FillInPlayerBuyMenuLoadout( &tempLoadout ) )
	{
		memcpy( &m_PlayerBuyMenuLoadout, &tempLoadout, sizeof( tempLoadout ) );
	}

	SFVALUE pFlashBuyMenuLoadout = CreateFlashBuyMenuLoadout( m_PlayerBuyMenuLoadout );

	m_pScaleformUI->Params_SetResult( obj, pFlashBuyMenuLoadout );

	if ( pFlashBuyMenuLoadout )
	{
		m_pScaleformUI->ReleaseValue( pFlashBuyMenuLoadout );
	}
}

void CCSBuyMenuScaleform::GetWeaponMoveRate( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot )

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int position = (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );

	int team = pPlayer->GetTeamNumber();

	C_EconItemView *pItem = NULL;

	if ( position < LOADOUT_POSITION_COUNT )
	{
		itemid_t itemID = m_PlayerBuyMenuLoadout.m_WeaponID[position];

		if ( itemID != INVALID_ITEM_ID )
		{
			CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

			if ( pInventory )
			{
				pItem = pInventory->GetInventoryItemByItemID( itemID );

				if ( !pItem || !pItem->GetStaticData() )
				{
					if ( ( itemID >> 60 ) == 0xF )
					{
						pItem = CSInventoryManager()->FindOrCreateReferenceEconItem( itemID );
					}
					else
					{
						pItem = CSInventoryManager()->GetItemInLoadoutForTeam( team, position );
					}
				}
			}
		}
	}

	const CCSWeaponInfo *pWeaponInfo = GetWeaponInfoForPosition( position );

	if ( !pWeaponInfo )
		return;

	CSWeaponID weaponID = WEAPON_NONE;

	if ( pItem && pItem->GetStaticData() )
	{
		weaponID = WeaponIdFromString( pItem->GetStaticData()->GetItemClass() );
	}

	float maxSpeed;

	if ( pWeaponInfo->GetWeaponType( pItem ) == WEAPONTYPE_KNIFE || pWeaponInfo->IsRevolver( pItem, false, 1.0f ) )
	{
		maxSpeed = pWeaponInfo->GetMaxSpeed( pItem, true, 1.0f );
	}
	else
	{
		maxSpeed = pWeaponInfo->GetMaxSpeed( pItem, false, 1.0f );
	}

	float minSpeed;
	float maxSpeedRange;

	if ( IsPrimaryWeapon( weaponID ) )
	{
		maxSpeedRange = m_WeaponStatRange.m_flMoveRateMax;
		minSpeed = m_WeaponStatRange.m_flMoveRateMin;
	}
	else
	{
		maxSpeedRange = m_WeaponStatRange.m_flMoveRateMax;
		minSpeed = m_WeaponStatRange.m_flMoveRateMin;
	}

	float normalized = ( ( maxSpeed - minSpeed ) * 100.0f ) / ( maxSpeedRange - minSpeed );

	normalized = (int)floorf( clamp( normalized + 0.5f, 0.0f, 100.0f ) );

	m_pScaleformUI->Params_SetResult( obj, normalized );
}

void CCSBuyMenuScaleform::GetWeaponHandling( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	// accuracy (standing inaccuracy + spread), normalized over all guns like the other bars
	float flAccuracy = 100.0f / MAX( 0.0001f, pInfo->GetSpread( NULL, 0, 0.0f ) + pInfo->GetInaccuracyStand( NULL, 0, 0.0f ) );
	float flRange = m_WeaponStatRange.m_flAccuracyMax - m_WeaponStatRange.m_flAccuracyMin;
	float flNormalized = flRange > 0.0f ? ( flAccuracy - m_WeaponStatRange.m_flAccuracyMin ) * 100.0f / flRange : 50.0f;
	m_pScaleformUI->Params_SetResult( obj, (int)clamp( flNormalized + 0.5f, 0.0f, 100.0f ) );
}

void CCSBuyMenuScaleform::GetWeaponArmorPen( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	m_pScaleformUI->Params_SetResult( obj, (int)clamp( pInfo->GetArmorRatio() * 50.0f + 0.5f, 0.0f, 100.0f ) );
}

void CCSBuyMenuScaleform::GetWeaponClipSize( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	m_pScaleformUI->Params_SetResult( obj, pInfo->GetPrimaryClipSize() );
}

void CCSBuyMenuScaleform::GetWeaponMaxCarry( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	m_pScaleformUI->Params_SetResult( obj, pInfo->GetPrimaryReserveAmmoMax() );
}

void CCSBuyMenuScaleform::GetWeaponPenetration( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	m_pScaleformUI->Params_SetResult( obj, pInfo->GetPenetration() );
}

void CCSBuyMenuScaleform::GetWeaponDMPoints( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	// deathmatch awards points instead of money; no per-weapon table here
	m_pScaleformUI->Params_SetResult( obj, 10 );
}

void CCSBuyMenuScaleform::GetWeaponKillAward( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	m_pScaleformUI->Params_SetResult( obj, pInfo->GetKillAward() );
}

void CCSBuyMenuScaleform::GetWeaponItemID( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// only used for stickers, which need the Steam inventory
	m_pScaleformUI->Params_SetResult( obj, "0" );
}

void CCSBuyMenuScaleform::GetWeaponFirepowerRaw( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	m_pScaleformUI->Params_SetResult( obj, pInfo->GetDamage() );
}

void CCSBuyMenuScaleform::GetWeaponArmorPenRaw( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	// the movie shows raw / 100 as the percentage
	m_pScaleformUI->Params_SetResult( obj, (int)( pInfo->GetArmorRatio() * 50.0f * 100.0f + 0.5f ) );
}

void CCSBuyMenuScaleform::GetWeaponRawMoveRatio( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	m_pScaleformUI->Params_SetResult( obj, (int)pInfo->GetMaxSpeed() );
}

void CCSBuyMenuScaleform::GetEffectiveRange( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	// distance (in meters) at which damage falls to 75%: rangemod ^ ( d / 500 ) = 0.75
	float flRangeMod = pInfo->GetRangeModifier();
	float flUnits = ( flRangeMod > 0.0f && flRangeMod < 1.0f ) ? 500.0f * logf( 0.75f ) / logf( flRangeMod ) : pInfo->GetRange();
	float flMeters = MIN( flUnits, pInfo->GetRange() ) * 0.0254f;
	m_pScaleformUI->Params_SetResult( obj, (int)clamp( flMeters * 100.0f / 60.0f + 0.5f, 0.0f, 100.0f ) );
}

void CCSBuyMenuScaleform::GetEffectiveRangeRaw( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo )
		return;
	// distance (in meters) at which damage falls to 75%: rangemod ^ ( d / 500 ) = 0.75
	float flRangeMod = pInfo->GetRangeModifier();
	float flUnits = ( flRangeMod > 0.0f && flRangeMod < 1.0f ) ? 500.0f * logf( 0.75f ) / logf( flRangeMod ) : pInfo->GetRange();
	float flMeters = MIN( flUnits, pInfo->GetRange() ) * 0.0254f;
	m_pScaleformUI->Params_SetResult( obj, (int)( flMeters + 0.5f ) );
}

void CCSBuyMenuScaleform::GetPlayerLoadout( SCALEFORM_CALLBACK_ARGS_DECL )
{
	UpdatePlayerLoadout( true, false );

	SFVALUE pFlashLoadout = CreateFlashLoadout( m_PlayerLoadout );

	C_CSPlayer* pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pLocalPlayer && pLocalPlayer->IsAlive() && !CSGameRules()->IsArmorFree() && pLocalPlayer->ArmorValue() > 0 )
	{
		m_pScaleformUI->Value_SetMember( pFlashLoadout, "armor", pLocalPlayer->ArmorValue() );
	}

	m_pScaleformUI->Params_SetResult( obj, pFlashLoadout );

	if ( pFlashLoadout )
	{
		SafeReleaseSFVALUE( pFlashLoadout );
	}
}

void CCSBuyMenuScaleform::AreWeaponsFree( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool IsPlayingGunGameDeathmatch = CSGameRules()->IsPlayingGunGameDeathmatch();

	m_pScaleformUI->Params_SetResult( obj, IsPlayingGunGameDeathmatch );
}

void CCSBuyMenuScaleform::Autobuy( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	engine->ClientCmd( "autobuy" );

	m_bNeedUpdate = true;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	int userID = -1;
	if ( pPlayer )
	{
		userID = pPlayer->GetUserID();
	}

	if ( g_pGameRules )
	{
		CSGameRules()->CloseBuyMenu( userID );
	}
}

void CCSBuyMenuScaleform::BuyWeapon( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( !C_CSPlayer::GetLocalCSPlayer() )
		return;

	const char *pszWeapon = "";

	if ( m_pScaleformUI->Params_GetNumArgs( obj ) > 0 && m_pScaleformUI->Params_GetArgType( obj ) == IUIMarshalHelper::VT_String )
	{
		pszWeapon = m_pScaleformUI->Params_GetArgAsString( obj );
	}

	int nCount = (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 1 );

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	char command[100];
	V_snprintf( command, sizeof( command ), "buy %s %d", pszWeapon, nCount );

	engine->ExecuteClientCmd( command );

	m_bNeedUpdate = true;
}

void CCSBuyMenuScaleform::BuyPrevious( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	// Execute the rebuy command
	engine->ClientCmd( "rebuy" );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	int userID = -1;
	if ( pPlayer )
	{
		userID = pPlayer->GetUserID();
	}

	if ( g_pGameRules )
	{
		CSGameRules()->CloseBuyMenu( userID );
	}
}

void CCSBuyMenuScaleform::CanAcquire( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	// Optional weapon alias passed from Flash
	const char *pszWeapon = "";

	if ( m_pScaleformUI->Params_GetNumArgs( obj ) > 0 && m_pScaleformUI->Params_GetArgType( obj ) == IUIMarshalHelper::VT_String )
	{
		pszWeapon = m_pScaleformUI->Params_GetArgAsString( obj );
	}

	int weaponID = AliasToWeaponID(pszWeapon);

	// Flash also passes a loadout slot
	int loadoutSlot = -1;

	if ( m_pScaleformUI->Params_GetNumArgs( obj ) == 2 )
	{
		loadoutSlot = (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 1 );
	}

	CEconItemView *pItem = nullptr;

	if ( loadoutSlot >= 0 && loadoutSlot < LOADOUT_POSITION_COUNT )
	{
		itemid_t itemID = m_PlayerBuyMenuLoadout.m_WeaponID[loadoutSlot];

		if ( itemID != INVALID_ITEM_ID )
		{
			CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

			if ( pInventory )
			{
				pItem = pInventory->GetInventoryItemByItemID(itemID);

				if ( !pItem || !pItem->IsValid() )
				{
					if ( ( itemID >> 60 ) == 0xF )
					{
						pItem = CSInventoryManager()->FindOrCreateReferenceEconItem( itemID );
					}
					else
					{
						pItem = CSInventoryManager()->GetItemInLoadoutForTeam( pPlayer->GetTeamNumber(), loadoutSlot );
					}
				}
			}
		}
	}

	AcquireResult::Type result = pPlayer->CanAcquire( (CSWeaponID)weaponID, AcquireMethod::Buy );

	m_pScaleformUI->Params_SetResult( obj, result );
}

void CCSBuyMenuScaleform::WeaponIsValid( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bValid = false;

	double type = m_pScaleformUI->Params_GetArgType( obj );

	if ( type == IUIMarshalHelper::VT_Number ) // The object argument is a loadout position.
	{
		int nPosition = (int)m_pScaleformUI->Params_GetArgAsNumber( obj );

		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

		if ( pPlayer )
		{
			int nTeam = pPlayer->GetTeamNumber();

			if ( nPosition >= 0 && nPosition < LOADOUT_POSITION_COUNT )
			{
				itemid_t itemID = m_PlayerBuyMenuLoadout.m_WeaponID[nPosition];

				C_EconItemView *pItem = NULL;

				if ( itemID != INVALID_ITEM_ID )
				{
					CCSInventoryManager *pInv = CSInventoryManager();

					if ( pInv && pInv->GetLocalInventory() )
					{
						pItem = pInv->GetLocalInventory()->GetInventoryItemByItemID( itemID );

						if ( !pItem || !pItem->GetStaticData() )
						{
							if ( ( itemID >> 60 ) == 0xF )
							{
								pItem = pInv->FindOrCreateReferenceEconItem( itemID );
							}
							else
							{
								pItem = pInv->GetItemInLoadoutForTeam( nTeam, nPosition );
							}
						}
					}
				}

				bValid = ( pItem != NULL );
			}
		}
	}

	else if ( type == IUIMarshalHelper::VT_String ) // The object argument is a weapon alias.
	{
		const char *pszAlias = m_pScaleformUI->Params_GetArgAsString( obj );

		int weaponID = AliasToWeaponID( pszAlias );

		if ( weaponID )
		{
			bValid = ( GetWeaponInfo( (CSWeaponID)weaponID ) != NULL );
		}
	}

	m_pScaleformUI->Params_SetResult( obj, bValid ); // Return the result to the action script.
}

const CCSWeaponInfo *CCSBuyMenuScaleform::GetWeaponInfoForPosition( int position )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return nullptr;

	int team = pPlayer->GetTeamNumber();

	if ( position >= LOADOUT_POSITION_COUNT )
		return nullptr;

	if ( !CSInventoryManager() )
		return nullptr;

	CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

	if ( !pInventory )
		return nullptr;

	itemid_t itemID = m_PlayerBuyMenuLoadout.m_WeaponID[position];

	if ( itemID == INVALID_ITEM_ID )
		return nullptr;

	C_EconItemView *pItem = pInventory->GetInventoryItemByItemID( itemID );

	if ( !pItem || !pItem->GetStaticData() )
	{
		if ( ( itemID >> 60 ) == 0xF )
		{
			pItem = CSInventoryManager()->FindOrCreateReferenceEconItem( itemID );
		}
		else
		{
			pItem = CSInventoryManager()->GetItemInLoadoutForTeam( team, position );
		}

		if ( !pItem || !pItem->GetStaticData() )
			return nullptr;
	}

	const CEconItemDefinition *pDef = pItem->GetStaticData();

	const char *pszClassname = pDef ? pDef->GetItemClass() : "";

	CSWeaponID weaponID = WeaponIdFromString( pszClassname );

	if ( weaponID == WEAPON_NONE )
		return nullptr;

	return GetWeaponInfo( weaponID );
}

uint32 CCSBuyMenuScaleform::GetWeaponFirepowerRawValue( uint32 position )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return 0;

	int team = pPlayer->GetTeamNumber();

	const CEconItemView *pItem = nullptr;

	if ( position < LOADOUT_POSITION_COUNT )
	{
		itemid_t itemID = m_PlayerBuyMenuLoadout.m_WeaponID[position];

		if ( itemID != INVALID_ITEM_ID )
		{
			CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

			if ( pInventory )
			{
				pItem = pInventory->GetInventoryItemByItemID( itemID );

				// Fallback if not a real inventory item
				if ( !pItem || !pItem->GetStaticData() )
				{
					if ( ( itemID >> 60 ) == 0xF )
					{
						pItem = CSInventoryManager()->FindOrCreateReferenceEconItem( itemID );
					}
					else
					{
						pItem = CSInventoryManager()->GetItemInLoadoutForTeam( team, position );
					}
				}
			}
		}
	}

	const CCSWeaponInfo *pWeaponInfo = GetWeaponInfoForPosition( position );

	if ( !pWeaponInfo )
		return 0;

	int damage = pWeaponInfo->GetDamage( pItem, false, 1.0f );
	int bullets = pWeaponInfo->GetBullets( pItem, false, 1.0f );

	return damage * bullets;
}

bool CCSBuyMenuScaleform::SetBuyMenuWeaponSliceEntry( const SFBuyMenuLoadout &loadout, int weaponPosition, SFVALUE flashArray, uint32 arrayIndex )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return false;

	SFVALUE flashObject = CreateFlashObject();

	m_pScaleformUI->Value_SetMember( flashObject, "sliceType", "weapon" );

	itemid_t itemID = loadout.m_WeaponID[ weaponPosition ];

	// Equipped weapon
	if ( itemID != INVALID_ITEM_ID )
	{
		C_EconItemView *pItem = nullptr;

		CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

		if ( ( itemID >> 60 ) == 0xF )
		{
			pItem = CSInventoryManager()->FindOrCreateReferenceEconItem( itemID );
		}
		else if ( pInventory )
		{
			pItem = pInventory->GetInventoryItemByItemID( itemID );
		}

		if ( pItem )
		{
			char itemIDString[256];
			V_snprintf( itemIDString, sizeof( itemIDString ), "%llu", itemID );

			m_pScaleformUI->Value_SetMember( flashObject, "weapon_itemid", itemIDString );
			m_pScaleformUI->Value_SetMember( flashObject, "weapon_position", weaponPosition );

			const CEconItemDefinition *pDef = pItem->GetStaticData();

			const char *pszClassname = pDef ? pDef->GetItemClass() : "";

			if ( IsWeaponClassname( pszClassname ) )
				pszClassname += 7;

			m_pScaleformUI->Value_SetMember( flashObject, "weapon", pszClassname );
		}
	}

	// Prohibited replacement weapon
	CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

	C_EconItemView *pProhibitedItem = pInventory->GetItemInLoadoutFilteredByProhibition( pLocalPlayer->GetTeamNumber(), weaponPosition );

	if ( pProhibitedItem )
	{
		if ( CSGameRules()->IsWeaponAllowed( nullptr, pLocalPlayer->GetTeamNumber(), pProhibitedItem ) )
		{
			uint16 defIndex = pProhibitedItem->GetStaticData()->GetDefinitionIndex();
			C_EconItemView *pReference = CSInventoryManager()->FindOrCreateReferenceEconItem( defIndex, 0, 0 );
			uint64 prohibitedID = pReference->GetFauxItemIDFromDefinitionIndex();

			char prohibitedString[256];
			V_snprintf( prohibitedString, sizeof( prohibitedString ), "%llu", prohibitedID );

			m_pScaleformUI->Value_SetMember( flashObject, "weapon_prohibiteditemid", prohibitedString );
		}
	}

	m_pScaleformUI->Value_SetArrayElement( flashArray, arrayIndex, flashObject );

	SafeReleaseSFVALUE( flashObject );

	return true;
}

SFVALUE CCSBuyMenuScaleform::CreateFlashBuyMenuLoadout( const SFBuyMenuLoadout &loadout )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return SFVALUE();

	SFVALUE rootMenu = CreateFlashObject();

	// MAIN MENU

	SFVALUE mainMenu = CreateFlashObject();
	m_pScaleformUI->Value_SetMember( mainMenu, "centerText", "#SFUI_BuyMenu_SelectWeapon" );
	m_pScaleformUI->Value_SetMember( mainMenu, "menuType", "parent" );

	SFVALUE menuSlices = CreateFlashArray( 24 );

	SFVALUE pistolSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember( pistolSlice, "sliceType", "subMenu" );
	m_pScaleformUI->Value_SetMember( pistolSlice, "name", "#SFUI_BuyMenu_Pistols" );
	m_pScaleformUI->Value_SetMember( pistolSlice, "image", "glock" );
	m_pScaleformUI->Value_SetMember( pistolSlice, "subMenu", "PistolMenu" );
	m_pScaleformUI->Value_SetArrayElement( menuSlices, 0, pistolSlice );

	SFVALUE heavySlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember( heavySlice, "sliceType", "subMenu" );
	m_pScaleformUI->Value_SetMember( heavySlice, "name", "#SFUI_BuyMenu_HeavyWeapons" );
	m_pScaleformUI->Value_SetMember( heavySlice, "image", "m249" );
	m_pScaleformUI->Value_SetMember( heavySlice, "subMenu", "HeavyMenu" );
	m_pScaleformUI->Value_SetArrayElement( menuSlices, 1, heavySlice );

	SFVALUE smgSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember( smgSlice, "sliceType", "subMenu" );
	m_pScaleformUI->Value_SetMember( smgSlice, "name", "#SFUI_BuyMenu_SMGs" );
	m_pScaleformUI->Value_SetMember( smgSlice, "image", "p90" );
	m_pScaleformUI->Value_SetMember( smgSlice, "subMenu", "SMGMenu" );
	m_pScaleformUI->Value_SetArrayElement( menuSlices, 2, smgSlice );

	SFVALUE rifleSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember( rifleSlice, "sliceType", "subMenu" );
	m_pScaleformUI->Value_SetMember( rifleSlice, "name", "#SFUI_BuyMenu_Rifles" );
	m_pScaleformUI->Value_SetMember( rifleSlice, "image", "m4a1" );
	m_pScaleformUI->Value_SetMember( rifleSlice, "subMenu", "RifleMenu" );
	m_pScaleformUI->Value_SetArrayElement( menuSlices, 3, rifleSlice );

	SFVALUE equipmentSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember( equipmentSlice, "sliceType", "subMenu" );
	m_pScaleformUI->Value_SetMember( equipmentSlice, "name", "#SFUI_BuyMenu_Equipment" );
	m_pScaleformUI->Value_SetMember( equipmentSlice, "image", "equipment" );
	m_pScaleformUI->Value_SetMember( equipmentSlice, "subMenu", "EquipmentMenu" );
	m_pScaleformUI->Value_SetArrayElement( menuSlices, 4, equipmentSlice );

	SFVALUE grenadeSlice = CreateFlashObject();
	m_pScaleformUI->Value_SetMember( grenadeSlice, "sliceType", "subMenu" );
	m_pScaleformUI->Value_SetMember( grenadeSlice, "name", "#SFUI_BuyMenu_Grenades" );
	m_pScaleformUI->Value_SetMember( grenadeSlice, "image", "grenades" );
	m_pScaleformUI->Value_SetMember( grenadeSlice, "subMenu", "GrenadeMenu" );
	m_pScaleformUI->Value_SetArrayElement( menuSlices, 5, grenadeSlice );

	m_pScaleformUI->Value_SetMember( mainMenu, "Slices", menuSlices );
	m_pScaleformUI->Value_SetMember( rootMenu, "MainMenu", mainMenu );

	// PISTOL MENU
	{
		int pistolCount =
			( loadout.m_WeaponID[LOADOUT_POSITION_SECONDARY0] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SECONDARY1] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SECONDARY2] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SECONDARY3] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SECONDARY4] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SECONDARY5] != -1 );

		SFVALUE pistolMenu = CreateFlashObject();
		SFVALUE pistolSlices = CreateFlashArray( pistolCount );

		m_pScaleformUI->Value_SetMember( pistolMenu, "centerText", "#SFUI_BuyMenu_Pistols" );
		m_pScaleformUI->Value_SetMember( pistolMenu, "menuType", "weapon" );

		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SECONDARY0, pistolSlices, 0 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SECONDARY1, pistolSlices, 1 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SECONDARY2, pistolSlices, 2 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SECONDARY3, pistolSlices, 3 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SECONDARY4, pistolSlices, 4 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SECONDARY5, pistolSlices, 5 );

		m_pScaleformUI->Value_SetMember( pistolMenu, "Slices", pistolSlices );
		m_pScaleformUI->Value_SetMember( rootMenu, "PistolMenu", pistolMenu );
	}

	// SMG MENU
	{
		int smgCount =
			( loadout.m_WeaponID[LOADOUT_POSITION_SMG0] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SMG1] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SMG2] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SMG3] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SMG4] != -1 ) +
			( loadout.m_WeaponID[LOADOUT_POSITION_SMG5] != -1 );

		SFVALUE smgMenu = CreateFlashObject();
		SFVALUE smgSlices = CreateFlashArray( smgCount );

		m_pScaleformUI->Value_SetMember( smgMenu, "centerText", "#SFUI_BuyMenu_SMGs" );
		m_pScaleformUI->Value_SetMember( smgMenu, "menuType", "weapon" );

		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SMG0, smgSlices, 0 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SMG1, smgSlices, 1 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SMG2, smgSlices, 2 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SMG3, smgSlices, 3 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SMG4, smgSlices, 4 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_SMG5, smgSlices, 5 );

		m_pScaleformUI->Value_SetMember( smgMenu, "Slices", smgSlices );
		m_pScaleformUI->Value_SetMember( rootMenu, "SMGMenu", smgMenu );
	}

	// RIFLE MENU
	{

		SFVALUE rifleMenu = CreateFlashObject();
		SFVALUE rifleSlices = CreateFlashArray( 6 );

		m_pScaleformUI->Value_SetMember( rifleMenu, "centerText", "#SFUI_BuyMenu_Rifles" );
		m_pScaleformUI->Value_SetMember( rifleMenu, "menuType", "weapon" );

		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_RIFLE0, rifleSlices, 0 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_RIFLE1, rifleSlices, 1 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_RIFLE2, rifleSlices, 2 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_RIFLE3, rifleSlices, 3 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_RIFLE4, rifleSlices, 4 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_RIFLE5, rifleSlices, 5 );

		m_pScaleformUI->Value_SetMember( rifleMenu, "Slices", rifleSlices );
		m_pScaleformUI->Value_SetMember( rootMenu, "RifleMenu", rifleMenu );
	}

	// HEAVY MENU
	{

		SFVALUE heavyMenu = CreateFlashObject();
		SFVALUE heavySlices = CreateFlashArray( 6 );

		m_pScaleformUI->Value_SetMember( heavyMenu, "centerText", "#SFUI_BuyMenu_HeavyWeapons" );
		m_pScaleformUI->Value_SetMember( heavyMenu, "menuType", "weapon" );

		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_HEAVY0, heavySlices, 0 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_HEAVY1, heavySlices, 1 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_HEAVY2, heavySlices, 2 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_HEAVY3, heavySlices, 3 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_HEAVY4, heavySlices, 4 );
		SetBuyMenuWeaponSliceEntry( loadout, LOADOUT_POSITION_HEAVY5, heavySlices, 5 );

		m_pScaleformUI->Value_SetMember( heavyMenu, "Slices", heavySlices );
		m_pScaleformUI->Value_SetMember( rootMenu, "HeavyMenu", heavyMenu );
	}

	// EQUIPMENT MENU
	{

		SFVALUE equipmentMenu = CreateFlashObject();
		SFVALUE equipmentSlices = CreateFlashArray( 4 );

		m_pScaleformUI->Value_SetMember( equipmentMenu, "centerText", "#SFUI_BuyMenu_Equipment" );
		m_pScaleformUI->Value_SetMember( equipmentMenu, "menuType", "weapon" );
		m_pScaleformUI->Value_SetMember( equipmentMenu, "autoReturn", "false" );

		SFVALUE armorObj = CreateFlashObject();

		bool coop = CSGameRules()->IsPlayingCoopMission();
		m_pScaleformUI->Value_SetMember( armorObj, "sliceType", "equipment" );
		if (coop)
		{
			m_pScaleformUI->Value_SetMember( armorObj, "weapon", "assaultsuit" );
			m_pScaleformUI->Value_SetMember( armorObj, "weapon_position", LOADOUT_POSITION_EQUIPMENT0 );
			m_pScaleformUI->Value_SetArrayElement( equipmentSlices, 0, armorObj );
			SafeReleaseSFVALUE( armorObj );
		}
		else
		{
			m_pScaleformUI->Value_SetMember( armorObj, "weapon", "kevlar" );
			m_pScaleformUI->Value_SetMember( armorObj, "weapon_position", LOADOUT_POSITION_EQUIPMENT0 );
			m_pScaleformUI->Value_SetArrayElement( equipmentSlices, 0, armorObj );
			SafeReleaseSFVALUE( armorObj );
		}

		SFVALUE suitObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember( suitObj, "sliceType", "equipment" );
		const char* sutiName = coop ? "heavyassaultsuit" : "assaultsuit";
		m_pScaleformUI->Value_SetMember( suitObj, "weapon", sutiName );
		m_pScaleformUI->Value_SetMember( suitObj, "weapon_position", LOADOUT_POSITION_EQUIPMENT1 );
		m_pScaleformUI->Value_SetArrayElement( equipmentSlices, 1, suitObj );
		SafeReleaseSFVALUE( suitObj );

		SFVALUE taserObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember( taserObj, "sliceType", "equipment" );
		m_pScaleformUI->Value_SetMember( taserObj, "weapon", "taser" );
		m_pScaleformUI->Value_SetMember( taserObj, "weapon_position", LOADOUT_POSITION_EQUIPMENT2 );
		m_pScaleformUI->Value_SetArrayElement( equipmentSlices, 2, taserObj );
		SafeReleaseSFVALUE( taserObj );

		SFVALUE weaponObj = CreateFlashObject();
		SFVALUE weaponObj2 = CreateFlashObject();
		m_pScaleformUI->Value_SetMember( weaponObj, "sliceType", "equipment" );
		m_pScaleformUI->Value_SetMember( weaponObj2, "ct_hostage", "cutters" );
		m_pScaleformUI->Value_SetMember( weaponObj2, "ct", "defuser" );
		m_pScaleformUI->Value_SetMember( weaponObj2, "t", "null" );
		m_pScaleformUI->Value_SetMember( weaponObj, "weapon", weaponObj2 );
		m_pScaleformUI->Value_SetMember( weaponObj, "weapon_position", LOADOUT_POSITION_EQUIPMENT3 );
		m_pScaleformUI->Value_SetArrayElement( equipmentSlices, 3, weaponObj );
		SafeReleaseSFVALUE( weaponObj2 );
		SafeReleaseSFVALUE( weaponObj );

		m_pScaleformUI->Value_SetMember( equipmentMenu, "Slices", equipmentSlices );
		m_pScaleformUI->Value_SetMember( rootMenu, "EquipmentMenu", equipmentMenu );
		SafeReleaseSFVALUE( equipmentMenu );
		SafeReleaseSFVALUE( equipmentSlices );
	}

	// GRENADE MENU
	{
		bool coop = CSGameRules()->IsPlayingCoopMission();

		SFVALUE grenadeMenu = CreateFlashObject();
		SFVALUE grenadeSlices = CreateFlashArray( 5 );

		m_pScaleformUI->Value_SetMember( grenadeMenu, "centerText", "#SFUI_BuyMenu_Grenades" );
		m_pScaleformUI->Value_SetMember( grenadeMenu, "menuType", "weapon" );
		m_pScaleformUI->Value_SetMember( grenadeMenu, "autoReturn", "false" );

		SFVALUE fireGrenadeObj = CreateFlashObject();
		SFVALUE fireObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember( fireGrenadeObj, "sliceType", "equipment" );
		m_pScaleformUI->Value_SetMember( fireObj, "ct", "Incgrenade" );
		m_pScaleformUI->Value_SetMember( fireObj, "t", "molotov" );
		m_pScaleformUI->Value_SetMember( fireGrenadeObj, "weapon", fireObj );
		m_pScaleformUI->Value_SetMember( fireGrenadeObj, "weapon_position", LOADOUT_POSITION_GRENADE0 );
		m_pScaleformUI->Value_SetArrayElement( grenadeSlices, 0, fireGrenadeObj );
		SafeReleaseSFVALUE( fireObj );
		SafeReleaseSFVALUE( fireGrenadeObj );

		SFVALUE decoyObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember( decoyObj, "sliceType", "equipment" );
		m_pScaleformUI->Value_SetMember( decoyObj, "weapon", "decoy" );
		m_pScaleformUI->Value_SetMember( decoyObj, "weapon_position", LOADOUT_POSITION_GRENADE1 );
		m_pScaleformUI->Value_SetArrayElement( grenadeSlices, 1, decoyObj );
		SafeReleaseSFVALUE( decoyObj );

		SFVALUE flashObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember( flashObj, "sliceType", "equipment" );
		m_pScaleformUI->Value_SetMember( flashObj, "weapon", "flashbang" );
		m_pScaleformUI->Value_SetMember( flashObj, "weapon_position", LOADOUT_POSITION_GRENADE2 );
		m_pScaleformUI->Value_SetArrayElement( grenadeSlices, 2, flashObj );
		SafeReleaseSFVALUE( flashObj );

		SFVALUE heObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember( heObj, "sliceType", "equipment" );
		m_pScaleformUI->Value_SetMember( heObj, "weapon", "hegrenade" );
		m_pScaleformUI->Value_SetMember( heObj, "weapon_position", LOADOUT_POSITION_GRENADE3 );
		m_pScaleformUI->Value_SetArrayElement( grenadeSlices, 3, heObj );
		SafeReleaseSFVALUE( heObj );

		SFVALUE smokeObj = CreateFlashObject();
		m_pScaleformUI->Value_SetMember( smokeObj, "sliceType", "equipment" );
		m_pScaleformUI->Value_SetMember( smokeObj, "weapon", "smokegrenade" );
		m_pScaleformUI->Value_SetMember( smokeObj, "weapon_position", LOADOUT_POSITION_GRENADE4 );
		m_pScaleformUI->Value_SetArrayElement( grenadeSlices, 4, smokeObj );
		SafeReleaseSFVALUE( smokeObj );

		if (coop)
		{
			SFVALUE taObj = CreateFlashObject();
			m_pScaleformUI->Value_SetMember( taObj, "sliceType", "equipment" );
			m_pScaleformUI->Value_SetMember( taObj, "weapon", "tagrenade" );
			m_pScaleformUI->Value_SetMember( taObj, "weapon_position", LOADOUT_POSITION_GRENADE5 );
			m_pScaleformUI->Value_SetArrayElement( grenadeSlices, 5, taObj );
			SafeReleaseSFVALUE( taObj );
		}

		m_pScaleformUI->Value_SetMember( grenadeMenu, "Slices", grenadeSlices );
		m_pScaleformUI->Value_SetMember( rootMenu, "GrenadeMenu", grenadeMenu );
	}

	return rootMenu;
}

bool CCSBuyMenuScaleform::FillInPlayerBuyMenuLoadout( SFBuyMenuLoadout *pLoadout )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return false;

	if ( !pPlayer->IsAlive() )
		return false;

	int team = pPlayer->GetTeamNumber();

	if ( ( team & ~1 ) != TEAM_TERRORIST )
		return false;

	memset( pLoadout, 0, sizeof( *pLoadout ) );

	CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

	for ( int slot = LOADOUT_POSITION_SECONDARY0; slot < LOADOUT_POSITION_HEAVY5; ++slot )
	{
		pLoadout->m_WeaponID[slot] = INVALID_ITEM_ID;
		C_EconItemView *pItem = pInventory->GetItemInLoadoutFilteredByProhibition( team, slot );

		if ( !pItem )
			continue;

		uint64 itemID;

		if ( pItem->GetItemID() != 0 )
			itemID = pItem->GetItemID();
		else
			itemID = pItem->GetFauxItemIDFromDefinitionIndex();

		pLoadout->m_WeaponID[slot] = itemID;
	}

	return true;
}

bool CCSBuyMenuScaleform::FillInPlayerLoadout( SFBuyLoadout &loadout )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *pCSPlayer = GetHudPlayer();
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pPlayer )
		return false;

	if ( !pPlayer->IsAlive() )
		return false;

	Q_memset( &loadout, 0, sizeof(loadout) );

	int equipCount = 0;

	if ( !CSGameRules()->IsArmorFree() && pPlayer->ArmorValue() > 0 )
	{
		if ( CSGameRules()->IsPlayingCoopMission() )
		{
			loadout.m_EquipmentArray[0].m_EquipmentID = pPlayer->HasHeavyArmor() ? ITEM_HEAVYASSAULTSUIT : ITEM_KEVLAR;
			loadout.m_EquipmentArray[0].m_EquipmentPos = 33;
		}
		else
		{
			loadout.m_EquipmentArray[0].m_EquipmentID = pPlayer->HasHelmet() ? ITEM_ASSAULTSUIT : ITEM_KEVLAR;
			loadout.m_EquipmentArray[0].m_EquipmentPos = pPlayer->HasHelmet() ? 33 : 32;
		}

		loadout.m_EquipmentArray[0].m_Quantity = pPlayer->ArmorValue();

		equipCount = 1;

		if ( pPlayer->ArmorValue() == 0 )
		{
			loadout.m_EquipmentArray[0].m_EquipmentID = 0;
			loadout.m_EquipmentArray[0].m_EquipmentPos = 0;
		}
	}

	if ( pPlayer->HasDefuser() )
		loadout.m_flags |= loadout.HAS_DEFUSE;

	for ( int i = 0; i < MAX_WEAPONS; ++i )
	{
		CWeaponCSBase *pWeapon = (CWeaponCSBase*)pCSPlayer->GetWeapon( i );

		if ( !pWeapon )
			continue;

		int weaponID = pWeapon->GetCSWeaponID();

		if ( weaponID == WEAPON_TASER )
		{
			loadout.m_flags |= loadout.HAS_TASER;
			continue;
		}

		const CCSWeaponInfo *pInfo = GetWeaponInfo( pWeapon->GetCSWeaponID() );

		if ( !pInfo )
			continue;

		CSWeaponType type = pInfo->GetWeaponType();

		if ( type == WEAPONTYPE_GRENADE )
		{
			if ( equipCount <= 5 )
			{
				loadout.m_EquipmentArray[equipCount].m_EquipmentID = weaponID;

				switch ( weaponID )
				{
				case WEAPON_FLASHBANG:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 28;
					break;

				case WEAPON_HEGRENADE:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 29;
					break;

				case WEAPON_SMOKEGRENADE:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 30;
					break;

				case WEAPON_MOLOTOV:
				case WEAPON_INCGRENADE:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 26;
					break;

				case WEAPON_DECOY:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 27;
					break;

				default:
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 31;
					break;
				}

				loadout.m_EquipmentArray[equipCount].m_Quantity = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );

				++equipCount;

				if (loadout.m_EquipmentArray[equipCount].m_Quantity <= 0)
				{
					loadout.m_EquipmentArray[equipCount].m_EquipmentID = 0;
					loadout.m_EquipmentArray[equipCount].m_EquipmentPos = 0;
				}
			}

			continue;
		}

		if ( type == WEAPONTYPE_C4 )
		{
			loadout.m_flags |= loadout.HAS_BOMB;
			continue;
		}
	}

	// Determine active loadout selections
	CCSPlayerInventory *pInventory = CSInventoryManager()->GetLocalCSInventory();

	int team = pPlayer->GetTeamNumber();

	for ( int slot = LOADOUT_POSITION_SECONDARY0; slot < LOADOUT_POSITION_COUNT; ++slot )
	{
		C_EconItemView *pLoadoutItem = pInventory->GetItemInLoadoutFilteredByProhibition( team, slot );

		if ( !pLoadoutItem )
			continue;

		uint16 loadoutDefIndex = pLoadoutItem->GetStaticData()->GetDefinitionIndex();

		for ( int weaponIndex = 0; weaponIndex < MAX_WEAPONS; ++weaponIndex )
		{
			CWeaponCSBase *pWeapon = (CWeaponCSBase*)pCSPlayer->GetWeapon( weaponIndex );

			if ( !pWeapon )
				continue;

			C_EconItemView *pWeaponItem = pWeapon->GetEconItemView();

			if ( !pWeaponItem )
				continue;

			if ( pWeaponItem->GetStaticData()->GetDefinitionIndex() != loadoutDefIndex )
			{
				continue;
			}

			int weaponID = pWeapon->GetCSWeaponID();

			const CCSWeaponInfo *pInfo = GetWeaponInfo(pWeapon->GetCSWeaponID());

			if ( pInfo->GetWeaponType() == WEAPONTYPE_PISTOL )
			{
				loadout.m_secondaryWeaponID = weaponID;
				loadout.m_secondaryWeaponItemPos = slot;
			}
			else if ( pInfo->GetWeaponType() != WEAPONTYPE_KNIFE )
			{
				loadout.m_primaryWeaponID = weaponID;
				loadout.m_primaryWeaponItemPos = slot;
			}
		}
	}

	return true;
}

SFVALUE CCSBuyMenuScaleform::CreateFlashLoadout( const SFBuyLoadout &loadout )
{
	SFVALUE flashObj = CreateFlashObject();

	int equipmentCount = 0;

	for ( int i = 0; i < 6; ++i )
	{
		if ( !loadout.m_EquipmentArray[i].m_EquipmentID )
			break;

		if ( !loadout.m_EquipmentArray[i].m_Quantity )
			break;

		++equipmentCount;
	}

	const uint32 totalItems = equipmentCount + ( loadout.m_flags & loadout.HAS_TASER ? 1 : 0 ) + ( loadout.m_primaryWeaponID ? 1 : 0 ) + ( loadout.m_secondaryWeaponID ? 1 : 0 );

	SFVALUE weaponsArray = CreateFlashArray( totalItems );
	SFVALUE slotsArray = CreateFlashArray( totalItems );
	SFVALUE countsArray = CreateFlashArray( totalItems );

	int index = 0;
	int totalPrice = 0;

	// PRIMARY
	if ( loadout.m_primaryWeaponID )
	{
		m_pScaleformUI->Value_SetArrayElement( weaponsArray, index, loadout.m_primaryWeaponID );
		m_pScaleformUI->Value_SetArrayElement( slotsArray, index, loadout.m_primaryWeaponItemPos );
		m_pScaleformUI->Value_SetArrayElement( countsArray, index, 1 );
		m_pScaleformUI->Value_SetMember( flashObj, "primary", index );

		if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
			totalPrice += pPlayer->GetWeaponPrice( static_cast<CSWeaponID>( loadout.m_primaryWeaponID ) );

		++index;
	}

	// SECONDARY
	if ( loadout.m_secondaryWeaponID )
	{
		m_pScaleformUI->Value_SetArrayElement( weaponsArray, index, loadout.m_secondaryWeaponID );
		m_pScaleformUI->Value_SetArrayElement( slotsArray, index, loadout.m_secondaryWeaponItemPos );
		m_pScaleformUI->Value_SetArrayElement( countsArray, index, 1 );
		m_pScaleformUI->Value_SetMember( flashObj, "secondary", index );

		if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
			totalPrice += pPlayer->GetWeaponPrice( static_cast<CSWeaponID>( loadout.m_secondaryWeaponID ) );

		++index;
	}

	// TASER
	if ( loadout.m_flags == loadout.HAS_TASER )
	{
		m_pScaleformUI->Value_SetArrayElement( weaponsArray, index, WEAPON_TASER );
		m_pScaleformUI->Value_SetArrayElement( slotsArray, index, 3 );
		m_pScaleformUI->Value_SetArrayElement( countsArray, index, 1 );
		m_pScaleformUI->Value_SetMember( flashObj, "taser", index );

		if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
			totalPrice += pPlayer->GetWeaponPrice( WEAPON_TASER );

		++index;
	}

	// EQUIPMENT
	for ( int i = 0; i < 6; ++i )
	{
		const SFBuyEquipmentLoadout &slot = loadout.m_EquipmentArray[i];

		if ( !slot.m_EquipmentID || !slot.m_Quantity )
			break;

		m_pScaleformUI->Value_SetArrayElement( weaponsArray, index + i, slot.m_EquipmentID );
		m_pScaleformUI->Value_SetArrayElement( slotsArray, index + i, slot.m_EquipmentPos );
		m_pScaleformUI->Value_SetArrayElement( countsArray, index + i, slot.m_Quantity );

		if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
		{
			totalPrice += slot.m_Quantity * pPlayer->GetWeaponPrice( static_cast<CSWeaponID>( slot.m_EquipmentID ) );
		}

		++index;
	}

	m_pScaleformUI->Value_SetMember( flashObj, "weapons", weaponsArray );
	m_pScaleformUI->Value_SetMember( flashObj, "weapons_position", slotsArray );
	m_pScaleformUI->Value_SetMember( flashObj, "counts", countsArray );
	m_pScaleformUI->Value_SetMember( flashObj, "price", totalPrice );
	m_pScaleformUI->Value_SetMember( flashObj, "bomb", loadout.m_flags == loadout.HAS_BOMB );
	m_pScaleformUI->Value_SetMember( flashObj, "defuse", loadout.m_flags == loadout.HAS_DEFUSE );

	SafeReleaseSFVALUE( weaponsArray );
	SafeReleaseSFVALUE( slotsArray );
	SafeReleaseSFVALUE( countsArray );

	return flashObj;
}

void CCSBuyMenuScaleform::UpdatePlayerLoadout( bool bForceUpdate, bool bUpdateFlash )
{
	if ( !FlashAPIIsValid() )
		return;

	SFBuyLoadout loadout;

	if ( !FillInPlayerLoadout( loadout ) )
		return;

	if ( !bForceUpdate && loadout == m_PlayerLoadout )
		return;

	m_PlayerLoadout = loadout;

	if ( !bUpdateFlash )
		return;

	WITH_SLOT_LOCKED
	{
		WITH_SFVALUEARRAY( args, 1 )
	{
		SFVALUE flashLoadout = CreateFlashLoadout( m_PlayerLoadout );
		m_pScaleformUI->ValueArray_SetElement( args, 0, flashLoadout );
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "updatePlayerInventory", args );
		SafeReleaseSFVALUE( flashLoadout );
	}
	}
}

void CCSBuyMenuScaleform::FireGameEvent( IGameEvent *event )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	const char *pszEvent = event->GetName();

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !V_strcmp( pszEvent, "cs_game_disconnected" ) )
	{
		if ( m_bShowOnReady )
		{
			if ( !m_bVisible )
			{
				GetHud().EnableHud();
			}

			if ( m_bShowOnReady )
			{
				g_pScaleformUI->RemoveElement( m_iSplitScreenSlot, m_FlashAPI );
			}
		}

		return;
	}

	if ( !V_strcmp( pszEvent, "player_team" ) )
	{
		if (pLocalPlayer)
		{
			int team = event->GetInt( "team" );
			int userid = event->GetInt( "userid" );

			if ( userid == pLocalPlayer->GetUserID() )
			{
				SetPlayerIsCT( team == TEAM_CT );
			}
		}

		return;
	}

	if ( !V_strcmp( pszEvent, "player_death" ) )
	{
		if ( pLocalPlayer )
		{
			int userid = event->GetInt( "userid" );

			if ( userid == pLocalPlayer->GetUserID() )
			{
				if ( m_bVisible )
				{
					CSGameRules()->CloseBuyMenu( pLocalPlayer->GetUserID() );
				}
			}
		}

		return;
	}

	if ( !V_strcmp( pszEvent, "item_equip" ) )
	{
		int userid = event->GetInt( "userid" );

		if ( pLocalPlayer )
		{
			if ( userid == pLocalPlayer->GetUserID() )
			{
				if ( m_bVisible )
				{
					UpdatePlayerLoadout( true, true );
				}
			}
		}
	}
}

void CCSBuyMenuScaleform::SetRadialSelection( int radialSlot, int categorySlot )
{
	if ( !m_bVisible )
		return;

	const int newSelection = radialSlot * 6 + categorySlot;

	if ( newSelection == m_iCurrentRadialSelection )
		return;

	if ( !m_bShowOnReady )
		return;

	m_iCurrentRadialSelection = newSelection;

	SFVALUEARRAY args;

	g_pScaleformUI->CreateValueArray( args, 2 );
	g_pScaleformUI->ValueArray_SetElement( args, 0, radialSlot );
	g_pScaleformUI->ValueArray_SetElement( args, 1, categorySlot );

	WITH_SLOT_LOCKED
	{
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setWedgeHighlight", args, 2 );
	}
}

void CCSBuyMenuScaleform::UpdateRadialSelection()
{
	float x = 0.0f;
	float y = 0.0f;

	// Read stick position from Scaleform
	if ( m_bVisible )
	{
		y = inputsystem->GetAnalogValue( (AnalogCode_t)JOYSTICK_AXIS( GET_ACTIVE_SPLITSCREEN_SLOT(), 0 ) );
		x = -inputsystem->GetAnalogValue( (AnalogCode_t)JOYSTICK_AXIS( GET_ACTIVE_SPLITSCREEN_SLOT(), 0 ) );
	}

	// Steam Controller / Steam Input
	if ( steamapicontext && steamapicontext->SteamController() )
	{
		ControllerDigitalActionHandle_t hNavigate = steamapicontext->SteamController()->GetDigitalActionHandle( "Navigate" );

		ControllerHandle_t handles[16];
		int count = steamapicontext->SteamController()->GetConnectedControllers( handles );

		for ( int i = 0; i < count; ++i )
		{
			ControllerAnalogActionData_t data = steamapicontext->SteamController()->GetAnalogActionData( handles[i], hNavigate );

			if ( data.bActive )
			{
				y += data.x;
				x += data.y;
			}
		}
	}

	int wheelSelection = -1;
	int categorySelection = -1;

	// Deadzone
	if ( sqrtf(x * x + y * y) > 0.25f )
	{
		float angle = RAD2DEG( atan2f( y, x ) );

		if ( angle < 0.0f )
			angle += 360.0f;

		// 8-way radial wedge
		wheelSelection = ( static_cast<int>( floorf( angle / 45.0f ) ) + 1 ) & 7;

		// 6 buy categories
		categorySelection = ( static_cast<int>( floorf( angle / 60.0f ) ) + 1 ) % 6;
	}

	SetRadialSelection( wheelSelection, categorySelection );
}

void CCSBuyMenuScaleform::InvalidateWeapon(int weaponID)
{
	CGameUiSetActiveSplitScreenPlayerGuard guard(m_iSplitScreenSlot - 2);

	const char *pszAlias = WeaponIDToAlias(weaponID);

	if (!m_bVisible || !pszAlias)
		return;

	WITH_SLOT_LOCKED
	{
		SFVALUEARRAY args;

		g_pScaleformUI->CreateValueArray(args, 1);
		g_pScaleformUI->ValueArray_SetElement(args, 0, pszAlias);
		g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "InvalidateWeapon", args, 1);
		g_pScaleformUI->ReleaseValueArray(args);
	}
}

bool CCSBuyMenuScaleform::UpdateTimeLeft(bool bForce)
{
	if (!m_bShowOnReady)
		return false;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	float flTimeLeft = 99.0f;

	if (CSGameRules()->IsPlayingCoopMission())
	{
		flTimeLeft = 9999.0f;
	}
	else if (CSGameRules()->IsWarmupPeriod() ||
		!CSGameRules()->IsPlayingCoopGuardian())
	{
		if (pPlayer && pPlayer->CanBuyDuringImmunity())
		{
			flTimeLeft = pPlayer->m_fImmuneToGunGameDamageTime - gpGlobals->curtime;
		}
		else if (CSGameRules()->IsWarmupPeriod() &&
			CSGameRules()->IsWarmupPeriodPaused())
		{
			flTimeLeft = CSGameRules()->GetWarmupPeriodEndTime() - CSGameRules()->GetWarmupPeriodStartTime();
		}
		else
		{
			flTimeLeft = CSGameRules()->GetBuyTimeLength() - CSGameRules()->GetRoundElapsedTime();
		}
	}
	else
	{
		float flEndTime;

		if (CSGameRules()->IsFreezePeriod())
			flEndTime = CSGameRules()->GetRoundStartTime();
		else
			flEndTime = CSGameRules()->GetWarmupPeriodEndTime();

		flTimeLeft = flEndTime - gpGlobals->curtime;

		if (flTimeLeft > 9999.0f)
			return true;
	}

	if (flTimeLeft <= 0.0f)
		return true;

	int nScaledTime = (int)floorf(flTimeLeft * 5.0f);

	if (bForce || nScaledTime != m_nLastTimeLeft)
	{
		m_nLastTimeLeft = nScaledTime;

		// (the restoration created a new array here every update and never
		// released it)
		WITH_SFVALUEARRAY_SLOT_LOCKED(args, 1)
		{
			g_pScaleformUI->ValueArray_SetElement(args, 0, floorf(flTimeLeft));
			g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "setBuyTimeLeft", args, 1);
		}
	}

	return bForce == false && flTimeLeft == 0;
}

void CCSBuyMenuScaleform::UpdatePlayerCash(bool bForce)
{
	if (!m_bShowOnReady)
		return;

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD(m_iSplitScreenSlot);

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	int nAccount = 0;
	if (pPlayer)
		nAccount = pPlayer->GetAccount();

	if (nAccount == m_nLastAccount && !bForce)
		return;

	WITH_SFVALUEARRAY_SLOT_LOCKED(args, 1)
	{
		m_pScaleformUI->ValueArray_SetElement(args, 0, nAccount);
		g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "setPlayerCash", args, 1);
	}

	m_nLastAccount = nAccount;
}

void CCSBuyMenuScaleform::ViewportThink( void )
{
	UpdateRadialSelection();

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if (!pPlayer)
		return;

	// Cache armor/helmet state
	int armorState = pPlayer->ArmorValue();

	if (pPlayer->HasHelmet())
		armorState += 1000;

	if (armorState != m_iCachedArmorState)
	{
		InvalidateWeapon(ITEM_KEVLAR);
		InvalidateWeapon(ITEM_ASSAULTSUIT);
		InvalidateWeapon(ITEM_HEAVYASSAULTSUIT);

		m_iCachedArmorState = armorState;
	}

	// Buy time expired
	if (UpdateTimeLeft(false))
	{
		Hide();

		if (!CSGameRules()->IsPlayingCooperativeGametype())
			PrintBuyTimeOverMessage();

		return;
	}

	// Player left buyzone
	if (!pPlayer->IsInBuyZone())
	{
		Hide();

		SFHudInfoPanel *pInfoPanel = GET_HUDELEMENT(SFHudInfoPanel);

		if (pInfoPanel)
		{
			pInfoPanel->SetPriorityText(g_pVGuiLocalize->Find("#SFUI_BuyMenu_NotInBuyZone"));
		}

		return;
	}

	// Periodic refresh
	float curTime = gpGlobals->curtime;

	if (curTime >= m_flNextLoadoutUpdate)
	{
		m_flNextLoadoutUpdate = curTime + 0.25f;

		UpdatePlayerCash(false);
		UpdatePlayerLoadout(false, true);
	}
}

#endif // INCLUDE_SCALEFORM

//-----------------------------------------------------------------------------
// Methods buy-menu.swf calls that the restoration did not implement
//-----------------------------------------------------------------------------

static ConVar cl_buymenu_closeonbuy( "cl_buymenu_closeonbuy", "0", FCVAR_ARCHIVE, "Close the buy menu after buying something" );

// no 3D weapon preview: returning false makes the movie show the weapon icon
void CCSBuyMenuScaleform::SetShowWeaponModel( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, false );
}

// weapon (or item definition) id -> short name ("ak47"), used for icons
void CCSBuyMenuScaleform::GetWeaponShortNameFromID( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int nID = (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	const char *pszName = WeaponIDToAlias( nID );
	if ( !pszName && GetItemSchema() )
	{
		const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinition( nID );
		if ( pDef )
		{
			pszName = pDef->GetDefinitionName();
			if ( !V_strncmp( pszName, "weapon_", 7 ) || !V_strncmp( pszName, "item_", 5 ) )
				pszName = V_strstr( pszName, "_" ) + 1;
		}
	}
	m_pScaleformUI->Params_SetResult( obj, pszName ? pszName : "" );
}

// loadout position -> localized weapon name
void CCSBuyMenuScaleform::GetWeaponName( SCALEFORM_CALLBACK_ARGS_DECL )
{
	const CCSWeaponInfo *pInfo = GetWeaponInfoForPosition( (int)m_pScaleformUI->Params_GetArgAsNumber( obj, 0 ) );
	if ( !pInfo || !pInfo->szPrintName[0] )
		return;
	const wchar_t *pwszName = g_pVGuiLocalize->Find( pInfo->szPrintName );
	if ( pwszName )
		m_pScaleformUI->Params_SetResult( obj, pwszName );
	else
		m_pScaleformUI->Params_SetResult( obj, pInfo->szPrintName );
}

// how many of this item the player may carry (grenades)
void CCSBuyMenuScaleform::GetCarryLimit( SCALEFORM_CALLBACK_ARGS_DECL )
{
	const char *pszName = m_pScaleformUI->Params_GetArgAsString( obj, 0 );
	CSWeaponID id = AliasToWeaponID( pszName );
	const CCSWeaponInfo *pInfo = id != WEAPON_NONE ? GetWeaponInfo( id ) : NULL;
	m_pScaleformUI->Params_SetResult( obj, pInfo ? MAX( 1, pInfo->GetPrimaryReserveAmmoMax() ) : 1 );
}

void CCSBuyMenuScaleform::ShouldCloseOnBuy( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, cl_buymenu_closeonbuy.GetBool() );
}

// saved loadouts came from the Steam inventory; report empty ones
void CCSBuyMenuScaleform::GetPredefinedLoadout( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SFVALUE result = m_pScaleformUI->CreateNewObject( m_iFlashSlot );
	SFVALUE weapons = m_pScaleformUI->CreateNewArray( m_iFlashSlot, 0 );
	SFVALUE positions = m_pScaleformUI->CreateNewArray( m_iFlashSlot, 0 );
	m_pScaleformUI->Value_SetMember( result, "price", 0 );
	m_pScaleformUI->Value_SetMember( result, "weapons", weapons );
	m_pScaleformUI->Value_SetMember( result, "weapons_position", positions );
	m_pScaleformUI->Value_SetMember( result, "weapons_positions", positions );
	m_pScaleformUI->Params_SetResult( obj, result );
	m_pScaleformUI->ReleaseValue( weapons );
	m_pScaleformUI->ReleaseValue( positions );
	m_pScaleformUI->ReleaseValue( result );
}

void CCSBuyMenuScaleform::BuyLoadout( SCALEFORM_CALLBACK_ARGS_DECL )
{
}

void CCSBuyMenuScaleform::ReportLoadouts( SCALEFORM_CALLBACK_ARGS_DECL )
{
	GetPredefinedLoadout( pui, obj );
}

void CCSBuyMenuScaleform::GetLocalPlayerXuid( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, "0" );
}
