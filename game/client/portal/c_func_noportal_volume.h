#ifndef C_FUNC_NOPORTAL_VOLUME_H
#define C_FUNC_NOPORTAL_VOLUME_H
#ifdef _WIN32
#pragma once
#endif
#include "cbase.h"

class C_FuncNoPortalVolume : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FuncNoPortalVolume, C_BaseEntity );
	DECLARE_CLIENTCLASS();
};

#endif
