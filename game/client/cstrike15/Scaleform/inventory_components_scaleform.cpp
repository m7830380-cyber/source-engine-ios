//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The _global.CScaleformComponent_* objects the main menu's inventory,
//          loadout and item tile movies (inventorypanel.swf, loadoutpanel.swf,
//          itemtilesmall.swf, inventorypanelmaster.swf, mainmenu.swf) call.
//          Valve's implementations were removed from the leaked source, so
//          every call returned undefined and the screens were empty.
//
//          Inventory/Loadout/MyPersona answer from the local CCSPlayerInventory
//          (filled by the offline inventory with -allskinsunlocked, otherwise
//          just the base items). The store, market, stickers, missions and
//          social parts have no backend here and return empty defaults.
//
//          Item IDs cross into ActionScript as decimal strings. Base items use
//          the combined def/paint IDs (CombinedItemIdMakeFromDefIndexAndPaint).
//
//===========================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#include "cstrike15_item_inventory.h"
#include "cstrike15_item_schema.h"
#include "econ_item_view_helpers.h"
#include "econ_item_constants.h"
#include "cs_shareddefs.h"
#include "tier1/fmtstr.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "ienginevgui.h"
#include "flash_item_model_panel.h"
#include "createmainmenuscreen_scaleform.h"
#include "ios_lan.h"
#include "offline_inventory.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Shared helpers
//-----------------------------------------------------------------------------
static const char *ArgString( IUIMarshalHelper *pui, SFPARAMS obj, int i )
{
	if ( (int)pui->Params_GetNumArgs( obj ) <= i )
		return "";
	IUIMarshalHelper::_ValueType t = pui->Params_GetArgType( obj, i );
	if ( t == IUIMarshalHelper::VT_String || t == IUIMarshalHelper::VT_StringW )
	{
		const char *psz = pui->Params_GetArgAsString( obj, i );
		return psz ? psz : "";
	}
	return "";
}

static int ArgInt( IUIMarshalHelper *pui, SFPARAMS obj, int i )
{
	if ( (int)pui->Params_GetNumArgs( obj ) <= i )
		return 0;
	IUIMarshalHelper::_ValueType t = pui->Params_GetArgType( obj, i );
	if ( t == IUIMarshalHelper::VT_String || t == IUIMarshalHelper::VT_StringW )
		return V_atoi( ArgString( pui, obj, i ) );
	if ( t == IUIMarshalHelper::VT_Boolean )
		return pui->Params_GetArgAsBool( obj, i ) ? 1 : 0;
	if ( t == IUIMarshalHelper::VT_Undefined || t == IUIMarshalHelper::VT_Null )
		return 0;
	return (int)pui->Params_GetArgAsNumber( obj, i );
}

static uint64 ArgItemID( IUIMarshalHelper *pui, SFPARAMS obj, int i )
{
	if ( (int)pui->Params_GetNumArgs( obj ) <= i )
		return 0;
	IUIMarshalHelper::_ValueType t = pui->Params_GetArgType( obj, i );
	if ( t == IUIMarshalHelper::VT_String || t == IUIMarshalHelper::VT_StringW )
		return V_atoui64( ArgString( pui, obj, i ) );
	if ( t == IUIMarshalHelper::VT_Undefined || t == IUIMarshalHelper::VT_Null )
		return 0;
	return (uint64)pui->Params_GetArgAsNumber( obj, i );
}

static void ResultItemID( IUIMarshalHelper *pui, SFPARAMS obj, uint64 ullID )
{
	pui->Params_SetResult( obj, CFmtStr( "%llu", ullID ).Access() );
}

static int TeamFromString( const char *psz )
{
	if ( !V_stricmp( psz, "ct" ) || !V_stricmp( psz, "counter-terrorists" ) )
		return TEAM_CT;
	if ( !V_stricmp( psz, "t" ) || !V_stricmp( psz, "terrorists" ) )
		return TEAM_TERRORIST;
	return 0;	// "noteam"
}

static int SlotFromString( const char *psz )
{
	if ( !psz || !psz[0] )
		return -1;
	const CUtlVector< const char * > &vecSlots = GetItemSchema()->GetLoadoutStringsSubPositions();
	FOR_EACH_VEC( vecSlots, i )
	{
		if ( vecSlots[i] && !V_stricmp( vecSlots[i], psz ) )
			return i;
	}
	// a category name ("melee", "c4") doubles as its only slot
	const CUtlVector< const char * > &vecCategories = GetItemSchema()->GetLoadoutStrings();
	FOR_EACH_VEC( vecCategories, i )
	{
		if ( vecCategories[i] && !V_stricmp( vecCategories[i], psz ) )
			return i;
	}
	return -1;
}

static CCSPlayerInventory *LocalInventory()
{
	return CSInventoryManager() ? CSInventoryManager()->GetLocalCSInventory() : NULL;
}

// UTF-8 by hand: V_UnicodeToUTF8 is wcstombs on POSIX, which fails in the C
// locale on iOS for anything non-ASCII (and then leaves nothing)
static void WideToUTF8( const wchar_t *pwsz, char *pszOut, int nOutBytes )
{
	int n = 0;
	for ( ; *pwsz; pwsz++ )
	{
		uint32 c = (uint32)*pwsz;
		char buf[4];
		int nLen;
		if ( c < 0x80 )			{ buf[0] = (char)c; nLen = 1; }
		else if ( c < 0x800 )	{ buf[0] = (char)( 0xC0 | ( c >> 6 ) ); buf[1] = (char)( 0x80 | ( c & 0x3F ) ); nLen = 2; }
		else if ( c < 0x10000 )	{ buf[0] = (char)( 0xE0 | ( c >> 12 ) ); buf[1] = (char)( 0x80 | ( ( c >> 6 ) & 0x3F ) ); buf[2] = (char)( 0x80 | ( c & 0x3F ) ); nLen = 3; }
		else					{ buf[0] = (char)( 0xF0 | ( c >> 18 ) ); buf[1] = (char)( 0x80 | ( ( c >> 12 ) & 0x3F ) ); buf[2] = (char)( 0x80 | ( ( c >> 6 ) & 0x3F ) ); buf[3] = (char)( 0x80 | ( c & 0x3F ) ); nLen = 4; }
		if ( n + nLen >= nOutBytes )
			break;
		for ( int i = 0; i < nLen; i++ )
			pszOut[ n++ ] = buf[i];
	}
	pszOut[ n ] = '\0';
}

static uint64 GetLocalXuid()
{
	return ( steamapicontext && steamapicontext->SteamUser() ) ? steamapicontext->SteamUser()->GetSteamID().ConvertToUint64() : 0;
}

// Real inventory item or a base item's combined def/paint ID
static CEconItemView *FindItem( uint64 ullID )
{
	if ( !ullID )
		return NULL;
	if ( CombinedItemIdIsDefIndexAndPaint( ullID ) )
		return InventoryManager()->FindOrCreateReferenceEconItem( ullID );
	CCSPlayerInventory *pInv = LocalInventory();
	return pInv ? pInv->GetInventoryItemByItemID( ullID ) : NULL;
}

static uint64 ItemIDForView( CEconItemView *pItem )
{
	if ( !pItem || !pItem->IsValid() )
		return 0;
	if ( pItem->GetItemID() )
		return pItem->GetItemID();
	return CombinedItemIdMakeFromDefIndexAndPaint( pItem->GetItemDefinition()->GetDefinitionIndex(), 0 );
}

static const CCStrike15ItemDefinition *ItemDef( CEconItemView *pItem )
{
	return pItem && pItem->IsValid() ? dynamic_cast< const CCStrike15ItemDefinition * >( pItem->GetItemDefinition() ) : NULL;
}

// Loadout position the item goes in for a team (or its default one)
static int ItemSlot( CEconItemView *pItem, int iTeam )
{
	const CCStrike15ItemDefinition *pDef = ItemDef( pItem );
	if ( !pDef )
		return -1;
	if ( iTeam == TEAM_TERRORIST || iTeam == TEAM_CT )
	{
		int nSlot = pDef->GetLoadoutSlot( iTeam );
		if ( nSlot >= 0 )
			return nSlot;
	}
	int nSlot = pDef->GetDefaultLoadoutSlot();
	if ( nSlot < 0 && pDef->CanBeUsedByTeam( TEAM_CT ) )
		nSlot = pDef->GetLoadoutSlot( TEAM_CT );
	if ( nSlot < 0 && pDef->CanBeUsedByTeam( TEAM_TERRORIST ) )
		nSlot = pDef->GetLoadoutSlot( TEAM_TERRORIST );
	return nSlot;
}

// items shown with a rendered icon: painted weapons, knives and gloves
static bool IsRenderedIconItem( CEconItemView *pItem )
{
	if ( !pItem || !pItem->IsValid() || !pItem->GetSOCData() || pItem->GetCustomPaintKitIndex() <= 0 )
		return false;
	return pItem->CanGenerateInventoryImageRgba();
}

