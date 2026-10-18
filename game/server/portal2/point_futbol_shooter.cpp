#include "cbase.h"
#include "prop_exploding_futbol.h"
#include "movevars_shared.h"
#include "tier0/memdbgon.h"

// Launches prop_exploding_futbols at a target on an arc (Wheatley's bombs in sp_a4_finale4).
// launchSpeed is the horizontal speed; the vertical speed is solved so the futbol lands on the target.

class CPointFutbolShooter : public CPointEntity
{
public:
	DECLARE_CLASS(CPointFutbolShooter, CPointEntity);
	DECLARE_DATADESC();

	CPointFutbolShooter();
	void Precache();
	void Spawn();

private:
	void ShootFutbol();
	Vector GetLaunchVelocity(const Vector& vecStart);
	void InputSetTarget(inputdata_t& inputdata);
	void InputShootFutbol(inputdata_t& inputdata);

	float m_flLaunchSpeed;
	EHANDLE m_hTarget;
	COutputEvent m_OnShootSuccess;
	COutputEvent m_OnShootFail;
};

LINK_ENTITY_TO_CLASS(point_futbol_shooter, CPointFutbolShooter);

BEGIN_DATADESC(CPointFutbolShooter)
	DEFINE_KEYFIELD(m_flLaunchSpeed, FIELD_FLOAT, "launchSpeed"),
	DEFINE_FIELD(m_hTarget, FIELD_EHANDLE),

	DEFINE_INPUTFUNC(FIELD_STRING, "SetTarget", InputSetTarget),
	DEFINE_INPUTFUNC(FIELD_VOID, "ShootFutbol", InputShootFutbol),

	DEFINE_OUTPUT(m_OnShootSuccess, "OnShootSuccess"),
	DEFINE_OUTPUT(m_OnShootFail, "OnShootFail"),
END_DATADESC()

CPointFutbolShooter::CPointFutbolShooter()
{
	m_flLaunchSpeed = 200.0f;
}

void CPointFutbolShooter::Precache()
{
	BaseClass::Precache();
	UTIL_PrecacheOther("prop_exploding_futbol");
}

void CPointFutbolShooter::Spawn()
{
	Precache();
	BaseClass::Spawn();
}

void CPointFutbolShooter::InputSetTarget(inputdata_t& inputdata)
{
	m_hTarget = gEntList.FindEntityByName(NULL, inputdata.value.String(), this, inputdata.pActivator, inputdata.pCaller);
}

void CPointFutbolShooter::InputShootFutbol(inputdata_t& inputdata)
{
	ShootFutbol();
}

Vector CPointFutbolShooter::GetLaunchVelocity(const Vector& vecStart)
{
	const float flSpeed = MAX(m_flLaunchSpeed, 1.0f);

	CBaseEntity* pTarget = m_hTarget.Get();
	if (!pTarget)
	{
		Vector vecForward;
		AngleVectors(GetAbsAngles(), &vecForward);
		return vecForward * flSpeed;
	}

	const float flGravity = GetCurrentGravity();

	const Vector vecDelta = pTarget->WorldSpaceCenter() - vecStart;
	const float flHorizontal = vecDelta.Length2D();

	// Time in the air at launchSpeed; keep a floor so a target straight below still gets a sane arc
	const float flTime = MAX(flHorizontal / flSpeed, 0.25f);

	Vector vecVelocity;
	vecVelocity.x = vecDelta.x / flTime;
	vecVelocity.y = vecDelta.y / flTime;
	vecVelocity.z = vecDelta.z / flTime + 0.5f * flGravity * flTime;
	return vecVelocity;
}

void CPointFutbolShooter::ShootFutbol()
{
	CPropExplodingFutbol* pFutbol = static_cast<CPropExplodingFutbol*>(CreateEntityByName("prop_exploding_futbol"));
	if (!pFutbol)
	{
		m_OnShootFail.FireOutput(this, this);
		return;
	}

	const Vector vecStart = GetAbsOrigin();
	pFutbol->SetAbsOrigin(vecStart);
	pFutbol->SetAbsAngles(GetAbsAngles());

	// Don't collide with whatever is carrying the shooter (Wheatley)
	CBaseEntity* pLauncher = GetRootMoveParent();
	pFutbol->SetOwnerEntity(pLauncher != this ? pLauncher : NULL);

	if (DispatchSpawn(pFutbol) < 0)
	{
		UTIL_Remove(pFutbol);
		m_OnShootFail.FireOutput(this, this);
		return;
	}
	pFutbol->Activate();

	IPhysicsObject* pPhys = pFutbol->VPhysicsGetObject();
	if (!pPhys)
	{
		UTIL_Remove(pFutbol);
		m_OnShootFail.FireOutput(this, this);
		return;
	}

	Vector vecVelocity = GetLaunchVelocity(vecStart);
	AngularImpulse angVelocity(RandomFloat(-200.0f, 200.0f), RandomFloat(-200.0f, 200.0f), RandomFloat(-200.0f, 200.0f));
	pPhys->Wake();
	pPhys->SetVelocity(&vecVelocity, &angVelocity);

	pFutbol->ReleaseFromSpawner(0.25f);

#if defined( IOS )
	{
		CBaseEntity* pTarget = m_hTarget.Get();
		Msg("[futbol] %s shot #%d from %.0f %.0f %.0f vel %.0f %.0f %.0f at %s, t=%.2f\n", GetDebugName(), pFutbol->entindex(),
			vecStart.x, vecStart.y, vecStart.z, vecVelocity.x, vecVelocity.y, vecVelocity.z,
			pTarget ? pTarget->GetDebugName() : "(forward)", gpGlobals->curtime);
	}
#endif

	m_OnShootSuccess.FireOutput(pFutbol, this);
}
