//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: _global.CScaleformComponent_GameTypes, which single-player.swf,
//          lobby.swf, tooltips.swf and others use to read map group and game
//          mode data (map names, icons, map lists). The component was removed
//          from the leaked source; without it every lookup returns undefined.
//          Answers come straight from GameModes.txt, like Panorama's GameTypesAPI.
//
//===========================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

class CScaleformComponentGameTypes : public ScaleformUIFunctionHandlerObject
{
public:
	CScaleformComponentGameTypes() : m_pKV( NULL ) {}

	KeyValues *Data()
	{
		if ( !m_pKV )
		{
			m_pKV = new KeyValues( "GameModes.txt" );
			if ( !m_pKV->LoadFromFile( g_pFullFileSystem, "GameModes.txt" ) )
				Warning( "CScaleformComponent_GameTypes: failed to load GameModes.txt\n" );
		}
		return m_pKV;
	}

	KeyValues *FindMapGroup( const char *pszMapGroup )
	{
		KeyValues *pGroups = Data()->FindKey( "mapgroups" );
		return ( pGroups && pszMapGroup && *pszMapGroup ) ? pGroups->FindKey( pszMapGroup ) : NULL;
	}

	KeyValues *FindGameMode( const char *pszType, const char *pszMode )
	{
		KeyValues *pTypes = Data()->FindKey( "gameTypes" );
		KeyValues *pType = ( pTypes && pszType && *pszType ) ? pTypes->FindKey( pszType ) : NULL;
		KeyValues *pModes = pType ? pType->FindKey( "gameModes" ) : NULL;
		return ( pModes && pszMode && *pszMode ) ? pModes->FindKey( pszMode ) : NULL;
	}

	static const char *StringArg( IUIMarshalHelper *pui, SFPARAMS obj, int i )
	{
		if ( (int)pui->Params_GetNumArgs( obj ) <= i )
			return "";
		IUIMarshalHelper::_ValueType t = pui->Params_GetArgType( obj, i );
		if ( t != IUIMarshalHelper::VT_String && t != IUIMarshalHelper::VT_StringW )
			return "";
		const char *psz = pui->Params_GetArgAsString( obj, i );
		return psz ? psz : "";
	}

	// GetMapGroupAttribute( mapGroup, attribute ) -> string
	void GetMapGroupAttribute( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		KeyValues *pGroup = FindMapGroup( StringArg( pui, obj, 0 ) );
		pui->Params_SetResult( obj, pGroup ? pGroup->GetString( StringArg( pui, obj, 1 ), "" ) : "" );
	}

	// GetMapGroupAttributeSubKeys( mapGroup, attribute ) -> "key1,key2,..."
	void GetMapGroupAttributeSubKeys( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		char szResult[2048] = "";
		KeyValues *pGroup = FindMapGroup( StringArg( pui, obj, 0 ) );
		KeyValues *pSub = pGroup ? pGroup->FindKey( StringArg( pui, obj, 1 ) ) : NULL;
		if ( pSub )
		{
			for ( KeyValues *pKey = pSub->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
			{
				if ( szResult[0] )
					V_strncat( szResult, ",", sizeof( szResult ) );
				V_strncat( szResult, pKey->GetName(), sizeof( szResult ) );
			}
		}
		pui->Params_SetResult( obj, szResult );
	}

	// GetGameModeAttribute( gameType, gameMode, attribute ) -> string
	void GetGameModeAttribute( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		KeyValues *pMode = FindGameMode( StringArg( pui, obj, 0 ), StringArg( pui, obj, 1 ) );
		pui->Params_SetResult( obj, pMode ? pMode->GetString( StringArg( pui, obj, 2 ), "" ) : "" );
	}

	// GetGameModeType( gameMode ) -> name of the game type that contains it
	void GetGameModeType( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		const char *pszMode = StringArg( pui, obj, 0 );
		KeyValues *pTypes = Data()->FindKey( "gameTypes" );
		for ( KeyValues *pType = pTypes ? pTypes->GetFirstTrueSubKey() : NULL; pType; pType = pType->GetNextTrueSubKey() )
		{
			KeyValues *pModes = pType->FindKey( "gameModes" );
			if ( pModes && *pszMode && pModes->FindKey( pszMode ) )
			{
				pui->Params_SetResult( obj, pType->GetName() );
				return;
			}
		}
		pui->Params_SetResult( obj, "" );
	}

	// no operation season is active offline; -1 hides the season upsell
	void GetActiveSeasionIndexValue( SCALEFORM_CALLBACK_ARGS_DECL )
	{
		pui->Params_SetResult( obj, -1 );
	}

private:
	KeyValues *m_pKV;
};

class CScaleformComponentGameTypes_Table : public IScaleformUIFunctionHandlerDefinitionTable
{
public:
	virtual const ScaleformUIFunctionHandlerDefinition *GetTable( void ) const
	{
		typedef CScaleformComponentGameTypes T;
		static const ScaleformUIFunctionHandlerDefinition table[] = {
			SFUI_DECL_METHOD( GetMapGroupAttribute ),
			SFUI_DECL_METHOD( GetMapGroupAttributeSubKeys ),
			SFUI_DECL_METHOD( GetGameModeAttribute ),
			SFUI_DECL_METHOD( GetGameModeType ),
			SFUI_DECL_METHOD( GetActiveSeasionIndexValue ),
			{ NULL, NULL }
		};
		return table;
	}
};

static CScaleformComponentGameTypes g_ScaleformComponentGameTypes;
static CScaleformComponentGameTypes_Table g_ScaleformComponentGameTypesTable;

// Installs the component into the full-screen slot (menus and dialogs) once
// that slot exists; the slot lives for the whole run.
void ScaleformComponentGameTypes_EnsureInstalled()
{
	static SFVALUE s_installed = NULL;
	if ( s_installed || !g_pScaleformUI )
		return;

	g_pScaleformUI->InstallGlobalObject( SF_FULL_SCREEN_SLOT, "CScaleformComponent_GameTypes",
		&g_ScaleformComponentGameTypes, &g_ScaleformComponentGameTypesTable, &s_installed );

	if ( s_installed )
	{
		printf( "[sf] installed CScaleformComponent_GameTypes\n" );
		fflush( stdout );
	}
}

#endif // INCLUDE_SCALEFORM
