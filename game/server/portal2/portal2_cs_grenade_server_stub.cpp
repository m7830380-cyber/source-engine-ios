#include "cbase.h"
#include "cstrike15/basecsgrenade_projectile.h"

#if defined( PORTAL2 ) && defined( GAME_DLL )

IMPLEMENT_NETWORKCLASS_ALIASED( BaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile )

BEGIN_NETWORK_TABLE( CBaseCSGrenadeProjectile, DT_BaseCSGrenadeProjectile )
	SendPropVector( SENDINFO( m_vInitialVelocity ), 20, 0, -3000, 3000 ),
	SendPropInt( SENDINFO( m_nBounces ) )
END_NETWORK_TABLE()

BEGIN_DATADESC( CBaseCSGrenadeProjectile )
END_DATADESC()

LINK_ENTITY_TO_CLASS( basecsgrenade_projectile, CBaseCSGrenadeProjectile );

void CBaseCSGrenadeProjectile::PostConstructor( const char *className )
{
	(void)className;
	BaseClass::PostConstructor( className );
}

CBaseCSGrenadeProjectile::~CBaseCSGrenadeProjectile()
{
}

void CBaseCSGrenadeProjectile::Spawn()
{
	BaseClass::Spawn();
}

void CBaseCSGrenadeProjectile::Precache()
{
	BaseClass::Precache();
}

int CBaseCSGrenadeProjectile::UpdateTransmitState()
{
	return BaseClass::UpdateTransmitState();
}

int CBaseCSGrenadeProjectile::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	return BaseClass::ShouldTransmit( pInfo );
}

void CBaseCSGrenadeProjectile::Explode( trace_t *pTrace, int bitsDamageType )
{
	(void)pTrace;
	(void)bitsDamageType;
	UTIL_Remove( this );
}

void CBaseCSGrenadeProjectile::Splash()
{
}

unsigned int CBaseCSGrenadeProjectile::PhysicsSolidMaskForEntity( void ) const
{
	return BaseClass::PhysicsSolidMaskForEntity();
}

void CBaseCSGrenadeProjectile::ResolveFlyCollisionCustom( CGameTrace &tr, Vector &vecVelocity )
{
	BaseClass::ResolveFlyCollisionCustom( tr, vecVelocity );
}

#endif
