#include "cbase.h"
#include "vote_controller.h"

IMPLEMENT_SERVERCLASS_ST( CVoteController, DT_VoteController )
	SendPropInt( SENDINFO( m_iActiveIssueIndex ) ),
	SendPropInt( SENDINFO( m_iOnlyTeamToVote ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_nVoteOptionCount ), SendPropInt( SENDINFO_ARRAY( m_nVoteOptionCount ), 8, SPROP_UNSIGNED ) ),
	SendPropInt( SENDINFO( m_nPotentialVotes ) ),
	SendPropBool( SENDINFO( m_bIsYesNoVote ) )
END_SEND_TABLE()

BEGIN_DATADESC( CVoteController )
END_DATADESC()

LINK_ENTITY_TO_CLASS( vote_controller, CVoteController );

CVoteController *g_voteControllerGlobal = NULL;
CVoteController *g_voteControllerCT = NULL;
CVoteController *g_voteControllerT = NULL;