static uint64 LoadoutItemID( int iTeam, int iSlot )
{
	if ( iSlot < 0 || iSlot >= LOADOUT_POSITION_COUNT )
		return 0;
	return ItemIDForView( CSInventoryManager()->GetItemInLoadoutForTeam( iTeam, iSlot ) );
}

static bool IsItemEquipped( CEconItemView *pItem, int iTeam )
{
	uint64 ullID = ItemIDForView( pItem );
	if ( !ullID )
		return false;
	if ( iTeam == 0 )
	{
		// only team-less items (flair, music kits, sprays) equip in the noteam loadout
		const CCStrike15ItemDefinition *pDef = ItemDef( pItem );
		if ( !pDef || pDef->CanBeUsedByTeam( TEAM_CT ) || pDef->CanBeUsedByTeam( TEAM_TERRORIST ) )
			return false;
		int nSlot = pDef ? pDef->GetDefaultLoadoutSlot() : -1;
		return nSlot >= 0 && LoadoutItemID( 0, nSlot ) == ullID;
	}
	const CCStrike15ItemDefinition *pDef = ItemDef( pItem );
	if ( !pDef || !pDef->CanBeUsedByTeam( iTeam ) )
		return false;
	return LoadoutItemID( iTeam, ItemSlot( pItem, iTeam ) ) == ullID;
}

static const char *RarityHexColor( int nRarity )
{
	const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinition( nRarity );
	if ( pRarity )
	{
		const CEconColorDefinition *pColor = GetItemSchema()->GetColorDefinitionByName( GetColorNameForAttribColor( pRarity->GetAttribColor() ) );
		if ( pColor && pColor->GetHexColor() && pColor->GetHexColor()[0] )
			return pColor->GetHexColor();
	}
	return "#b0c3d9";
}

//-----------------------------------------------------------------------------
// CScaleformComponent_Inventory
//-----------------------------------------------------------------------------
static const char *s_pszSortMethods[] = { "newest", "oldest", "alphaascend", "alphadescend", "mostrare", "leastrare", "equipped", "slot", "collection", "unsorted" };

//-----------------------------------------------------------------------------
// Inventory "Inspect": the preview dialog (tooltips.swf) shows the name and
// rarity and calls LaunchWeaponPreviewPanel( path ) for the 3D model, which the
// client drew. A CFlashItemModelPanel draws it over the middle of the dialog
// (MainMenu.swf's Panel.TooltipItemPreview), clear of its title and buttons.
// Drag a finger over it to turn the model.
//-----------------------------------------------------------------------------
static int GetInspectModelPlacement( int &x, int &y, int &w, int &h )
{
	CCreateMainMenuScreenScaleform *pMenu = CCreateMainMenuScreenScaleform::GetInstance();
	if ( !pMenu || !pMenu->FlashAPIIsValid() || !g_pScaleformUI )
		return 0;
	SFVALUE panel = g_pScaleformUI->Value_GetMember( pMenu->m_FlashAPI, "Panel" );
	if ( !panel )
		return 0;
	SFVALUE dialog = g_pScaleformUI->Value_GetMember( panel, "TooltipItemPreview" );
	float x0, y0, x1, y1;
	bool bOK = dialog && GetFlashClipScreenRect( dialog, SF_FULL_SCREEN_SLOT, x0, y0, x1, y1 );
	if ( dialog )
		g_pScaleformUI->ReleaseValue( dialog );
	g_pScaleformUI->ReleaseValue( panel );
	if ( !bOK )
		return 0;

	float dw = x1 - x0, dh = y1 - y0;
	x = (int)( x0 + dw * 0.06f );
	w = (int)( dw * 0.88f );
	y = (int)( y0 + dh * 0.16f );
	h = (int)( dh * 0.62f );

	static bool s_bLogged = false;
	if ( !s_bLogged )
	{
		VERBOSE_PRINTF( "[inspect] dialog %.0f,%.0f - %.0f,%.0f -> panel %d,%d %dx%d\n", x0, y0, x1, y1, x, y, w, h );
		fflush( stdout );
		s_bLogged = true;
	}
	return 1;
}

static CFlashItemModelPanel *s_pInspectPanel = NULL;
static uint64 s_ullInspectItemID = 0;

class CScaleformComponentInventory : public ScaleformUIFunctionHandlerObject
{
public:
	CUtlVector< uint64 > m_vecList;			// result of the last SetInventorySortAndFilters
	CUtlString m_strSavedFilter;
	CUtlString m_strSavedSort;

	CScaleformComponentInventory() : m_strSavedFilter( "all" ), m_strSavedSort( "newest" ) {}

	struct Filter_t
	{
		int m_iTeam;			// 0 = any
		int m_nSlot;			// -1 = any
		CUtlVector< CUtlString > m_vecCategories;
		bool m_bIncludeBaseItems;
		bool m_bNotBaseItem;
		bool m_bNotDefaultEquipped;
		bool m_bEquippedOnly;
		bool m_bMatchNothing;
	};

	static bool IsCategoryToken( const char *psz )
	{
		static const char *s_pszCategories[] = { "all", "only_weapons", "heavy", "secondary", "rifle", "smg", "melee", "clothing", "gloves", "grenade", "equipment" };
		for ( int i = 0; i < ARRAYSIZE( s_pszCategories ); i++ )
			if ( !V_stricmp( psz, s_pszCategories[i] ) )
				return true;
		return false;
	}

	static void ParseFilter( const char *pszFilter, Filter_t &f )
	{
		f.m_iTeam = 0;
		f.m_nSlot = -1;
		f.m_bIncludeBaseItems = f.m_bNotBaseItem = f.m_bNotDefaultEquipped = f.m_bEquippedOnly = f.m_bMatchNothing = false;

		CUtlStringList vecTokens;
		V_SplitString( pszFilter, ",", vecTokens );
		FOR_EACH_VEC( vecTokens, i )
		{
			const char *psz = vecTokens[i];
			if ( !psz[0] )
				continue;
			if ( !V_stricmp( psz, "t" ) || !V_stricmp( psz, "ct" ) || !V_stricmp( psz, "noteam" ) )
				f.m_iTeam = TeamFromString( psz );
			else if ( !V_stricmp( psz, "any_equipment" ) )
				f.m_bIncludeBaseItems = true;
			else if ( !V_stricmp( psz, "not_base_item" ) )
				f.m_bNotBaseItem = true;
			else if ( !V_stricmp( psz, "not_defaultequipped" ) )
				f.m_bNotDefaultEquipped = true;
			else if ( !V_stricmp( psz, "equipped" ) )
				f.m_bEquippedOnly = true;
			else if ( IsCategoryToken( psz ) )
				f.m_vecCategories.AddToTail( psz );
			else if ( SlotFromString( psz ) >= 0 && V_strlen( psz ) > 1 && V_isdigit( psz[ V_strlen( psz ) - 1 ] ) )
				f.m_nSlot = SlotFromString( psz );
			else
				f.m_bMatchNothing = true;	// stickers, sprays, music kits, flair, tools: none offline
		}
	}

	static bool CategoryMatches( const char *pszCategory, int nSlot )
	{
		if ( !V_stricmp( pszCategory, "all" ) )
			return true;
		if ( nSlot < 0 || nSlot >= LOADOUT_POSITION_COUNT )
			return false;
		const char *pszItemCategory = GetItemSchema()->GetLoadoutStrings()[ nSlot ];
		if ( !pszItemCategory )
			return false;
		if ( !V_stricmp( pszCategory, "only_weapons" ) )
			return !V_stricmp( pszItemCategory, "melee" ) || !V_stricmp( pszItemCategory, "secondary" ) || !V_stricmp( pszItemCategory, "smg" ) || !V_stricmp( pszItemCategory, "rifle" ) || !V_stricmp( pszItemCategory, "heavy" );
		if ( !V_stricmp( pszCategory, "gloves" ) )
			return nSlot == LOADOUT_POSITION_CLOTHING_HANDS;
		return !V_stricmp( pszCategory, pszItemCategory );
	}

	static bool ItemMatches( CEconItemView *pItem, const Filter_t &f, const char *pszText )
	{
		const CCStrike15ItemDefinition *pDef = ItemDef( pItem );
		if ( !pDef )
			return false;
		if ( f.m_iTeam && !pDef->CanBeUsedByTeam( f.m_iTeam ) )
			return false;
		int nSlot = ItemSlot( pItem, f.m_iTeam );
		if ( f.m_nSlot >= 0 && nSlot != f.m_nSlot )
			return false;
		if ( f.m_vecCategories.Count() )
		{
			bool bAny = false;
			FOR_EACH_VEC( f.m_vecCategories, i )
				bAny |= CategoryMatches( f.m_vecCategories[i], nSlot );
			if ( !bAny )
				return false;
		}
		if ( f.m_bEquippedOnly && !IsItemEquipped( pItem, TEAM_CT ) && !IsItemEquipped( pItem, TEAM_TERRORIST ) )
			return false;
		if ( pszText && pszText[0] )
		{
			char szName[256];
			const wchar_t *pwszName = pItem->GetItemName();
			WideToUTF8( pwszName ? pwszName : L"", szName, sizeof( szName ) );
			if ( !V_stristr( szName, pszText ) && !V_stristr( pDef->GetDefinitionName(), pszText ) )
				return false;
		}
		return true;
	}

