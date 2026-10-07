#include "cbase.h"
#include "hl2/weapon_rpg.h"

#if defined( PORTAL2 ) && defined( GAME_DLL )

CUtlVector<CMissile::CustomDetonator_t> CMissile::gm_CustomDetonators;

BEGIN_DATADESC( CMissile )
END_DATADESC()

CMissile::CMissile()
{
	m_flDamage = 100.0f;
	m_bCreateDangerSounds = false;
	m_flGracePeriodEndsAt = 0.0f;
	m_flAugerTime = 0.0f;
	m_flMarkDeadTime = 0.0f;
}

CMissile::~CMissile()
{
}

void CMissile::Spawn()
{
	BaseClass::Spawn();
}

void CMissile::Precache()
{
	BaseClass::Precache();
}

void CMissile::MissileTouch( CBaseEntity *pOther )
{
	(void)pOther;
	Explode();
}

void CMissile::Explode()
{
	UTIL_Remove( this );
}

void CMissile::ShotDown()
{
	Explode();
}

void CMissile::AccelerateThink()
{
}

void CMissile::AugerThink()
{
}

void CMissile::IgniteThink()
{
}

void CMissile::SeekThink()
{
}

void CMissile::DumbFire()
{
}

void CMissile::SetGracePeriod( float flGracePeriod )
{
	m_flGracePeriodEndsAt = gpGlobals->curtime + flGracePeriod;
}

int CMissile::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	return BaseClass::OnTakeDamage_Alive( info );
}

void CMissile::Event_Killed( const CTakeDamageInfo &info )
{
	BaseClass::Event_Killed( info );
}

unsigned int CMissile::PhysicsSolidMaskForEntity( void ) const
{
	return BaseClass::PhysicsSolidMaskForEntity();
}

CMissile *CMissile::Create( const Vector &vecOrigin, const QAngle &vecAngles, edict_t *pentOwner )
{
	CMissile *pMissile = CREATE_ENTITY( CMissile, "rpg_missile" );
	if ( !pMissile )
	{
		return NULL;
	}

	pMissile->SetAbsOrigin( vecOrigin );
	pMissile->SetAbsAngles( vecAngles );
	pMissile->SetOwnerEntity( Instance( pentOwner ) );
	DispatchSpawn( pMissile );
	return pMissile;
}

void CMissile::AddCustomDetonator( CBaseEntity *pEntity, float radius, float height )
{
	(void)pEntity;
	(void)radius;
	(void)height;
}

void CMissile::RemoveCustomDetonator( CBaseEntity *pEntity )
{
	(void)pEntity;
}

void CMissile::DoExplosion()
{
	Explode();
}

void CMissile::ComputeActualDotPosition( CLaserDot *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed )
{
	(void)pLaserDot;
	if ( pActualDotPosition )
	{
		pActualDotPosition->Init();
	}
	if ( pHomingSpeed )
	{
		*pHomingSpeed = 0.0f;
	}
}

void CMissile::CreateSmokeTrail()
{
}

void CMissile::GetShootPosition( CLaserDot *pLaserDot, Vector *pShootPosition )
{
	(void)pLaserDot;
	if ( pShootPosition )
	{
		*pShootPosition = GetAbsOrigin();
	}
}

BEGIN_DATADESC( CAPCMissile )
END_DATADESC()

CAPCMissile *CAPCMissile::Create( const Vector &vecOrigin, const QAngle &vecAngles, const Vector &vecVelocity, CBaseEntity *pOwner )
{
	CAPCMissile *pMissile = CREATE_ENTITY( CAPCMissile, "apc_missile" );
	if ( !pMissile )
	{
		return NULL;
	}

	pMissile->SetAbsOrigin( vecOrigin );
	pMissile->SetAbsAngles( vecAngles );
	pMissile->SetOwnerEntity( pOwner );
	pMissile->SetAbsVelocity( vecVelocity );
	DispatchSpawn( pMissile );
	return pMissile;
}

CAPCMissile::CAPCMissile()
{
	m_pNext = NULL;
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

void CAPCMissile::DisableGuiding()
{
}

void CAPCMissile::AimAtSpecificTarget( CBaseEntity *pTarget )
{
	(void)pTarget;
}

void CAPCMissile::SetGuidanceHint( const char *pHintName )
{
	(void)pHintName;
}

void CAPCMissile::APCSeekThink()
{
}

void CAPCMissile::DoExplosion()
{
	Explode();
}

void CAPCMissile::ComputeActualDotPosition( CLaserDot *pLaserDot, Vector *pActualDotPosition, float *pHomingSpeed )
{
	BaseClass::ComputeActualDotPosition( pLaserDot, pActualDotPosition, pHomingSpeed );
}

int CAPCMissile::AugerHealth()
{
	return 1;
}

void CAPCMissile::Init()
{
}

void CAPCMissile::ComputeLeadingPosition( const Vector &vecShootPosition, CBaseEntity *pTarget, Vector *pLeadPosition )
{
	(void)vecShootPosition;
	(void)pTarget;
	if ( pLeadPosition )
	{
		pLeadPosition->Init();
	}
}

void CAPCMissile::BeginSeekThink()
{
}

void CAPCMissile::AugerStartThink()
{
}

void CAPCMissile::ExplodeThink()
{
}

void CAPCMissile::APCMissileTouch( CBaseEntity *pOther )
{
	(void)pOther;
}

CAPCMissile *FindAPCMissileInCone( const Vector &vecOrigin, const Vector &vecDirection, float flAngle )
{
	(void)vecOrigin;
	(void)vecDirection;
	(void)flAngle;
	return NULL;
}

#endif
