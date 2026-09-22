//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: The part of GCSDK that CS:GO's client and server link against.
//
// The game coordinator SDK (gcsdk_game.vpc) is not part of the CS:GO source
// tree. Game code uses it for the item schema/inventory (shared objects,
// protobuf message pools) and registers GC jobs at startup. There is no game
// coordinator on this build (NO_STEAM_GAMECOORDINATOR), so jobs are
// registered but never run and no GC packets ever arrive; shared objects,
// their caches and the message pools are fully implemented because the
// inventory uses them locally. Implementations follow the GCSDK sources from
// Valve's 2018 TF2 tree, adapted to CS:GO's headers.
//
//=============================================================================

#include "gcsdk/gcsdk_auto.h"
#include "steammessages.pb.h"
#include "google/protobuf/descriptor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

//-----------------------------------------------------------------------------
// Logging
//-----------------------------------------------------------------------------

DECLARE_GC_EMIT_GROUP( SPEW_GC, gc );

void EmitWarning( const CGCEmitGroup &Group, int iLevel, const char *pchMsg, ... )
{
	char szMsg[2048];
	va_list args;
	va_start( args, pchMsg );
	V_vsnprintf( szMsg, sizeof( szMsg ), pchMsg, args );
	va_end( args );
	Warning( "[%s] %s", Group.GetName(), szMsg );
}

//-----------------------------------------------------------------------------
// Jobs: registered at startup, never started (no game coordinator)
//-----------------------------------------------------------------------------

uint64 CJobTime::sm_lTimeCur = 0;
bool CJob::s_bStartDefaultJobsDelayed = false;

CJobTime::CJobTime()
{
	m_lTime = sm_lTimeCur;
}

CJob::CJob( CJobMgr &jobMgr, char const *pchJobName )
	: m_JobMgr( jobMgr )
{
	m_bRunFromMsg = false;
	m_bWorkItemCanceled = false;
	m_bIsTest = false;
	m_bIsLongRunning = false;
	m_JobID = k_GIDNil;
	m_hCoroutine = 0;
	m_pvStartParam = NULL;
	m_flags.m_uFlags = 0;
	m_cLocksAttempted = 0;
	m_cLocksWaitedFor = 0;
	m_ePauseReason = k_EJobPauseReasonNone;
	m_pszPauseResourceName = NULL;
	m_unWaitMsgType = 0;
	m_nContextMask = 0;
	m_pJobPrev = NULL;
	m_pWaitingOnLock = NULL;
	m_pWaitingOnLockFilename = NULL;
	m_waitingOnLockLine = 0;
	m_pJobToNotifyOnLockRelease = NULL;
	m_pWaitingOnWorkItem = NULL;
	m_pJobType = NULL;
	m_pchJobName = pchJobName;
}

CJob::~CJob()
{
}

uint32 CJob::CHeartbeatsBeforeTimeout()
{
	return 0;
}

bool CJob::BYieldingWaitOneFrame()
{
	AssertMsg( false, "GC jobs do not run without a game coordinator" );
	return false;
}

void CJobMgr::RegisterJobType( const JobType_t *pJobType )
{
	// message dispatch to jobs only happens through the GC connection
}

//-----------------------------------------------------------------------------
// Protobuf message pools
//-----------------------------------------------------------------------------

CProtoBufMsgMemoryPoolMgr *GProtoBufMsgMemoryPoolMgr()
{
	static CProtoBufMsgMemoryPoolMgr s_ProtoBufMsgMemoryPoolMgr;
	return &s_ProtoBufMsgMemoryPoolMgr;
}

CThreadMutex CProtoBufMsgBase::s_PoolRegMutex;

CProtoBufMsgMemoryPoolBase::CProtoBufMsgMemoryPoolBase( uint32 unTargetLow, uint32 unTargetHigh )
{
	m_unTargetCountLow = unTargetLow;
	m_unTargetCountHigh = unTargetHigh;
	m_unAllocHitCounter = 0;
	m_unAllocMissCounter = 0;
	m_unAllocated = 0;
	m_pTSQueueFreeObjects = new CTSQueue< ::google::protobuf::Message * >();
}

CProtoBufMsgMemoryPoolBase::~CProtoBufMsgMemoryPoolBase()
{
	m_pTSQueueFreeObjects->Purge();
	delete m_pTSQueueFreeObjects;
}

