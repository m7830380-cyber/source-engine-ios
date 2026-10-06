#ifndef PROP_MONSTER_BOX_H
#define PROP_MONSTER_BOX_H
#ifdef _WIN32
#pragma once
#endif

#include "props.h"

class CPropMonsterBox : public CDynamicProp
{
public:
	DECLARE_CLASS( CPropMonsterBox, CDynamicProp );

	void BecomeBox( bool b ) { (void)b; }
	void BecomeMonster( bool b ) { (void)b; }
};

#endif