	static int SortRank( const char *pszSort, CEconItemView *pItem )
	{
		if ( !V_stricmp( pszSort, "mostrare" ) || !V_stricmp( pszSort, "leastrare" ) )
			return pItem->GetRarity();
		if ( !V_stricmp( pszSort, "equipped" ) )
			return ( IsItemEquipped( pItem, TEAM_CT ) || IsItemEquipped( pItem, TEAM_TERRORIST ) ) ? 1 : 0;
		if ( !V_stricmp( pszSort, "slot" ) )
			return -ItemSlot( pItem, 0 );
		return 0;
	}

	struct SortEntry_t { uint64 m_ullID; int m_nRank; int m_nOrder; char m_szName[128]; };
	static const char *s_pszSortForCompare;
	static int CompareEntries( const SortEntry_t *a, const SortEntry_t *b )
	{
		const char *pszSort = s_pszSortForCompare;
		if ( !V_stricmp( pszSort, "alphaascend" ) || !V_stricmp( pszSort, "alphadescend" ) )
		{
			int n = V_stricmp( a->m_szName, b->m_szName );
			if ( n )
				return !V_stricmp( pszSort, "alphaascend" ) ? n : -n;
		}
		else if ( a->m_nRank != b->m_nRank )
		{
			bool bAscending = !V_stricmp( pszSort, "leastrare" );
			return bAscending ? ( a->m_nRank - b->m_nRank ) : ( b->m_nRank - a->m_nRank );
		}
		// "newest" first by default; "oldest" keeps inventory order
		int nOrder = a->m_nOrder - b->m_nOrder;
		return !V_stricmp( pszSort, "oldest" ) ? nOrder : -nOrder;
	}

	// SetInventorySortAndFilters( xuid, sort, reverse, filter, savedSettings, customText )
	void SetInventorySortAndFilters( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const char *pszSort = ArgString( pui, obj, 1 );
		bool bReverse = ArgInt( pui, obj, 2 ) != 0;
		const char *pszFilter = ArgString( pui, obj, 3 );
		const char *pszSaved = ArgString( pui, obj, 4 );
		const char *pszText = ArgString( pui, obj, 5 );
		if ( pszSaved[0] )
			m_strSavedFilter = pszSaved;
		if ( pszSort[0] )
			m_strSavedSort = pszSort;

		Filter_t f;
		ParseFilter( pszFilter, f );

		CUtlVector< SortEntry_t > vecEntries;
		CCSPlayerInventory *pInv = LocalInventory();
		if ( pInv && !f.m_bMatchNothing )
		{
			for ( int i = 0; i < pInv->GetItemCount(); i++ )
			{
				CEconItemView *pItem = pInv->GetItem( i );
				if ( !pItem || !pItem->IsValid() || !ItemMatches( pItem, f, pszText ) )
					continue;
				SortEntry_t &e = vecEntries[ vecEntries.AddToTail() ];
				e.m_ullID = pItem->GetItemID();
				e.m_nOrder = i;
				e.m_nRank = SortRank( pszSort, pItem );
				WideToUTF8( pItem->GetItemName() ? pItem->GetItemName() : L"", e.m_szName, sizeof( e.m_szName ) );
			}

			// The loadout lists the plain versions of the weapons that fit the slot too,
			// so a default (e.g. USP-S vs P2000) can be chosen
			if ( f.m_bIncludeBaseItems && !f.m_bNotBaseItem && f.m_nSlot >= 0 )
			{
				const CEconItemSchema::ItemDefinitionMap_t &mapDefs = GetItemSchema()->GetItemDefinitionMap();
				FOR_EACH_MAP_FAST( mapDefs, i )
				{
					const CCStrike15ItemDefinition *pDef = dynamic_cast< const CCStrike15ItemDefinition * >( mapDefs[i] );
					if ( !pDef || !pDef->IsBaseItem() )
						continue;
					uint64 ullID = CombinedItemIdMakeFromDefIndexAndPaint( pDef->GetDefinitionIndex(), 0 );
					CEconItemView *pItem = FindItem( ullID );
					if ( !pItem || !ItemMatches( pItem, f, pszText ) )
						continue;
					SortEntry_t &e = vecEntries[ vecEntries.AddToTail() ];
					e.m_ullID = ullID;
					e.m_nOrder = pInv->GetItemCount() + 100000;	// defaults first under "newest"
					e.m_nRank = SortRank( pszSort, pItem );
					WideToUTF8( pItem->GetItemName() ? pItem->GetItemName() : L"", e.m_szName, sizeof( e.m_szName ) );
				}
			}
		}

		if ( V_stricmp( pszSort, "unsorted" ) && V_stricmp( pszSort, "collection" ) )
		{
			s_pszSortForCompare = pszSort;
			vecEntries.Sort( CompareEntries );
		}

		m_vecList.RemoveAll();
		FOR_EACH_VEC( vecEntries, i )
			m_vecList.AddToTail( vecEntries[i].m_ullID );
		if ( bReverse )
		{
			for ( int i = 0, j = m_vecList.Count() - 1; i < j; i++, j-- )
			{
				uint64 ullTmp = m_vecList[i];
				m_vecList[i] = m_vecList[j];
				m_vecList[j] = ullTmp;
			}
		}
	}