::google::protobuf::Message *CProtoBufMsgMemoryPoolBase::Alloc()
{
	::google::protobuf::Message *pObject;
	if ( !m_pTSQueueFreeObjects->PopItem( &pObject ) || !pObject )
	{
		++m_unAllocMissCounter;
		pObject = InternalAlloc();
	}
	else
	{
		++m_unAllocHitCounter;
		uint32 unCount = m_pTSQueueFreeObjects->Count();
		// shrink towards the target counts over time
		bool bFreeAnother = ( unCount > m_unTargetCountHigh )
			|| ( unCount > m_unTargetCountLow && m_unAllocHitCounter % 6 == 0 );
		if ( bFreeAnother )
		{
			::google::protobuf::Message *pThrowAway;
			if ( m_pTSQueueFreeObjects->PopItem( &pThrowAway ) && pThrowAway )
			{
				InternalFree( pThrowAway );
			}
		}
	}
	++m_unAllocated;
	return pObject;
}

void CProtoBufMsgMemoryPoolBase::Free( ::google::protobuf::Message *pObject )
{
	pObject->Clear();
	--m_unAllocated;
	m_pTSQueueFreeObjects->PushItem( pObject );
}

uint32 CProtoBufMsgMemoryPoolBase::GetEstimatedSize()
{
	return 0;
}

bool CProtoBufMsgMemoryPoolBase::PopItem( google::protobuf::Message **ppMsg )
{
	return m_pTSQueueFreeObjects->PopItem( ppMsg );
}

CProtoBufMsgMemoryPoolMgr::CProtoBufMsgMemoryPoolMgr() : m_PoolHeaders()
{
	m_vecMsgPools.AddToTail( &m_PoolHeaders );
}

CProtoBufMsgMemoryPoolMgr::~CProtoBufMsgMemoryPoolMgr()
{
	FOR_EACH_VEC( m_vecMsgPools, i )
	{
		if ( m_vecMsgPools[i] != &m_PoolHeaders )
			delete m_vecMsgPools[i];
	}
}

void CProtoBufMsgMemoryPoolMgr::RegisterPool( CProtoBufMsgMemoryPoolBase *pPool )
{
	m_vecMsgPools.AddToTail( pPool );
}

void CProtoBufMsgMemoryPoolMgr::DumpPoolInfo()
{
	FOR_EACH_VEC( m_vecMsgPools, i )
	{
		Msg( "%43s%12d%12d\n", m_vecMsgPools[i]->GetName().String(), m_vecMsgPools[i]->GetAllocated(), m_vecMsgPools[i]->GetFree() );
	}
}

CProtoBufMsgBase::CProtoBufMsgBase()
	: m_pNetPacket( NULL )
	, m_pProtoBufHdr( NULL )
	, m_eMsg( 0 )
{
}

CProtoBufMsgBase::CProtoBufMsgBase( MsgType_t eMsg )
	: m_pNetPacket( NULL )
	, m_pProtoBufHdr( NULL )
	, m_eMsg( eMsg )
{
	m_pProtoBufHdr = GProtoBufMsgMemoryPoolMgr()->AllocProtoBufHdr();
}

CProtoBufMsgBase::~CProtoBufMsgBase()
{
	// no net packets without a GC connection, so the header is always ours
	if ( m_pProtoBufHdr )
	{
		GProtoBufMsgMemoryPoolMgr()->FreeProtoBufHdr( m_pProtoBufHdr );
	}
	m_pProtoBufHdr = NULL;
}

bool CProtoBufMsgBase::InitFromPacket( IMsgNetPacket *pNetPacket )
{
	AssertMsg( false, "GC packets do not arrive without a game coordinator" );
	return false;
}

//-----------------------------------------------------------------------------
// Shared objects
//-----------------------------------------------------------------------------

CSharedObject::TVecFactories CSharedObject::sm_vecFactories;

bool CSharedObject::BIsKeyEqual( const CSharedObject &soRHS ) const
{
	if ( GetTypeID() != soRHS.GetTypeID() )
		return false;
	return !BIsKeyLess( soRHS ) && !soRHS.BIsKeyLess( *this );
}

