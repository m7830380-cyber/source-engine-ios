#ifndef C_TRIGGER_PORTAL_CLEANSER_H
#define C_TRIGGER_PORTAL_CLEANSER_H
#ifdef _WIN32
#pragma once
#endif
#include "c_triggers.h"

class C_TriggerPortalCleanser : public C_BaseTrigger
{
public:
	DECLARE_CLASS( C_TriggerPortalCleanser, C_BaseTrigger );
	DECLARE_CLIENTCLASS();
};

#endif