	void GetInventoryCount( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, m_vecList.Count() ); }
	void GetInventoryItemIDByIndex( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		int i = ArgInt( pui, obj, 0 );
		if ( m_vecList.IsValidIndex( i ) )
			ResultItemID( pui, obj, m_vecList[i] );
		else
			pui->Params_SetResult( obj, "" );
	}

	void GetSortMethodsCount( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, (int)ARRAYSIZE( s_pszSortMethods ) ); }
	void GetSortMethodByIndex( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		int i = ArgInt( pui, obj, 0 );
		pui->Params_SetResult( obj, ( i >= 0 && i < ARRAYSIZE( s_pszSortMethods ) ) ? s_pszSortMethods[i] : "" );
	}
	void GetSavedFilterMethod( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, m_strSavedFilter.Get() ); }
	void GetSavedSortMethod( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, m_strSavedSort.Get() ); }

	// ( xuid, itemid ) item queries
	void GetItemName( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		uint64 ullID = ArgItemID( pui, obj, 1 );
		CEconItemView *pItem = FindItem( ullID );
		const wchar_t *pwsz = pItem ? pItem->GetItemName() : NULL;
		// diagnostics: knife names
		static int s_nKnifeNameLogs = 0;
		const CCStrike15ItemDefinition *pDef = ItemDef( pItem );
		if ( pDef && pDef->GetDefaultLoadoutSlot() == LOADOUT_POSITION_MELEE && s_nKnifeNameLogs < 40 )
		{
			s_nKnifeNameLogs++;
			// ASCII as-is, anything else as \uXXXX, so the log shows exactly what the menu gets
			char szName[512];
			int nOut = 0;
			for ( const wchar_t *pw = pwsz ? pwsz : L""; *pw && nOut < (int)sizeof( szName ) - 12; pw++ )
			{
				if ( *pw >= 32 && *pw < 127 )
					szName[ nOut++ ] = (char)*pw;
				else
					nOut += V_snprintf( szName + nOut, sizeof( szName ) - nOut, "\\u%04X", (unsigned)*pw );
			}
			szName[ nOut ] = '\0';
			VERBOSE_PRINTF( "[offline] name arg '%s' id %llu: def %d %s, paint %d, quality %d, base '%s' -> '%s'\n",
				ArgString( pui, obj, 1 ), ullID, pDef->GetDefinitionIndex(), pDef->GetDefinitionName(), pItem->GetCustomPaintKitIndex(),
				pItem->GetQuality(), pDef->GetItemBaseName(), szName );
			fflush( stdout );
		}
		// UTF-8, Scaleform's own string format: the wide-string path garbled names
		// with non-ASCII characters (the star on knives came out as boxes followed by
		// leftover text from other strings)
		char szUTF8[512];
		WideToUTF8( pwsz ? pwsz : L"", szUTF8, sizeof( szUTF8 ) );
		pui->Params_SetResult( obj, szUTF8 );
	}
	void GetItemRarityColor( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		CEconItemView *pItem = FindItem( ArgItemID( pui, obj, 1 ) );
		pui->Params_SetResult( obj, RarityHexColor( pItem ? pItem->GetRarity() : 0 ) );
	}
	void GetItemRarity( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		CEconItemView *pItem = FindItem( ArgItemID( pui, obj, 1 ) );
		pui->Params_SetResult( obj, pItem ? pItem->GetRarity() : 0 );
	}
	void GetSlot( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		int nSlot = ItemSlot( FindItem( ArgItemID( pui, obj, 1 ) ), 0 );
		pui->Params_SetResult( obj, ( nSlot >= 0 && nSlot < LOADOUT_POSITION_COUNT ) ? GetItemSchema()->GetLoadoutStrings()[ nSlot ] : "" );
	}
	void GetSlotSubPosition( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		int nSlot = ItemSlot( FindItem( ArgItemID( pui, obj, 1 ) ), 0 );
		pui->Params_SetResult( obj, ( nSlot >= 0 && nSlot < LOADOUT_POSITION_COUNT ) ? GetItemSchema()->GetLoadoutStringsSubPositions()[ nSlot ] : "" );
	}
	void GetItemTeam( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const CCStrike15ItemDefinition *pDef = ItemDef( FindItem( ArgItemID( pui, obj, 1 ) ) );
		const char *psz = "#CSGO_Inventory_Team_Any";
		if ( pDef )
		{
			bool bCT = pDef->CanBeUsedByTeam( TEAM_CT ), bT = pDef->CanBeUsedByTeam( TEAM_TERRORIST );
			if ( bCT && !bT )
				psz = "#CSGO_Inventory_Team_CT";
			else if ( bT && !bCT )
				psz = "#CSGO_Inventory_Team_T";
		}
		pui->Params_SetResult( obj, psz );
	}
	void IsEquipped( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		CEconItemView *pItem = FindItem( ArgItemID( pui, obj, 1 ) );
		pui->Params_SetResult( obj, pItem ? IsItemEquipped( pItem, TeamFromString( ArgString( pui, obj, 2 ) ) ) : false );
	}
	// Skins get icons rendered by the game (the weapon model with its paint kit,
	// CEconItemView::GetInventoryImageRgba -> img://inventory_<id>); plain items
	// keep their shipped PNG from GetItemInventoryImage.
	void IsInventoryImageCachable( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		CEconItemView *pItem = FindItem( ArgItemID( pui, obj, 1 ) );
		pui->Params_SetResult( obj, IsRenderedIconItem( pItem ) );
	}
	// ---- cases (offline_inventory.cpp): opened without a key ----------------
	void IsItemUnusual( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const CCStrike15ItemDefinition *pDef = ItemDef( FindItem( ArgItemID( pui, obj, 1 ) ) );
		pui->Params_SetResult( obj, pDef && ( pDef->GetDefaultLoadoutSlot() == LOADOUT_POSITION_MELEE || pDef->GetDefaultLoadoutSlot() == LOADOUT_POSITION_CLOTHING_HANDS ) );
	}
	void GetItemCapabilitiesCount( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		pui->Params_SetResult( obj, OfflineCase_IsCase( ArgItemID( pui, obj, 1 ) ) ? 1 : 0 );
	}
	void GetItemCapabilityByIndex( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		bool bCase = OfflineCase_IsCase( ArgItemID( pui, obj, 1 ) ) && pui->Params_GetArgAsNumber( obj, 2 ) == 0.0;
		pui->Params_SetResult( obj, bCase ? "decodable" : "" );
	}
	// the contents, then "0" for the exceedingly rare (star) slot
	void GetLootListItemsCount( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		CUtlVector< uint64 > vecItems;
		bool bRare;
		OfflineCase_GetContents( ArgItemID( pui, obj, 1 ), vecItems, bRare );
		pui->Params_SetResult( obj, vecItems.Count() + ( bRare ? 1 : 0 ) );
	}
	void GetLootListItemIdByIndex( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		CUtlVector< uint64 > vecItems;
		bool bRare;
		OfflineCase_GetContents( ArgItemID( pui, obj, 1 ), vecItems, bRare );
		int i = (int)pui->Params_GetArgAsNumber( obj, 2 );
		char szID[32] = "0";
		if ( i >= 0 && i < vecItems.Count() )
			V_snprintf( szID, sizeof( szID ), "%llu", vecItems[i] );
		pui->Params_SetResult( obj, szID );
	}
	// the case's own "exceedingly rare item" picture and name (weapon_case_base's by default)
	void GetLootListUnusualItemImage( SCALEFORM_CALLBACK_ARGS_DECL ) { CaseRawString( pui, obj, "image_unusual_item", "econ/weapon_cases/default_rare_item" ); }
	void GetLootListUnusualItemName( SCALEFORM_CALLBACK_ARGS_DECL ) { CaseRawString( pui, obj, "loot_list_rare_item_name", "#Exceedingly_Rare_Item" ); }
	void CaseRawString( SCALEFORM_CALLBACK_ARGS_DECL, const char *pszKey, const char *pszDefault )
	{
		const CCStrike15ItemDefinition *pDef = ItemDef( FindItem( ArgItemID( pui, obj, 1 ) ) );
		KeyValues *pKV = pDef ? pDef->GetRawDefinition() : NULL;
		const char *psz = pKV ? pKV->GetString( pszKey, "" ) : "";
		pui->Params_SetResult( obj, psz[0] ? psz : pszDefault );
	}
	// UseTool( xuid, tool, item ): a keyless case open; the panel's reel stops on
	// the item the CrateOpened event names (mainmenu.swf -> SetItemThatCameFromOpeningCrate)
	void UseTool( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		uint64 ullCase = ArgItemID( pui, obj, 2 );
		uint64 ullItem = OfflineCase_Open( ullCase );
		if ( !ullItem )
			return;
		CEconItemView *pWon = FindItem( ullItem );
		Msg( "[case] opened %llu: %s\n", ullCase, pWon && pWon->GetItemDefinition() ? pWon->GetItemDefinition()->GetDefinitionName() : "?" );
		if ( CCreateMainMenuScreenScaleform::IsActive() )
		{
			KeyValues *pEvent = new KeyValues( "ScaleformComponent_Inventory_CrateOpened" );
			pEvent->SetString( "itemid", CFmtStr( "%llu", ullItem ) );
			CCreateMainMenuScreenScaleform::GetInstance()->OnEvent( pEvent );
			pEvent->deleteThis();
		}
	}

	void GetItemInventoryImage( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		// per-skin icons came from Steam's CDN; show the weapon's own icon
		const CCStrike15ItemDefinition *pDef = ItemDef( FindItem( ArgItemID( pui, obj, 1 ) ) );
		const char *psz = pDef ? pDef->GetInventoryImage() : NULL;
		pui->Params_SetResult( obj, psz ? psz : "" );
	}
	void IsItemInfoValid( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, FindItem( ArgItemID( pui, obj, 1 ) ) != NULL ); }
	void IsItemDefault( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, CombinedItemIdIsDefIndexAndPaint( ArgItemID( pui, obj, 1 ) ) ); }
	void GetItemDefinitionIndex( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const CCStrike15ItemDefinition *pDef = ItemDef( FindItem( ArgItemID( pui, obj, 1 ) ) );
		pui->Params_SetResult( obj, pDef ? (int)pDef->GetDefinitionIndex() : 0 );
	}
	void GetItemDefinitionName( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const CCStrike15ItemDefinition *pDef = ItemDef( FindItem( ArgItemID( pui, obj, 0 ) ) );
		pui->Params_SetResult( obj, pDef ? pDef->GetDefinitionName() : "" );
	}
	void DoesItemMatchDefinitionByName( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const CCStrike15ItemDefinition *pDef = ItemDef( FindItem( ArgItemID( pui, obj, 1 ) ) );
		pui->Params_SetResult( obj, pDef && !V_stricmp( pDef->GetDefinitionName(), ArgString( pui, obj, 2 ) ) );
	}
	void GetItemType( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const CCStrike15ItemDefinition *pDef = ItemDef( FindItem( ArgItemID( pui, obj, 1 ) ) );
		pui->Params_SetResult( obj, pDef && pDef->GetItemTypeName() ? pDef->GetItemTypeName() : "" );
	}
	void GetWear( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		CEconItemView *pItem = FindItem( ArgItemID( pui, obj, 1 ) );
		pui->Params_SetResult( obj, pItem ? pItem->GetCustomPaintKitWear() : 0.0f );
	}
	// kill eater, season access, deployment date...: offline items have none
	void GetItemAttributeValue( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 0 ); }

	// The preview dialog asks for the certificate of the item it's opening just
	// before it asks for the model; that's where the item ID comes from.
	// No certificates offline ("" hides the button).
	void GetItemCertificateInfo( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		s_ullInspectItemID = ArgItemID( pui, obj, 1 );
		pui->Params_SetResult( obj, "" );
	}

	// LaunchWeaponPreviewPanel( path ): show the item being previewed; "" closes it
	void LaunchWeaponPreviewPanel( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const char *pszPath = ArgString( pui, obj, 0 );
		if ( !pszPath[0] )
		{
			if ( s_pInspectPanel )
				s_pInspectPanel->SetVisible( false );
			return;
		}
		CEconItemView *pItem = FindItem( s_ullInspectItemID );
		if ( !pItem )
			return;
		if ( !s_pInspectPanel )
			s_pInspectPanel = new CFlashItemModelPanel( "InventoryInspectPanel", GetInspectModelPlacement, true, true );
		s_pInspectPanel->ShowItem( pItem );
	}

	// Things with no offline backend: empty/false/zero
	void ReturnZero( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 0 ); }
	void ReturnFalse( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, false ); }
	void ReturnTrue( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, true ); }
	void ReturnEmpty( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, "" ); }
	void ReturnMinusOne( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, -1 ); }
	void DoNothing( SCALEFORM_CALLBACK_ARGS_DECL ) {}
};
const char *CScaleformComponentInventory::s_pszSortForCompare = "newest";

