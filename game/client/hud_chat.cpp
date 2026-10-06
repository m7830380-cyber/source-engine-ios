//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: HUD chat for games without their own (see hud_chat.h)
//
//=============================================================================//

#include "cbase.h"
#include "hud_chat.h"
#include "hud_macros.h"
#include "text_message.h"
#include "vguicenterprint.h"
#include "hud_basechat.h"
#include <vgui/ILocalize.h>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


DECLARE_HUDELEMENT_FLAGS( CHudChat, HUDELEMENT_SS_FULLSCREEN_ONLY );

//=====================
//CHudChat
//=====================

CHudChat::CHudChat( const char *pElementName ) : BaseClass( pElementName )
{
}

void CHudChat::Init( void )
{
	BaseClass::Init();

	m_UMCMsgSayText.Bind< CS_UM_SayText, CCSUsrMsg_SayText >( UtlMakeDelegate( this, static_cast< bool ( CBaseHudChat::* )( const CCSUsrMsg_SayText & ) >( &CBaseHudChat::MsgFunc_SayText ) ) );
	m_UMCMsgSayText2.Bind< CS_UM_SayText2, CCSUsrMsg_SayText2 >( UtlMakeDelegate( this, static_cast< bool ( CBaseHudChat::* )( const CCSUsrMsg_SayText2 & ) >( &CBaseHudChat::MsgFunc_SayText2 ) ) );
	m_UMCMsgTextMsg.Bind< CS_UM_TextMsg, CCSUsrMsg_TextMsg >( UtlMakeDelegate( this, static_cast< bool ( CBaseHudChat::* )( const CCSUsrMsg_TextMsg & ) >( &CBaseHudChat::MsgFunc_TextMsg ) ) );
}