void CSharedObject::RegisterFactory( int nTypeID, SOCreationFunc_t fnFactory, uint32 unFlags, const char *pchClassName, const char *pszBuildCacheName, const char *pszCreateName, const char *pszUpdateName )
{
	SharedObjectInfo_t info;
	info.m_nID = nTypeID;
	info.m_unFlags = unFlags;
	info.m_pFactoryFunction = fnFactory;
	info.m_pchClassName = pchClassName;
	info.m_pchBuildCacheSubNodeName = pszBuildCacheName;
	info.m_pchUpdateNodeName = pszUpdateName;
	info.m_pchCreateNodeName = pszCreateName;

	int nIndex = sm_vecFactories.Find( nTypeID );
	if ( nIndex != sm_vecFactories.InvalidIndex() )
	{
		sm_vecFactories[nIndex] = info;
	}
	else
	{
		sm_vecFactories.Insert( info );
	}
}

const CSharedObject::SharedObjectInfo_t *CSharedObject::FindSharedObjectInfo( int nTypeID )
{
	int nIndex = sm_vecFactories.Find( nTypeID );
	return ( nIndex != sm_vecFactories.InvalidIndex() ) ? &sm_vecFactories[nIndex] : NULL;
}

CSharedObject *CSharedObject::Create( int nTypeID )
{
	const SharedObjectInfo_t *pInfo = FindSharedObjectInfo( nTypeID );
	AssertMsg1( pInfo, "Probably failed to set object type (%d) on the server/client.\n", nTypeID );
	return pInfo ? pInfo->m_pFactoryFunction() : NULL;
}

uint32 CSharedObject::GetTypeFlags( int nTypeID )
{
	const SharedObjectInfo_t *pInfo = FindSharedObjectInfo( nTypeID );
	return pInfo ? pInfo->m_unFlags : 0;
}

const char *CSharedObject::PchClassName( int nTypeID )
{
	const SharedObjectInfo_t *pInfo = FindSharedObjectInfo( nTypeID );
	return pInfo ? pInfo->m_pchClassName : "Unknown";
}

const char *CSharedObject::PchClassBuildCacheNodeName( int nTypeID )
{
	const SharedObjectInfo_t *pInfo = FindSharedObjectInfo( nTypeID );
	return pInfo ? pInfo->m_pchBuildCacheSubNodeName : "Unknown";
}

const char *CSharedObject::PchClassCreateNodeName( int nTypeID )
{
	const SharedObjectInfo_t *pInfo = FindSharedObjectInfo( nTypeID );
	return pInfo ? pInfo->m_pchCreateNodeName : "Unknown";
}

const char *CSharedObject::PchClassUpdateNodeName( int nTypeID )
{
	const SharedObjectInfo_t *pInfo = FindSharedObjectInfo( nTypeID );
	return pInfo ? pInfo->m_pchUpdateNodeName : "Unknown";
}

//-----------------------------------------------------------------------------
// Shared object caches
//-----------------------------------------------------------------------------

int CSharedObjectTypeCache::FindSharedObjectIndex( const CSharedObject &soIndex ) const
{
	FOR_EACH_VEC( m_vecObjects, nObj )
	{
		if ( m_vecObjects[nObj]->BIsKeyEqual( soIndex ) )
			return nObj;
	}
	return -1;
}

CSharedObject *CSharedObjectTypeCache::FindSharedObject( const CSharedObject &soIndex )
{
	int nIndex = FindSharedObjectIndex( soIndex );
	return ( nIndex >= 0 ) ? m_vecObjects[nIndex] : NULL;
}

const CSharedObject *CSharedObjectTypeCache::FindSharedObject( const CSharedObject &soIndex ) const
{
	int nIndex = FindSharedObjectIndex( soIndex );
	return ( nIndex >= 0 ) ? m_vecObjects[nIndex] : NULL;
}

CSharedObjectTypeCache *CSharedObjectCache::FindBaseTypeCache( int nClassID )
{
	FOR_EACH_VEC( m_CacheObjects, i )
	{
		if ( m_CacheObjects[i]->GetTypeID() == nClassID )
			return m_CacheObjects[i];
	}
	return NULL;
}