class CScaleformComponentInventory_Table : public IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const
	{
		typedef CScaleformComponentInventory T;
		static const ScaleformUIFunctionHandlerDefinition table[] = {
			SFUI_DECL_METHOD( SetInventorySortAndFilters ),
			SFUI_DECL_METHOD( GetInventoryCount ),
			SFUI_DECL_METHOD( GetInventoryItemIDByIndex ),
			SFUI_DECL_METHOD( GetSortMethodsCount ),
			SFUI_DECL_METHOD( GetSortMethodByIndex ),
			SFUI_DECL_METHOD( GetSavedFilterMethod ),
			SFUI_DECL_METHOD( GetSavedSortMethod ),
			SFUI_DECL_METHOD( GetItemName ),
			SFUI_DECL_METHOD( GetItemRarityColor ),
			SFUI_DECL_METHOD( GetItemRarity ),
			SFUI_DECL_METHOD( GetSlot ),
			SFUI_DECL_METHOD( GetSlotSubPosition ),
			SFUI_DECL_METHOD( GetItemTeam ),
			SFUI_DECL_METHOD( IsEquipped ),
			SFUI_DECL_METHOD( GetItemInventoryImage ),
			SFUI_DECL_METHOD( IsItemInfoValid ),
			SFUI_DECL_METHOD( IsItemDefault ),
			SFUI_DECL_METHOD( GetItemDefinitionIndex ),
			SFUI_DECL_METHOD( GetItemDefinitionName ),
			SFUI_DECL_METHOD( DoesItemMatchDefinitionByName ),
			SFUI_DECL_METHOD( GetItemType ),
			SFUI_DECL_METHOD( GetWear ),
			SFUI_DECL_METHOD( GetItemAttributeValue ),
			// no backend offline
			SFUI_DECL_METHOD( IsInventoryImageCachable ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsTool" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsMarketable" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsDeletable" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "CanTradeUp" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "HasCustomName" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "HasMusic" ),
			SFUI_DECL_METHOD( IsItemUnusual ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsCouponCrate" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "CheckCampaignOwnership" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "DoesUserOwnQuest" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsItemStickerAtExtremeWear" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "SetStickerToolSlot" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "SetNameToolString" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsCraftReady" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "ItemHasScorecardValues" ),
			SFUI_DECL_METHOD_AS( ReturnTrue, "TestMusicVolume" ),
			SFUI_DECL_METHOD( GetItemCapabilitiesCount ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetChosenActionItemsCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetItemStickerCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetItemStickerSlotCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetAssociatedItemsCount" ),
			SFUI_DECL_METHOD( GetLootListItemsCount ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetNumItemsNeededToTradeUp" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetUnacknowledgeItemsCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetSprayChargesAsBaseline" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetCraftIngredientCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMaxCraftIngredientsNeeded" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetInventoryCountDefault" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMissionBacklog" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetSecondsUntilNextMission" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetQuestEventCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetCacheTypeElementsCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMaxLevel" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetActiveQuest" ),
			SFUI_DECL_METHOD_AS( ReturnMinusOne, "GetActiveSeasonCoinItemId" ),
			SFUI_DECL_METHOD( GetItemCapabilityByIndex ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemCapabilityDisabledMessageByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetChosenActionItemIDByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemStickerImageByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemStickerImageBySlot" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemStickerNameByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemGifterXuid" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemPickupMethod" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetSprayTintColorCode" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetToolType" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemTypeFromEnum" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetUnacknowledgeItemByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetAssociatedItemIdByIndex" ),
			SFUI_DECL_METHOD( GetLootListItemIdByIndex ),
			SFUI_DECL_METHOD( GetLootListUnusualItemImage ),
			SFUI_DECL_METHOD( GetLootListUnusualItemName ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetTradeUpContractItemID" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetCampaignForSeason" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetCampaignName" ),
			SFUI_DECL_METHOD( GetItemCertificateInfo ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetSet" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemSet" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemDescription" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetWeaponCategory" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMarketCraftCompletionLink" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetCraftIngredientByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetFlairItemName" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetFlairItemId" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMusicNameFromMusicID" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemInventoryImageFromMusicID" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetRawDefinitionKey" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetCacheTypeElementFieldByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMusicIDForPlayer" ),
			SFUI_DECL_METHOD_AS( DoNothing, "AcknowledgeNewItems" ),
			SFUI_DECL_METHOD_AS( DoNothing, "AcknowledgeNewItembyItemID" ),
			SFUI_DECL_METHOD( LaunchWeaponPreviewPanel ),
			SFUI_DECL_METHOD_AS( DoNothing, "PlayAudioFile" ),
			SFUI_DECL_METHOD_AS( DoNothing, "SetDefaultMusicVolume" ),
			SFUI_DECL_METHOD_AS( DoNothing, "SellItem" ),
			SFUI_DECL_METHOD_AS( DoNothing, "DeleteItem" ),
			SFUI_DECL_METHOD( UseTool ),
			SFUI_DECL_METHOD_AS( DoNothing, "ClearCustomName" ),
			SFUI_DECL_METHOD_AS( DoNothing, "HighlightStickerBySlot" ),
			SFUI_DECL_METHOD_AS( DoNothing, "PreviewStickerInModelPanel" ),
			SFUI_DECL_METHOD_AS( DoNothing, "PeelEffectStickerBySlot" ),
			SFUI_DECL_METHOD_AS( DoNothing, "WearItemSticker" ),
			SFUI_DECL_METHOD_AS( DoNothing, "PlayItemPreviewMusic" ),
			SFUI_DECL_METHOD_AS( DoNothing, "StopItemPreviewMusic" ),
			SFUI_DECL_METHOD_AS( DoNothing, "CancelQuestAudio" ),
			SFUI_DECL_METHOD_AS( DoNothing, "AddCraftIngredient" ),
			SFUI_DECL_METHOD_AS( DoNothing, "RemoveCraftIngredient" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ClearCraftIngredients" ),
			SFUI_DECL_METHOD_AS( DoNothing, "CraftIngredients" ),
			SFUI_DECL_METHOD_AS( DoNothing, "SetCraftTarget" ),
			SFUI_DECL_METHOD_AS( DoNothing, "OnViewOffers" ),
			SFUI_DECL_METHOD_AS( DoNothing, "RequestNewMission" ),
			SFUI_DECL_METHOD_AS( DoNothing, "RequestPrestigeCoin" ),
			{ NULL, NULL }
		};
		return table;
	}
};

//-----------------------------------------------------------------------------
// CScaleformComponent_Loadout
//-----------------------------------------------------------------------------
class CScaleformComponentLoadout : public ScaleformUIFunctionHandlerObject
{
public:
	// GetItemID( xuid, team, slot ) -> item id of what's in that loadout slot
	void GetItemID( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		ResultItemID( pui, obj, LoadoutItemID( TeamFromString( ArgString( pui, obj, 1 ) ), SlotFromString( ArgString( pui, obj, 2 ) ) ) );
	}

	// GetDefaultItem( xuid, team, slot ) -> the slot's base item
	void GetDefaultItem( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		int nSlot = SlotFromString( ArgString( pui, obj, 2 ) );
		CEconItemView *pBase = nSlot >= 0 ? CSInventoryManager()->GetBaseItemForTeam( TeamFromString( ArgString( pui, obj, 1 ) ), nSlot ) : NULL;
		ResultItemID( pui, obj, ItemIDForView( pBase ) );
	}

	// EquipItemInSlot( team, itemid, slot )
	void EquipItemInSlot( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		int iTeam = TeamFromString( ArgString( pui, obj, 0 ) );
		uint64 ullID = ArgItemID( pui, obj, 1 );
		int nSlot = SlotFromString( ArgString( pui, obj, 2 ) );
		if ( nSlot < 0 )
			nSlot = ItemSlot( FindItem( ullID ), iTeam );
		bool bOK = nSlot >= 0 && CSInventoryManager()->EquipItemInLoadout( iTeam, nSlot, ullID );
		CEconItemView *pItem = FindItem( ullID );
		const CCStrike15ItemDefinition *pDef = ItemDef( pItem );
		VERBOSE_PRINTF( "[offline] equip team %d id %llu slot '%s'->%d: %s (item %s, def slot %d/%d, now in slot %llu, equipped %d)\n",
			iTeam, ullID, ArgString( pui, obj, 2 ), nSlot, bOK ? "ok" : "FAILED", pDef ? pDef->GetDefinitionName() : "NOT FOUND",
			pDef ? pDef->GetDefaultLoadoutSlot() : -1, pDef ? pDef->GetLoadoutSlot( iTeam ) : -1,
			LoadoutItemID( iTeam, nSlot ), pItem ? (int)IsItemEquipped( pItem, iTeam ) : -1 );
		fflush( stdout );
		pui->Params_SetResult( obj, bOK );
	}

	// CleanupDuplicateBaseItems( xuid, team )
	void CleanupDuplicateBaseItems( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		CSInventoryManager()->CleanupDuplicateBaseItems( TeamFromString( ArgString( pui, obj, 1 ) ) );
	}

	void ReturnTrue( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, true ); }
	void ReturnZero( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 0 ); }
	void ReturnEmpty( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, "" ); }
	void DoNothing( SCALEFORM_CALLBACK_ARGS_DECL ) {}
};

class CScaleformComponentLoadout_Table : public IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const
	{
		typedef CScaleformComponentLoadout T;
		static const ScaleformUIFunctionHandlerDefinition table[] = {
			SFUI_DECL_METHOD( GetItemID ),
			SFUI_DECL_METHOD( GetDefaultItem ),
			SFUI_DECL_METHOD( EquipItemInSlot ),
			SFUI_DECL_METHOD( CleanupDuplicateBaseItems ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetItemGamePrice" ),
			SFUI_DECL_METHOD_AS( ReturnTrue, "IsLoadoutAllowed" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetSprayCooldownRemaining" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetSprayCooldownSetting" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetSprayApplicationError" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionSpray" ),
			{ NULL, NULL }
		};
		return table;
	}
};

//-----------------------------------------------------------------------------
// CScaleformComponent_MyPersona
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Offline profile: cfg/user.cfg (in the csgo folder) sets what the menus and the
// scoreboard show for you. Created with defaults on first run; edit and restart.
// The rank/level/wins/commends convars are userinfo so the (local) server can
// put them in the player resource for the scoreboard.
//-----------------------------------------------------------------------------
static void ProfileNameChanged( IConVar *var, const char *pOldValue, float flOldValue );
static ConVar profile_name( "profile_name", "Player", FCVAR_RELEASE, "Your name (also sets 'name')", ProfileNameChanged );
static ConVar profile_avatar( "profile_avatar", "", FCVAR_RELEASE, "Avatar image in the csgo folder (PNG or JPG, square), e.g. avatar.png; empty = default" );
static ConVar profile_level( "profile_level", "1", FCVAR_RELEASE | FCVAR_USERINFO, "Profile rank (level) 1-40", true, 1, true, 40 );
static ConVar profile_xp( "profile_xp", "1", FCVAR_RELEASE, "XP towards the next level (0-4999)", true, 0, true, 4999 );
static ConVar profile_rank( "profile_rank", "1", FCVAR_RELEASE | FCVAR_USERINFO, "Competitive skill group: 0 none, 1 Silver I ... 18 Global Elite", true, 0, true, 18 );
static ConVar profile_wins( "profile_wins", "1", FCVAR_RELEASE | FCVAR_USERINFO, "Competitive wins", true, 0, false, 0 );
static ConVar profile_commend_friendly( "profile_commend_friendly", "1", FCVAR_RELEASE | FCVAR_USERINFO, "Commendations: friendly", true, 0, false, 0 );
static ConVar profile_commend_teaching( "profile_commend_teaching", "1", FCVAR_RELEASE | FCVAR_USERINFO, "Commendations: teacher", true, 0, false, 0 );
static ConVar profile_commend_leader( "profile_commend_leader", "1", FCVAR_RELEASE | FCVAR_USERINFO, "Commendations: leader", true, 0, false, 0 );

static void ProfileNameChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	const char *pszName = profile_name.GetString();
	if ( !pszName || !pszName[0] )
		return;
	// the offline Steam stub answers persona name queries from this
	setenv( "STEAM_PERSONA_NAME", pszName, 1 );
	static ConVarRef name( "name" );
	if ( name.IsValid() && V_strcmp( name.GetString(), pszName ) )
		name.SetValue( pszName );
}

static const char s_szDefaultUserCfg[] =
	"// Your offline profile. Edit the values and restart the game.\n"
	"\n"
	"profile_name \"Player\"          // your name\n"
	"profile_avatar \"\"              // avatar: a PNG or JPG in the csgo folder, e.g. \"avatar.png\" (square, 184x184 or 64x64 looks best)\n"
	"profile_level 1                // profile rank (level), 1-40\n"
	"profile_xp 1                   // XP towards the next level, 0-4999\n"
	"profile_rank 1                 // skill group: 0 none, 1 Silver I, 2 Silver II, 3 Silver III, 4 Silver IV,\n"
	"                               //   5 Silver Elite, 6 Silver Elite Master, 7-10 Gold Nova I-Master,\n"
	"                               //   11 Master Guardian I, 12 MG II, 13 MG Elite, 14 DMG,\n"
	"                               //   15 Legendary Eagle, 16 LEM, 17 Supreme, 18 Global Elite\n"
	"profile_wins 1                 // competitive wins\n"
	"profile_commend_friendly 1\n"
	"profile_commend_teaching 1\n"
	"profile_commend_leader 1\n";

static void LoadUserProfile()
{
	static bool s_bLoaded = false;
	if ( s_bLoaded )
		return;
	s_bLoaded = true;

	if ( !g_pFullFileSystem->FileExists( "cfg/user.cfg", "MOD" ) )
	{
		g_pFullFileSystem->CreateDirHierarchy( "cfg", "MOD" );
		FileHandle_t f = g_pFullFileSystem->Open( "cfg/user.cfg", "wb", "MOD" );
		if ( f )
		{
			g_pFullFileSystem->Write( s_szDefaultUserCfg, V_strlen( s_szDefaultUserCfg ), f );
			g_pFullFileSystem->Close( f );
		}
	}
	engine->ExecuteClientCmd( "exec user.cfg" );
	ProfileNameChanged( NULL, NULL, 0.0f );
}

static uint64 GetLocalXuid();

static bool IsLocalXuid( uint64 xuid )
{
	uint64 ullLocal = GetLocalXuid();
	// the UI passes full 64-bit Steam IDs; compare account IDs
	return xuid && ullLocal && ( xuid & 0xFFFFFFFFull ) == ( ullLocal & 0xFFFFFFFFull );
}

class CScaleformComponentMyPersona : public ScaleformUIFunctionHandlerObject
{
public:
	void GetXuid( SCALEFORM_CALLBACK_ARGS_DECL ) { ResultItemID( pui, obj, GetLocalXuid() ); }
	void ReturnXpPerLevel( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 5000 ); }
	void IsInventoryValid( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, LocalInventory() != NULL ); }
	void ReturnZero( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 0 ); }
	void ReturnFalse( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, false ); }
	void ReturnEmpty( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, "" ); }
	void DoNothing( SCALEFORM_CALLBACK_ARGS_DECL ) {}
};

