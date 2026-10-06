#include "cbase.h"
#include "hltvreplaysystem.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

CHltvReplaySystem g_HltvReplaySystem;

CHltvReplaySystem::CLocalPlayerProps::CLocalPlayerProps()
	: m_bLastSeenAlive( false ), m_nLastTickUpdated( 0 )
{
}

bool CHltvReplaySystem::CLocalPlayerProps::Update()
{
	return false;
}

CHltvReplaySystem::CHltvReplaySystem()
{
	m_nHltvReplayDelay = 0;
	m_bDemoPlayback = false;
	m_bWaitingForHltvReplayTick = false;
	m_bHltvReplayButtonTimedOut = false;
	m_bListeningForGameEvents = false;
	m_nExperimentalEvents = 0;
	m_flReplayVideoFadeAmount = 0.0f;
	m_flReplaySoundFadeAmount = 0.0f;
	m_flFadeinStartRealTime = -1000.0f;
	m_flFadeoutEndTime = -1000.0f;
	m_DelayedReplay.Reset();
}

CHltvReplaySystem::~CHltvReplaySystem()
{
}

void CHltvReplaySystem::OnHltvReplay( const CSVCMsg_HltvReplay & )
{
}

void CHltvReplaySystem::OnHltvReplayTick()
{
}

void CHltvReplaySystem::EmitTimeJump()
{
}

void CHltvReplaySystem::StopHltvReplay()
{
}

float CHltvReplaySystem::GetHltvReplayDistortAmount() const
{
	return 0.0f;
}

bool CHltvReplaySystem::IsHltvReplayButtonEnabled()
{
	return false;
}

bool CHltvReplaySystem::IsHltvReplayButtonTimedOut()
{
	return m_bHltvReplayButtonTimedOut;
}

void CHltvReplaySystem::RequestHltvReplayDeath()
{
}

void CHltvReplaySystem::RequestHltvReplay( const ReplayParams_t & )
{
}

void CHltvReplaySystem::RequestCancelHltvReplay( bool )
{
}

bool CHltvReplaySystem::IsHltvReplayFeatureEnabled()
{
	return false;
}

bool CHltvReplaySystem::IsFadeoutFinished()
{
	return true;
}

bool CHltvReplaySystem::IsFadeoutActive()
{
	return false;
}

bool CHltvReplaySystem::PrepareHltvReplayCountdown()
{
	return false;
}

void CHltvReplaySystem::CancelDelayedHltvReplay()
{
}

void CHltvReplaySystem::OnLevelInit()
{
}

void CHltvReplaySystem::OnPlayerDeath( IGameEvent * )
{
}

bool CHltvReplaySystem::WantsReplayEffect() const
{
	return false;
}

void CHltvReplaySystem::OnDemoPlayback( bool bPlaying )
{
	m_bDemoPlayback = bPlaying;
}

void CHltvReplaySystem::SetDemoPlaybackHighlightXuid( uint64, bool )
{
}

void CHltvReplaySystem::SetDemoPlaybackFadeBrackets( int, int, int )
{
}

void CHltvReplaySystem::FireGameEvent( IGameEvent * )
{
}

void CHltvReplaySystem::OnLevelShutdown()
{
}

void CHltvReplaySystem::OnLocalPlayerRespawning()
{
}

void CHltvReplaySystem::StopFades()
{
}

void CHltvReplaySystem::StopFadeout()
{
}

void CHltvReplaySystem::StartFadeout( float, float )
{
}

void CHltvReplaySystem::StartFadein( float )
{
}

void CHltvReplaySystem::SetReplaySoundMixLayer( float )
{
}

C_BasePlayer *CHltvReplaySystem::GetDemoPlaybackPlayer()
{
	return NULL;
}

bool CHltvReplaySystem::IsDemoPlaybackXuidOther() const
{
	return false;
}

void CHltvReplaySystem::Update()
{
}

void CHltvReplaySystem::PurgeRagdollBoneCache()
{
}

void CHltvReplaySystem::CacheRagdollBones()
{
}

CachedRagdollBones_t *CHltvReplaySystem::GetCachedRagdollBones( int, bool )
{
	return NULL;
}

void CHltvReplaySystem::FreeCachedRagdollBones( CachedRagdollBones_t * )
{
}

bool CHltvReplaySystem::UpdateHltvReplayButtonTimeOutState()
{
	return false;
}

float CHltvReplaySystem::GetReplayMessageTime()
{
	return 0.0f;
}

int CL_GetHltvReplayDelay()
{
	return g_HltvReplaySystem.GetHltvReplayDelay();
}

#endif
