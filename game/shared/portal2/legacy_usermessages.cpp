//====== Portal 2 on the CS:GO engine (iOS) ======
//
// Purpose: Portal 2's named bf_write user messages on top of the CS:GO
//			engine's protobuf user message channel (see legacy_usermessages.h).
//
//==================================================

#include "cbase.h"
#include "legacy_usermessages.h"
#include "portal2_usermessages.pb.h"
#ifdef CLIENT_DLL
#include "usermessages.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CLegacyUserMessages g_LegacyUserMessages;
CLegacyUserMessages *usermessages = &g_LegacyUserMessages;

CLegacyUserMessages::CLegacyUserMessages()
	: m_Messages( k_eDictCompareTypeCaseSensitive )
{
}

CLegacyUserMessages::~CLegacyUserMessages()
{
	m_Messages.PurgeAndDeleteElements();
}

int CLegacyUserMessages::FindOrAdd( const char *name )
{
	int idx = m_Messages.Find( name );
	if ( idx == m_Messages.InvalidIndex() )
	{
		Message_t *pMsg = new Message_t;
		pMsg->size = -1;
		idx = m_Messages.Insert( name, pMsg );
	}
	return idx;
}

int CLegacyUserMessages::LookupUserMessage( const char *name )
{
	int idx = m_Messages.Find( name );
	return idx == m_Messages.InvalidIndex() ? -1 : idx;
}

int CLegacyUserMessages::GetUserMessageSize( int index )
{
	return IsValidIndex( index ) ? m_Messages[ index ]->size : -1;
}

const char *CLegacyUserMessages::GetUserMessageName( int index )
{
	return IsValidIndex( index ) ? m_Messages.GetElementName( index ) : NULL;
}

bool CLegacyUserMessages::IsValidIndex( int index )
{
	return m_Messages.IsValidIndex( index );
}

void CLegacyUserMessages::Register( const char *name, int size )
{
	m_Messages[ FindOrAdd( name ) ]->size = size;
}

void CLegacyUserMessages::HookMessage( const char *name, pfnUserMsgHook hook )
{
	Message_t *pMsg = m_Messages[ FindOrAdd( name ) ];
	if ( pMsg->hooks.Find( hook ) == pMsg->hooks.InvalidIndex() )
	{
		pMsg->hooks.AddToTail( hook );
	}
}

bool CLegacyUserMessages::DispatchUserMessage( const char *name, bf_read &msg_data )
{
	int idx = m_Messages.Find( name );
	if ( idx == m_Messages.InvalidIndex() || m_Messages[ idx ]->hooks.Count() == 0 )
	{
		DevMsg( "CLegacyUserMessages::DispatchUserMessage: no hook for %s\n", name );
		return true; // not hooking a message is fine
	}

	Message_t *pMsg = m_Messages[ idx ];
	for ( int i = 0; i < pMsg->hooks.Count(); ++i )
	{
		bf_read copy = msg_data;
		( *pMsg->hooks[ i ] )( copy );
	}
	return true;
}

#ifdef CLIENT_DLL

CLegacyUserMessageRegister *CLegacyUserMessageRegister::s_pHead = NULL;

CLegacyUserMessageRegister::CLegacyUserMessageRegister( const char *pMessageName, pfnUserMsgHook pfnHook )
	: m_pMessageName( pMessageName ), m_pfnHook( pfnHook )
{
	m_pNext = s_pHead;
	s_pHead = this;
}

void CLegacyUserMessageRegister::RegisterAll()
{
	for ( CLegacyUserMessageRegister *pReg = s_pHead; pReg; pReg = pReg->m_pNext )
	{
		usermessages->HookMessage( pReg->m_pMessageName, pReg->m_pfnHook );
	}
}

#endif

//-----------------------------------------------------------------------------
// Declares the messages on both sides and, on the client, receives them
//-----------------------------------------------------------------------------
class CLegacyUserMessageSystem : public CAutoGameSystem
{
public:
	CLegacyUserMessageSystem() : CAutoGameSystem( "CLegacyUserMessageSystem" ) {}

	virtual bool Init()
	{
		RegisterUserMessages();
#ifdef CLIENT_DLL
		CLegacyUserMessageRegister::RegisterAll();
		m_Binder.Bind< P2_UM_Legacy, CP2UsrMsg_Legacy >( UtlMakeDelegate( this, &CLegacyUserMessageSystem::OnMessage ) );
#endif
		return true;
	}

#ifdef CLIENT_DLL
	virtual void Shutdown()
	{
		m_Binder.Unbind();
	}

	bool OnMessage( const CP2UsrMsg_Legacy &msg )
	{
		const std::string &data = msg.data();
		int nBits = msg.has_bits() ? msg.bits() : (int)data.size() * 8;
		nBits = MIN( nBits, (int)data.size() * 8 );
		bf_read buf( data.data(), data.size(), nBits );
		return usermessages->DispatchUserMessage( msg.name().c_str(), buf );
	}

	CUserMessageBinder m_Binder;
#endif
};

static CLegacyUserMessageSystem g_LegacyUserMessageSystem;
