#include "cbase.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

#include "portal2/gameui/portal2/basemodframe.h"
#include "portal2/gameui/portal2/gamemodes.h"
#include "portal2/gameui/portal2/uigamedata.h"
#include "portal2/gameui/portal2/vdropdownmenu.h"
#include "portal2/gameui/portal2/vslidercontrol.h"
#include "portal2/gameui/portal2/vgenericconfirmation.h"
#include "portal2/gameui/portal2/vgenericpanellist.h"
#include "portal2/gameui/portal2/vgameoptions.h"
#include "portal2/gameui/portal2/vleaderboard.h"
#include "portal2/gameui/portal2/vvoteoptions.h"
#include "portal2/gameui/portal2/vachievements.h"
#include "portal2/gameui/portal2/vsignindialog.h"
#include "portal2/gameui/portal2/vgetlegacydata.h"
#include "portal2/gameui/portal2/vpasswordentry.h"
#include "portal2/gameui/portal2/vattractscreen.h"
#include "portal2/gameui/portal2/vcustomcampaigns.h"
#include "portal2/gameui/portal2/vfoundgroupgames.h"
#include "portal2/gameui/portal2/vaddonassociation.h"
#include "portal2/gameui/portal2/vdownloadcampaign.h"
#include "portal2/gameui/portal2/vfoundpublicgames.h"
#include "portal2/gameui/portal2/vtransitionscreen.h"
#include "portal2/gameui/portal2/vcontrolleroptions.h"
#include "portal2/gameui/portal2/vcontrolleroptionssticks.h"
#include "portal2/gameui/portal2/vcontrolleroptionsbuttons.h"
#include "portal2/gameui/portal2/vaddons.h"
#include "portal2/gameui/portal2/voptions.h"
#include "portal2/gameui/portal2/vdownloads.h"
#include "portal2/gameui/portal2/vkeyboard.h"
#include "portal2/gameui/portal2/vingamechapterselect.h"
#include "portal2/gameui/portal2/vingamekickplayerlist.h"
#include "portal2/gameui/portal2/vingamedifficultyselect.h"
#include "portal2/gameui/portal2/vsteamcloudconfirmation.h"
namespace BaseModUI
{

static CUIGameData s_UIGameData;

CUIGameData *CUIGameData::Get()
{
	return &s_UIGameData;
}

void CUIGameData::RunFrame()
{
}

void CUIGameData::Shutdown()
{
}

bool CUIGameData::CheckAndDisplayErrorIfOffline( CBaseModFrame *pCallerFrame, char const *szMsg )
{
	(void)pCallerFrame;
	(void)szMsg;
	return false;
}

bool CUIGameData::CheckAndDisplayErrorIfNotSignedInToLive( CBaseModFrame *pCallerFrame )
{
	(void)pCallerFrame;
	return false;
}

GameOptions::GameOptions( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

Leaderboard::Leaderboard( vgui::Panel *parent )
	: BaseClass( parent, "Leaderboard" )
{
}

VoteOptions::VoteOptions( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

Achievements::Achievements( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

SignInDialog::SignInDialog( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

GetLegacyData::GetLegacyData( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

PasswordEntry::PasswordEntry( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

CAttractScreen::CAttractScreen( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

void CAttractScreen::SetAttractMode( AttractMode_t mode, int iPlaylist )
{
	(void)mode;
	(void)iPlaylist;
}

CustomCampaigns::CustomCampaigns( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

FoundGroupGames::FoundGroupGames( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

bool AddonAssociation::CheckAndSeeIfShouldShow()
{
	return false;
}

AddonAssociation::AddonAssociation( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

DownloadCampaign::DownloadCampaign( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

FoundPublicGames::FoundPublicGames( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

CTransitionScreen::CTransitionScreen( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

ControllerOptions::ControllerOptions( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

ControllerOptionsSticks::ControllerOptionsSticks( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

ControllerOptionsButtons::ControllerOptionsButtons( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

Addons::Addons( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

Options::Options( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

Downloads::Downloads( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

VKeyboard::VKeyboard( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

InGameChapterSelect::InGameChapterSelect( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

InGameKickPlayerList::InGameKickPlayerList( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

InGameDifficultySelect::InGameDifficultySelect( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

SteamCloudConfirmation::SteamCloudConfirmation( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

CChangeStorageDevice::CChangeStorageDevice( int iCtrlrIndex )
{
	(void)iCtrlrIndex;
}

void DropDownMenu::CloseDropDown()
{
}

void DropDownMenu::SetOpenCallback( void ( *pfnOpen )( DropDownMenu *, FlyoutMenu * ) )
{
	(void)pfnOpen;
}

FlyoutMenu *DropDownMenu::GetCurrentFlyout()
{
	return NULL;
}

void DropDownMenu::SetCurrentSelection( char const *szSelection )
{
	(void)szSelection;
}

float SliderControl::GetCurrentValue()
{
	return 0.0f;
}

void SliderControl::SetCurrentValue( float flValue, bool bSetSlider )
{
	(void)flValue;
	(void)bSetSlider;
}

void SliderControl::ResetSliderPosAndDefaultMarkers()
{
}

void SliderControl::Reset()
{
}

bool GenericPanelList::GetPanelItemIndex( vgui::Panel *pPanel, unsigned short &index )
{
	(void)pPanel;
	index = 0;
	return false;
}

int GenericConfirmation::SetUsageData( Data_t const &data )
{
	(void)data;
	return 0;
}

GenericConfirmation::Data_t::Data_t()
{
}

GenericConfirmation::GenericConfirmation( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, panelName )
{
}

} // namespace BaseModUI

GameModes::GameModes( vgui::Panel *pParent, const char *pName )
	: BaseClass( pParent, pName, "" )
{
}

GameModes::~GameModes()
{
}

BaseModUI::BaseModHybridButton *GameModes::GetHybridButton( int index )
{
	(void)index;
	return NULL;
}

int GameModes::GetNumGameInfos()
{
	return 0;
}

#endif