const CSharedObjectTypeCache *CSharedObjectCache::FindBaseTypeCache( int nClassID ) const
{
	return const_cast< CSharedObjectCache * >( this )->FindBaseTypeCache( nClassID );
}

CSharedObject *CSharedObjectCache::FindSharedObject( const CSharedObject &soIndex )
{
	CSharedObjectTypeCache *pTypeCache = FindBaseTypeCache( soIndex.GetTypeID() );
	return pTypeCache ? pTypeCache->FindSharedObject( soIndex ) : NULL;
}

const CSharedObject *CSharedObjectCache::FindSharedObject( const CSharedObject &soIndex ) const
{
	return const_cast< CSharedObjectCache * >( this )->FindSharedObject( soIndex );
}

//-----------------------------------------------------------------------------
// Protobuf-backed shared objects
//-----------------------------------------------------------------------------

using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;

static bool IsKeyField( const FieldDescriptor *pField )
{
	return pField->options().GetExtension( key_field );
}

// key fields are scalars or strings
static bool IsProtoBufFieldLess( const Message &msgLHS, const Message &msgRHS, const FieldDescriptor *pField )
{
	const Reflection *pLHS = msgLHS.GetReflection();
	const Reflection *pRHS = msgRHS.GetReflection();
	switch ( pField->cpp_type() )
	{
	case FieldDescriptor::CPPTYPE_INT32:	return pLHS->GetInt32( msgLHS, pField ) < pRHS->GetInt32( msgRHS, pField );
	case FieldDescriptor::CPPTYPE_INT64:	return pLHS->GetInt64( msgLHS, pField ) < pRHS->GetInt64( msgRHS, pField );
	case FieldDescriptor::CPPTYPE_UINT32:	return pLHS->GetUInt32( msgLHS, pField ) < pRHS->GetUInt32( msgRHS, pField );
	case FieldDescriptor::CPPTYPE_UINT64:	return pLHS->GetUInt64( msgLHS, pField ) < pRHS->GetUInt64( msgRHS, pField );
	case FieldDescriptor::CPPTYPE_DOUBLE:	return pLHS->GetDouble( msgLHS, pField ) < pRHS->GetDouble( msgRHS, pField );
	case FieldDescriptor::CPPTYPE_FLOAT:	return pLHS->GetFloat( msgLHS, pField ) < pRHS->GetFloat( msgRHS, pField );
	case FieldDescriptor::CPPTYPE_BOOL:		return pLHS->GetBool( msgLHS, pField ) < pRHS->GetBool( msgRHS, pField );
	case FieldDescriptor::CPPTYPE_ENUM:		return pLHS->GetEnum( msgLHS, pField )->number() < pRHS->GetEnum( msgRHS, pField )->number();
	case FieldDescriptor::CPPTYPE_STRING:	return pLHS->GetString( msgLHS, pField ) < pRHS->GetString( msgRHS, pField );
	default:
		AssertMsg1( false, "Unsupported key field type %d", pField->cpp_type() );
		return false;
	}
}

bool CProtoBufSharedObjectBase::BParseFromMessage( const CUtlBuffer &buffer )
{
	return GetPObject()->ParseFromArray( buffer.Base(), buffer.TellMaxPut() );
}

bool CProtoBufSharedObjectBase::BParseFromMessage( const std::string &buffer )
{
	return GetPObject()->ParseFromString( buffer );
}

bool CProtoBufSharedObjectBase::BUpdateFromNetwork( const CSharedObject &objUpdate )
{
	const CProtoBufSharedObjectBase &pbobjUpdate = (const CProtoBufSharedObjectBase &)objUpdate;
	GetPObject()->CopyFrom( *pbobjUpdate.GetPObject() );
	return true;
}

bool CProtoBufSharedObjectBase::BAddToMessage( std::string *pBuffer ) const
{
	return GetPObject()->SerializeToString( pBuffer );
}

