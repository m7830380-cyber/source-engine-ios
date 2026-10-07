#include "cbase.h"
#include "cstrike15/basecsgrenade_projectile.h"

#if defined( PORTAL2 ) && defined( GAME_DLL )

IMPLEMENT_NETWORKCLASS_ALIASED( BaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile )

BEGIN_NETWORK_TABLE( CBaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile )
	SendPropVector( SENDINFO( m_vInitialVelocity ), 20, 0, -3000, 3000 ),
	SendPropInt( SENDINFO( m_nBounces ) )
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( basecsgrenade_projectile, CBaseCSGrenadeProjectile );

void CBaseCSGrenadeProjectile::Spawn()
{
	BaseClass::Spawn();
}

unsigned int CBaseCSGrenadeProjectile::PhysicsSolidMaskForEntity( void ) const
{
	return BaseClass::PhysicsSolidMaskForEntity();
}

#endif
