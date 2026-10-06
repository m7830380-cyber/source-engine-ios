#include "cbase.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

#include "achievementmgr.h"

static CAchievementMgr g_AchievementMgrPortal2;
static CAchievementMgrDelegateIAchievementMgr g_IAchievementMgrPortal2( &g_AchievementMgrPortal2 );

CAchievementMgr *CAchievementMgr::GetInstance()
{
	return &g_AchievementMgrPortal2;
}

IAchievementMgr *CAchievementMgr::GetInstanceInterface()
{
	return &g_IAchievementMgrPortal2;
}

#endif