class CScaleformComponentMyPersona_Table : public IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const
	{
		typedef CScaleformComponentMyPersona T;
		static const ScaleformUIFunctionHandlerDefinition table[] = {
			SFUI_DECL_METHOD( GetXuid ),
			SFUI_DECL_METHOD( IsInventoryValid ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetLauncherType" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetLicenseType" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetFriendCode" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetActiveXpBonuses" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMyOfficialTeamName" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMyOfficialTeamTag" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMyOfficialTournamentName" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMyNotifications" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMyClanIdByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMyClanNameById" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMyClanTagById" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetMyMedalAdditionalInfo" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMyOfficialTeamID" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMyClanCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetMyMedalRankByType" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetTimePlayedConsecutively" ),
			SFUI_DECL_METHOD_AS( ReturnXpPerLevel, "GetXpPerLevel" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetElevatedState" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetElevatedTime" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "HasPrestige" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsVacBanned" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionAcknowledgeNotifications" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionElevate" ),
			{ NULL, NULL }
		};
		return table;
	}
};

//-----------------------------------------------------------------------------
// FriendsList: the profile panels ask it about you (by your XUID). Other XUIDs
// get their in-game name if they're in the game; anything else stays undefined,
// as before this component existed, so the panels keep their own fallbacks.
//-----------------------------------------------------------------------------
class CScaleformComponentFriendsList : public ScaleformUIFunctionHandlerObject
{
public:
	void GetFriendName( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		uint64 xuid = ArgItemID( pui, obj, 0 );
#if defined( IOS )
		if ( const LanGame_t *pGame = LanBrowser_FindByXuid( xuid ) )
		{
			pui->Params_SetResult( obj, pGame->m_szName );
			return;
		}
#endif
		if ( IsLocalXuid( xuid ) )
		{
			pui->Params_SetResult( obj, profile_name.GetString() );
			return;
		}
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			player_info_t pi;
			if ( engine->GetPlayerInfo( i, &pi ) && pi.xuid && ( pi.xuid & 0xFFFFFFFFull ) == ( xuid & 0xFFFFFFFFull ) )
			{
				pui->Params_SetResult( obj, pi.name );
				return;
			}
		}
	}
	void GetFriendLevel( SCALEFORM_CALLBACK_ARGS_DECL ) { LocalInt( pui, obj, profile_level.GetInt() ); }
	void GetFriendXp( SCALEFORM_CALLBACK_ARGS_DECL ) { LocalInt( pui, obj, profile_xp.GetInt() ); }
	void GetFriendCompetitiveRank( SCALEFORM_CALLBACK_ARGS_DECL ) { LocalInt( pui, obj, profile_rank.GetInt() ); }
	void GetFriendCompetitiveWins( SCALEFORM_CALLBACK_ARGS_DECL ) { LocalInt( pui, obj, profile_wins.GetInt() ); }
	void GetFriendCommendations( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const char *pszType = ArgString( pui, obj, 1 );
		const ConVar &cv = !V_stricmp( pszType, "teaching" ) ? profile_commend_teaching :
						   !V_stricmp( pszType, "leader" ) ? profile_commend_leader : profile_commend_friendly;
		LocalInt( pui, obj, cv.GetInt() );
	}
	void ReturnZero( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 0 ); }
	void ReturnFalse( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, false ); }
	void DoNothing( SCALEFORM_CALLBACK_ARGS_DECL ) {}

	// The friends tab lists LAN games (iOS): each is a "friend" named after the
	// host, playing CS:GO, joinable. There are no Steam friends offline.
	void GetCount( SCALEFORM_CALLBACK_ARGS_DECL )
	{
#if defined( IOS )
		pui->Params_SetResult( obj, LanBrowser_Count() );
#else
		pui->Params_SetResult( obj, 0 );
#endif
	}
	void GetXuidByIndex( SCALEFORM_CALLBACK_ARGS_DECL )
	{
#if defined( IOS )
		const LanGame_t *pGame = LanBrowser_Get( ArgInt( pui, obj, 0 ) );
		ResultItemID( pui, obj, pGame ? pGame->m_ullXuid : 0 );
#else
		ResultItemID( pui, obj, 0 );
#endif
	}
	void GetFriendStatus( SCALEFORM_CALLBACK_ARGS_DECL )
	{
#if defined( IOS )
		// "Mirage - 2/10 players" (the host counts only human slots)
		if ( const LanGame_t *pGame = LanBrowser_FindByXuid( ArgItemID( pui, obj, 0 ) ) )
		{
			wchar_t wszMap[64];
			const wchar_t *pwszMap = g_pVGuiLocalize ? g_pVGuiLocalize->Find( CFmtStr( "#SFUI_Map_%s", pGame->m_szMap ) ) : NULL;
			if ( !pwszMap )
			{
				V_UTF8ToUnicode( pGame->m_szMap, wszMap, sizeof( wszMap ) );
				pwszMap = wszMap;
			}
			wchar_t wszStatus[128];
			V_snwprintf( wszStatus, ARRAYSIZE( wszStatus ), L"%ls - %d/%d players", pwszMap, pGame->m_nHumans, pGame->m_nMax );
			pui->Params_SetResult( obj, wszStatus );
			return;
		}
#endif
		pui->Params_SetResult( obj, "" );
	}
	void GetFriendStatusBucket( SCALEFORM_CALLBACK_ARGS_DECL )
	{
#if defined( IOS )
		if ( LanBrowser_FindByXuid( ArgItemID( pui, obj, 0 ) ) )
		{
			pui->Params_SetResult( obj, "PlayingCSGO" );		// green, "in game"
			return;
		}
#endif
		pui->Params_SetResult( obj, "Offline" );
	}
	void IsFriendJoinable( SCALEFORM_CALLBACK_ARGS_DECL )
	{
#if defined( IOS )
		pui->Params_SetResult( obj, LanBrowser_FindByXuid( ArgItemID( pui, obj, 0 ) ) != NULL );
#else
		pui->Params_SetResult( obj, false );
#endif
	}
	void GetFriendRelationship( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		// not "friend": the context menu would add Message and Remove Friend
		pui->Params_SetResult( obj, "none" );
	}
	void ActionJoinFriendSession( SCALEFORM_CALLBACK_ARGS_DECL )
	{
#if defined( IOS )
		LanBrowser_Join( LanBrowser_FindByXuid( ArgItemID( pui, obj, 0 ) ) );
#endif
	}

