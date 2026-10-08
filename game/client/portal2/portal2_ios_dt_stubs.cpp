#include "cbase.h"
#include "c_weapon__stubs.h"
#include "cstrike15/basecsgrenade_projectile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

// Server ships these from h_cycler / weapon_cubemap; CS clientmode registered them.
STUB_WEAPON_CLASS( cycler_weapon, WeaponCycler, C_BaseCombatWeapon );
STUB_WEAPON_CLASS( weapon_cubemap, WeaponCubemap, C_BaseCombatWeapon );

// Match portal2_cs_grenade_server_stub.cpp SendTable (not full CS grenade client).
IMPLEMENT_NETWORKCLASS_ALIASED( BaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile )

BEGIN_NETWORK_TABLE( CBaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile )
	RecvPropVector( RECVINFO( m_vInitialVelocity ) ),
	RecvPropInt( RECVINFO( m_nBounces ) )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS_ALIASED( basecsgrenade_projectile, BaseCSGrenadeProjectile );

CBaseCSGrenadeProjectile::~CBaseCSGrenadeProjectile()
{
}

void CBaseCSGrenadeProjectile::Spawn()
{
	BaseClass::Spawn();
}

int CBaseCSGrenadeProjectile::DrawModel( int flags, const RenderableInstance_t &instance )
{
	return BaseClass::DrawModel( flags, instance );
}

void CBaseCSGrenadeProjectile::PostDataUpdate( DataUpdateType_t type )
{
	BaseClass::PostDataUpdate( type );
}

void CBaseCSGrenadeProjectile::ClientThink( void )
{
}

void CBaseCSGrenadeProjectile::CreateGrenadeTrail( void )
{
}

#endif
