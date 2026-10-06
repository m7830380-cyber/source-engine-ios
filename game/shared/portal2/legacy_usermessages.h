//====== Portal 2 on the CS:GO engine (iOS) ======
//
// Purpose: Portal 2's named bf_write user messages on top of the CS:GO
//			engine's protobuf user message channel.
//
//			Server: UserMessageBegin( filter, "Name" ), WRITE_*, MessageEnd()
//			(game/server/gameinterface.cpp) sends a CP2UsrMsg_Legacy of
//			type P2_UM_Legacy.
//			Client: LEGACY_USER_MESSAGE_REGISTER( Name ) / usermessages->HookMessage
//			hook a void __MsgFunc_Name( bf_read &msg ) handler, called with
//			the bits the server wrote.
//
//==================================================

#ifndef LEGACY_USERMESSAGES_H
#define LEGACY_USERMESSAGES_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utldict.h"
#include "tier1/utlvector.h"
#include "tier1/bitbuf.h"

// wire type of CP2UsrMsg_Legacy, above every ECstrike15UserMessages value
#define P2_UM_Legacy	200

// largest message body (bytes)
#define P2_LEGACY_USERMSG_MAX	4096

typedef void (*pfnUserMsgHook)( bf_read &msg );

class CLegacyUserMessages
{
public:
	CLegacyUserMessages();
	~CLegacyUserMessages();

	// Returns -1 if not found, otherwise the message index
	int		LookupUserMessage( const char *name );
	int		GetUserMessageSize( int index );
	const char *GetUserMessageName( int index );
	bool	IsValidIndex( int index );

	// Server: declares a message (size is the byte size, -1 for variable)
	void	Register( const char *name, int size );

	// Client
	void	HookMessage( const char *name, pfnUserMsgHook hook );
	bool	DispatchUserMessage( const char *name, bf_read &msg_data );

private:
	struct Message_t
	{
		int size;
		CUtlVector< pfnUserMsgHook > hooks;
	};
	int		FindOrAdd( const char *name );
	CUtlDict< Message_t*, int > m_Messages;
};

extern CLegacyUserMessages *usermessages;

// Server and client: declare Portal 2's messages (portal_usermessages.cpp)
void RegisterUserMessages( void );

#ifdef CLIENT_DLL
// Collects USER_MESSAGE_REGISTER handlers until the client hooks them up
class CLegacyUserMessageRegister
{
public:
	CLegacyUserMessageRegister( const char *pMessageName, pfnUserMsgHook pfnHook );

	// Hooks every registered handler (once)
	static void RegisterAll();

private:
	const char *m_pMessageName;
	pfnUserMsgHook m_pfnHook;
	CLegacyUserMessageRegister *m_pNext;
	static CLegacyUserMessageRegister *s_pHead;
};

#define LEGACY_USER_MESSAGE_REGISTER( msgName ) \
	static CLegacyUserMessageRegister legacyUserMessageRegister_##msgName( #msgName, __MsgFunc_##msgName );
#endif

#endif // LEGACY_USERMESSAGES_H