// just the key fields, which is what the other side needs to find the object
Message *CProtoBufSharedObjectBase::BuildDestroyToMessage( const Message &msg )
{
	const ::google::protobuf::Descriptor *pDescriptor = msg.GetDescriptor();
	Message *pMessageToSend = msg.New();
	for ( int nField = 0; nField < pDescriptor->field_count(); nField++ )
	{
		const FieldDescriptor *pField = pDescriptor->field( nField );
		if ( !IsKeyField( pField ) || pField->is_repeated() )
			continue;
		const Reflection *pSrc = msg.GetReflection();
		const Reflection *pDst = pMessageToSend->GetReflection();
		switch ( pField->cpp_type() )
		{
		case FieldDescriptor::CPPTYPE_INT32:	pDst->SetInt32( pMessageToSend, pField, pSrc->GetInt32( msg, pField ) ); break;
		case FieldDescriptor::CPPTYPE_INT64:	pDst->SetInt64( pMessageToSend, pField, pSrc->GetInt64( msg, pField ) ); break;
		case FieldDescriptor::CPPTYPE_UINT32:	pDst->SetUInt32( pMessageToSend, pField, pSrc->GetUInt32( msg, pField ) ); break;
		case FieldDescriptor::CPPTYPE_UINT64:	pDst->SetUInt64( pMessageToSend, pField, pSrc->GetUInt64( msg, pField ) ); break;
		case FieldDescriptor::CPPTYPE_DOUBLE:	pDst->SetDouble( pMessageToSend, pField, pSrc->GetDouble( msg, pField ) ); break;
		case FieldDescriptor::CPPTYPE_FLOAT:		pDst->SetFloat( pMessageToSend, pField, pSrc->GetFloat( msg, pField ) ); break;
		case FieldDescriptor::CPPTYPE_BOOL:		pDst->SetBool( pMessageToSend, pField, pSrc->GetBool( msg, pField ) ); break;
		case FieldDescriptor::CPPTYPE_ENUM:		pDst->SetEnum( pMessageToSend, pField, pSrc->GetEnum( msg, pField ) ); break;
		case FieldDescriptor::CPPTYPE_STRING:	pDst->SetString( pMessageToSend, pField, pSrc->GetString( msg, pField ) ); break;
		default: break;
		}
	}
	return pMessageToSend;
}

bool CProtoBufSharedObjectBase::BAddDestroyToMessage( std::string *pBuffer ) const
{
	Message *pMessageToSend = BuildDestroyToMessage( *GetPObject() );
	bool bResult = pMessageToSend->SerializeToString( pBuffer );
	delete pMessageToSend;
	return bResult;
}

bool CProtoBufSharedObjectBase::BIsKeyLess( const CSharedObject &soRHS ) const
{
	const Message &msgLHS = *GetPObject();
	const Message &msgRHS = *( (const CProtoBufSharedObjectBase &)soRHS ).GetPObject();
	const ::google::protobuf::Descriptor *pDescriptor = msgLHS.GetDescriptor();
	for ( int nField = 0; nField < pDescriptor->field_count(); nField++ )
	{
		const FieldDescriptor *pField = pDescriptor->field( nField );
		if ( !IsKeyField( pField ) )
			continue;
		if ( IsProtoBufFieldLess( msgLHS, msgRHS, pField ) )
			return true;
		if ( IsProtoBufFieldLess( msgRHS, msgLHS, pField ) )
			return false;
	}
	return false;
}

void CProtoBufSharedObjectBase::Copy( const CSharedObject &soRHS )
{
	Assert( GetTypeID() == soRHS.GetTypeID() );
	GetPObject()->CopyFrom( *( (const CProtoBufSharedObjectBase &)soRHS ).GetPObject() );
}

void CProtoBufSharedObjectBase::Dump( const Message &msg )
{
	// line by line to stay under the spew buffer size
	CUtlStringList lines;
	V_SplitString( msg.DebugString().c_str(), "\n", lines );
	for ( int i = 0; i < lines.Count(); i++ )
	{
		Msg( "%s\n", lines[i] );
	}
}

void CProtoBufSharedObjectBase::Dump() const
{
	Dump( *GetPObject() );
}

bool CProtoBufSharedObjectBase::SerializeToBuffer( const Message &msg, CUtlBuffer &bufOutput )
{
	uint32 unSize = msg.ByteSize();
	bufOutput.Clear();
	bufOutput.EnsureCapacity( unSize );
	msg.SerializeWithCachedSizesToArray( (uint8 *)bufOutput.Base() );
	bufOutput.SeekPut( CUtlBuffer::SEEK_HEAD, unSize );
	return true;
}

} // namespace GCSDK