private:
	void LocalInt( IUIMarshalHelper *pui, SFPARAMS obj, int nValue )
	{
		if ( IsLocalXuid( ArgItemID( pui, obj, 0 ) ) )
			pui->Params_SetResult( obj, nValue );
		else
			pui->Params_SetResult( obj, 0 );
	}
};

class CScaleformComponentFriendsList_Table : public IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const
	{
		typedef CScaleformComponentFriendsList T;
		static const ScaleformUIFunctionHandlerDefinition table[] = {
			SFUI_DECL_METHOD( GetFriendName ),
			SFUI_DECL_METHOD( GetFriendLevel ),
			SFUI_DECL_METHOD( GetFriendXp ),
			SFUI_DECL_METHOD( GetFriendCompetitiveRank ),
			SFUI_DECL_METHOD( GetFriendCompetitiveWins ),
			SFUI_DECL_METHOD( GetFriendCommendations ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetFriendMedalRankByType" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetFriendDisplayItemDefCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetFriendDisplayItemDefFeatured" ),
			SFUI_DECL_METHOD( GetCount ),
			SFUI_DECL_METHOD( GetXuidByIndex ),
			SFUI_DECL_METHOD( GetFriendStatus ),
			SFUI_DECL_METHOD( GetFriendStatusBucket ),
			SFUI_DECL_METHOD( IsFriendJoinable ),
			SFUI_DECL_METHOD( GetFriendRelationship ),
			SFUI_DECL_METHOD( ActionJoinFriendSession ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsFriendInvited" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsFriendWatchable" ),
			SFUI_DECL_METHOD_AS( ReturnFalse, "IsFriendPlayingCSGO" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetFriendRequestsCount" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionInviteFriend" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionWatchFriendSession" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionShowCSGOProfile" ),
			{ NULL, NULL }
		};
		return table;
	}
};

//-----------------------------------------------------------------------------
// Small components the same screens touch: all empty offline
//-----------------------------------------------------------------------------
class CScaleformComponentStub : public ScaleformUIFunctionHandlerObject
{
public:
	void ReturnZero( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 0 ); }
	void ReturnMinusOne( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, -1 ); }
	void ReturnFalse( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, false ); }
	void ReturnTrue( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, true ); }
	void ReturnEmpty( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, "" ); }
	void Return730( SCALEFORM_CALLBACK_ARGS_DECL ) { pui->Params_SetResult( obj, 730 ); }
	void DoNothing( SCALEFORM_CALLBACK_ARGS_DECL ) {}
};

//-----------------------------------------------------------------------------
// CompetitiveMatch: never installed, so every call returned undefined and the
// main menu's warnings panel read GetCooldownSecondsRemaining() != 0 (undefined
// isn't 0 in ActionScript) as a matchmaking ban. Offline: no cooldown, no match.
//-----------------------------------------------------------------------------
class CScaleformComponentCompetitiveMatch_Table : public IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const
	{
		typedef CScaleformComponentStub T;
		static const ScaleformUIFunctionHandlerDefinition table[] = {
			SFUI_DECL_METHOD_AS( ReturnFalse, "HasOngoingMatch" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetCooldownSecondsRemaining" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetCooldownType" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetCooldownReason" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetTournamentStageCount" ),
			SFUI_DECL_METHOD_AS( ReturnZero, "GetTournamentTeamCount" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetTournamentStageNameByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetTournamentTeamNameByID" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetTournamentTeamNameByIndex" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetTournamentTeamTagByID" ),
			SFUI_DECL_METHOD_AS( ReturnEmpty, "GetTournamentTeamTagByIndex" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionAbandonOngoingMatch" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionAcknowledgePenalty" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionReconnectToOngoingMatch" ),
			SFUI_DECL_METHOD_AS( DoNothing, "ActionMatchmaking" ),
			{ NULL, NULL }
		};
		return table;
	}
};

#define STUB_TABLE( name, ... ) \
class CScaleformComponent##name##_Table : public IScaleformUIFunctionHandlerDefinitionTable \
{ \
public: \
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const \
	{ \
		typedef CScaleformComponentStub T; \
		static const ScaleformUIFunctionHandlerDefinition table[] = { __VA_ARGS__, { NULL, NULL } }; \
		return table; \
	} \
};

// ImageCache: EnsureInventoryImageCached registers the item with Scaleform's
// inventory images (img://inventory_<id>), which starts the icon render; the image
// is transparent until the render is done, then updates in place.
class CScaleformComponentImageCache : public ScaleformUIFunctionHandlerObject
{
public:
	void EnsureInventoryImageCached( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		extern IScaleformInventoryImageProvider *g_pIScaleformInventoryImageProvider;
		uint64 ullID = ArgItemID( pui, obj, 0 );
		if ( !ullID || !g_pScaleformUI || !g_pIScaleformInventoryImageProvider )
			return;
		// one reference per item for the session: the icons stay cached while you browse
		if ( m_Cached.Find( ullID ) != m_Cached.InvalidIndex() )
			return;
		if ( !IsRenderedIconItem( FindItem( ullID ) ) )
			return;
		if ( g_pScaleformUI->InventoryImageAddRef( ullID, g_pIScaleformInventoryImageProvider ) )
			m_Cached.Insert( ullID );
	}
	void DoNothing( SCALEFORM_CALLBACK_ARGS_DECL ) {}

private:
	CUtlRBTree< uint64 > m_Cached{ DefLessFunc( uint64 ) };
};

class CScaleformComponentImageCache_Table : public IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const
	{
		typedef CScaleformComponentImageCache T;
		static const ScaleformUIFunctionHandlerDefinition table[] = {
			SFUI_DECL_METHOD( EnsureInventoryImageCached ),
			SFUI_DECL_METHOD_AS( DoNothing, "EnsureItemDataImageCached" ),
			SFUI_DECL_METHOD_AS( DoNothing, "EnsureAvatarCached" ),
			{ NULL, NULL }
		};
		return table;
	}
};

