#include "cbase.h"

#if defined( PORTAL2 ) && defined( GAME_DLL )

#include "player.h"
#include "pushentity.h"
#include "hl2/weapon_rpg.h"
#include "hl2/HL2_Player.h"
#include "dedicated_server_ugc_manager.h"
#include "networkstringtabledefs.h"

INetworkStringTable *g_pStringTableIOSAvatars = NULL;

static CPhysicsPushedEntities s_Portal2PushedEntities;
CPhysicsPushedEntities *g_pPushedEntities = &s_Portal2PushedEntities;

int g_interactionCombineBash = 0;

ConVar sv_coaching_enabled( "sv_coaching_enabled", "0", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar sv_deadtalk( "sv_deadtalk", "0", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar sv_alternateticks( "sv_alternateticks", "0", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar sv_robust_explosions( "sv_robust_explosions", "0", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar sv_server_graphic1( "sv_server_graphic1", "", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar sv_server_graphic2( "sv_server_graphic2", "", FCVAR_REPLICATED | FCVAR_RELEASE );
ConVar mp_verbose_changelevel_spew( "mp_verbose_changelevel_spew", "0", FCVAR_RELEASE );

const char *ChangeLevel_DestinationMapName( void )
{
	return "";
}

const char *ChangeLevel_OriginMapName( void )
{
	return "";
}

const char *ChangeLevel_GetLandmarkName( void )
{
	return "";
}

void LoadEquipmentData( void )
{
}

CBaseEntity *FindPickerEntityClass( CBasePlayer *pPlayer, char *classname )
{
	if ( !pPlayer )
	{
		return NULL;
	}
	return pPlayer->FindPickerEntityClass( classname );
}

extern CBasePlayer *GetPlayerHoldingEntity( CBaseEntity *pEntity );

CBasePlayer *GetPlayerHoldingEntity( const CBaseEntity *pEntity )
{
	return GetPlayerHoldingEntity( const_cast<CBaseEntity *>( pEntity ) );
}

void CMissile::DumbFire( void )
{
}

CBaseEntity *CreateLaserDot( const Vector &origin, CBaseEntity *pOwner, bool bVisibleDot )
{
	(void)origin;
	(void)pOwner;
	(void)bVisibleDot;
	return NULL;
}

void EnableLaserDot( CBaseEntity *pLaser, bool bActive )
{
	(void)pLaser;
	(void)bActive;
}

void SetLaserDotTarget( CBaseEntity *pLaser, CBaseEntity *pTarget )
{
	(void)pLaser;
	(void)pTarget;
}

#include "portal/portal_base2d.h"

void AddPortalVisibilityToPVS( CProp_Portal *pPortal, int pvssize, unsigned char *pvs )
{
	AddPortalVisibilityToPVS( static_cast<CPortal_Base2D *>( pPortal ), pvssize, pvs );
}

bool IsPlayerNearTargetPortal( CProp_Portal *pPortal )
{
	return IsPlayerNearTargetPortal( static_cast<CPortal_Base2D *>( pPortal ) );
}

void CHL2_Player::StopSprinting( void )
{
}

unsigned int CMissile::PhysicsSolidMaskForEntity( void ) const
{
	return BaseClass::PhysicsSolidMaskForEntity();
}

CAPCMissile *CAPCMissile::Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, CBaseEntity *pOwner )
{
	(void)vecOrigin;
	(void)vecAngles;
	(void)vecVelocity;
	(void)pOwner;
	return NULL;
}

CAPCMissile::CAPCMissile()
{
}

CAPCMissile::~CAPCMissile()
{
}

void CAPCMissile::IgniteDelay()
{
}

void CAPCMissile::AugerDelay( float )
{
}

void CAPCMissile::ExplodeDelay( float )
{
}

static CDedicatedServerWorkshopManager g_Portal2DedicatedServerWorkshopManager;

CDedicatedServerWorkshopManager &DedicatedServerWorkshop( void )
{
	return g_Portal2DedicatedServerWorkshopManager;
}

bool CDedicatedServerWorkshopManager::Init( void )
{
	return true;
}

void CDedicatedServerWorkshopManager::Shutdown( void )
{
	Cleanup();
}

void CDedicatedServerWorkshopManager::LevelInitPreEntity( void )
{
}

void CDedicatedServerWorkshopManager::GetNewestSubscribedFiles( void )
{
}

bool CDedicatedServerWorkshopManager::GetMapsMatchingName( const char *, CUtlVector<const DedicatedServerUGCFileInfo_t *> & ) const
{
	return false;
}

PublishedFileId_t CDedicatedServerWorkshopManager::GetUGCMapPublishedFileID( const char * ) const
{
	return 0;
}

const char *CDedicatedServerWorkshopManager::GetUGCMapPath( PublishedFileId_t ) const
{
	return NULL;
}

void CDedicatedServerWorkshopManager::Update( void )
{
}

const CUtlVector<PublishedFileId_t> &CDedicatedServerWorkshopManager::GetWorkshopMapList( void ) const
{
	static CUtlVector<PublishedFileId_t> s_EmptyList;
	return s_EmptyList;
}

void CDedicatedServerWorkshopManager::HostWorkshopMap( PublishedFileId_t )
{
}

void CDedicatedServerWorkshopManager::HostWorkshopMapCollection( PublishedFileId_t )
{
}

bool CDedicatedServerWorkshopManager::HasPendingMapDownloads( void ) const
{
	return false;
}

void CDedicatedServerWorkshopManager::CheckForNewVersion( PublishedFileId_t )
{
}

void CDedicatedServerWorkshopManager::CheckIfCurrentLevelNeedsUpdate( void )
{
}

bool CDedicatedServerWorkshopManager::CurrentLevelNeedsUpdate( void ) const
{
	return false;
}

void CDedicatedServerWorkshopManager::GetWorkshopMasWithValidUgcInformation( CUtlVector<const DedicatedServerUGCFileInfo_t *> & ) const
{
}

void CDedicatedServerWorkshopManager::Cleanup( void )
{
}

#endif
