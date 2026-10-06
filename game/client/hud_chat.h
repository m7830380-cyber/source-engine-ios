//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: HUD chat for games without their own (Portal 2 on the CS:GO engine:
//			the SayText/SayText2/TextMsg user messages are CS:GO's protobufs,
//			handled by CBaseHudChat).
//
//=============================================================================//

#ifndef HUD_CHAT_H
#define HUD_CHAT_H
#ifdef _WIN32
#pragma once
#endif

#include <hud_basechat.h>

class CHudChat : public CBaseHudChat
{
	DECLARE_CLASS_SIMPLE( CHudChat, CBaseHudChat );

public:
	explicit CHudChat( const char *pElementName );

	virtual void	Init( void );

private:
	CUserMessageBinder m_UMCMsgSayText;
	CUserMessageBinder m_UMCMsgSayText2;
	CUserMessageBinder m_UMCMsgTextMsg;
};

#endif	//HUD_CHAT_H