STUB_TABLE( ItemData,
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemName" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemDescription" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetItemInventoryImage" ) )

STUB_TABLE( Store,
	SFUI_DECL_METHOD_AS( ReturnFalse, "IsBundle" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "IsItemNew" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "IsBannerEntryMarketLink" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "BStoreDisabledInBetaMode" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetBundleItemCount" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetBannerEntryCount" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetBannerEntryDefIdx" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetStoreItemCount" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetStoreItemPercentReduction" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetSecondsUntilTimestamp" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetBundleItemByIndex" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetBannerEntryCustomFormatString" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetStoreItemName_Underscores" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetStoreItemOriginalPrice" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetStoreItemSalePrice" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetStoreItemsSalePrice" ),
	SFUI_DECL_METHOD_AS( DoNothing, "StoreItemPurchase" ),
	SFUI_DECL_METHOD_AS( DoNothing, "PurchaseKeyAndOpenCrate" ),
	SFUI_DECL_METHOD_AS( DoNothing, "PurchaseItemWithStaticAttrValue" ),
	SFUI_DECL_METHOD_AS( DoNothing, "RecordUIEvent" ) )

STUB_TABLE( SteamOverlay,
	SFUI_DECL_METHOD_AS( ReturnFalse, "IsEnabled" ),
	// the friends panel shows "you're offline" otherwise; its friends tab lists LAN games
	SFUI_DECL_METHOD_AS( ReturnTrue, "BSignedInToFriends" ),
	SFUI_DECL_METHOD_AS( DoNothing, "InteractWithUser" ),
	SFUI_DECL_METHOD_AS( DoNothing, "StartChatWithUser" ),
	SFUI_DECL_METHOD_AS( Return730, "GetAppID" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetSteamCommunityURL" ),
	SFUI_DECL_METHOD_AS( DoNothing, "OpenURL" ),
	SFUI_DECL_METHOD_AS( DoNothing, "OpenExternalBrowserURL" ),
	SFUI_DECL_METHOD_AS( DoNothing, "ShowUserProfilePage" ) )

STUB_TABLE( PartyList,
	SFUI_DECL_METHOD_AS( ReturnFalse, "IsPartySessionActive" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetPartySessionSetting" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetCount" ),
	SFUI_DECL_METHOD_AS( ReturnEmpty, "GetXuidByIndex" ) )

STUB_TABLE( News,
	SFUI_DECL_METHOD_AS( ReturnZero, "GetActiveTournamentEventID" ),
	SFUI_DECL_METHOD_AS( ReturnFalse, "IsNewClientAvailable" ) )

STUB_TABLE( Predictions,
	SFUI_DECL_METHOD_AS( ReturnMinusOne, "GetMyPredictionItemIDEventSectionIndex" ),
	SFUI_DECL_METHOD_AS( ReturnZero, "GetEventSectionsCount" ) )

//-----------------------------------------------------------------------------
// Installation into the full-screen slot (menus), once it exists
//-----------------------------------------------------------------------------
static CScaleformComponentInventory g_ComponentInventory;
static CScaleformComponentLoadout g_ComponentLoadout;
static CScaleformComponentMyPersona g_ComponentMyPersona;
static CScaleformComponentStub g_ComponentStub;
static CScaleformComponentFriendsList g_ComponentFriendsList;
static CScaleformComponentFriendsList_Table g_ComponentFriendsListTable;
static CScaleformComponentCompetitiveMatch_Table g_ComponentCompetitiveMatchTable;

static CScaleformComponentInventory_Table g_ComponentInventoryTable;
static CScaleformComponentLoadout_Table g_ComponentLoadoutTable;
static CScaleformComponentMyPersona_Table g_ComponentMyPersonaTable;
static CScaleformComponentImageCache g_ComponentImageCache;
static CScaleformComponentImageCache_Table g_ComponentImageCacheTable;
static CScaleformComponentItemData_Table g_ComponentItemDataTable;
static CScaleformComponentStore_Table g_ComponentStoreTable;
static CScaleformComponentSteamOverlay_Table g_ComponentSteamOverlayTable;
static CScaleformComponentPartyList_Table g_ComponentPartyListTable;
static CScaleformComponentNews_Table g_ComponentNewsTable;
static CScaleformComponentPredictions_Table g_ComponentPredictionsTable;

void ScaleformInventoryComponents_EnsureInstalled()
{
	static bool s_bInstalled = false;
	if ( s_bInstalled || !g_pScaleformUI )
		return;

	LoadUserProfile();

	struct Component_t { const char *m_pszName; ScaleformUIFunctionHandlerObject *m_pObject; const IScaleformUIFunctionHandlerDefinitionTable *m_pTable; SFVALUE m_hValue; };
	static Component_t s_Components[] =
	{
		{ "CScaleformComponent_Inventory", &g_ComponentInventory, &g_ComponentInventoryTable, NULL },
		{ "CScaleformComponent_Loadout", &g_ComponentLoadout, &g_ComponentLoadoutTable, NULL },
		{ "CScaleformComponent_MyPersona", &g_ComponentMyPersona, &g_ComponentMyPersonaTable, NULL },
		{ "CScaleformComponent_ImageCache", &g_ComponentImageCache, &g_ComponentImageCacheTable, NULL },
		{ "CScaleformComponent_ItemData", &g_ComponentStub, &g_ComponentItemDataTable, NULL },
		{ "CScaleformComponent_Store", &g_ComponentStub, &g_ComponentStoreTable, NULL },
		{ "CScaleformComponent_SteamOverlay", &g_ComponentStub, &g_ComponentSteamOverlayTable, NULL },
		{ "CScaleformComponent_PartyList", &g_ComponentStub, &g_ComponentPartyListTable, NULL },
		{ "CScaleformComponent_News", &g_ComponentStub, &g_ComponentNewsTable, NULL },
		// match history for the Watch panel (matchhistory_scaleform.cpp): filled below
		{ "CScaleformComponent_MatchList", NULL, NULL, NULL },
		{ "CScaleformComponent_MatchInfo", NULL, NULL, NULL },
		{ "CScaleformComponent_Predictions", &g_ComponentStub, &g_ComponentPredictionsTable, NULL },
		{ "CScaleformComponent_FriendsList", &g_ComponentFriendsList, &g_ComponentFriendsListTable, NULL },
		{ "CScaleformComponent_CompetitiveMatch", &g_ComponentStub, &g_ComponentCompetitiveMatchTable, NULL },
	};

	extern void MatchHistory_GetComponents( ScaleformUIFunctionHandlerObject **ppMatchList, const IScaleformUIFunctionHandlerDefinitionTable **ppMatchListTable,
		ScaleformUIFunctionHandlerObject **ppMatchInfo, const IScaleformUIFunctionHandlerDefinitionTable **ppMatchInfoTable );
	for ( int i = 0; i < ARRAYSIZE( s_Components ); i++ )
	{
		if ( !V_strcmp( s_Components[i].m_pszName, "CScaleformComponent_MatchList" ) )
			MatchHistory_GetComponents( &s_Components[i].m_pObject, &s_Components[i].m_pTable, &s_Components[i + 1].m_pObject, &s_Components[i + 1].m_pTable );
	}

	// install into the full-screen slot once it exists (the first try succeeds or none do)
	g_pScaleformUI->InstallGlobalObject( SF_FULL_SCREEN_SLOT, s_Components[0].m_pszName, s_Components[0].m_pObject, s_Components[0].m_pTable, &s_Components[0].m_hValue );
	if ( !s_Components[0].m_hValue )
		return;
	for ( int i = 1; i < ARRAYSIZE( s_Components ); i++ )
		g_pScaleformUI->InstallGlobalObject( SF_FULL_SCREEN_SLOT, s_Components[i].m_pszName, s_Components[i].m_pObject, s_Components[i].m_pTable, &s_Components[i].m_hValue );

	s_bInstalled = true;

#if defined( IOS )
	// the friends panel's friends tab is the LAN game list
	if ( g_pVGuiLocalize )
	{
		static wchar_t s_wszTitle[] = L"LAN Games";
		static wchar_t s_wszEmpty[] = L"No LAN games found. Games hosted on this Wi-Fi (Play > Offline with bots) show up here; tap one to join.";
		g_pVGuiLocalize->AddString( "SFUI_Lobby_FriendsListerTitle", s_wszTitle, NULL );
		g_pVGuiLocalize->AddString( "SFUI_Friends_Play", s_wszEmpty, NULL );
	}
#endif

	VERBOSE_PRINTF( "[sf] installed inventory/loadout components\n" );
	fflush( stdout );
}

#endif // INCLUDE_SCALEFORM
