#ifndef PORTAL2_IOS_COMPAT_H
#define PORTAL2_IOS_COMPAT_H

#include "tier1/strtools.h"

#if !defined( _WIN32 ) && !defined( itoa )
inline char *itoa( int value, char *buffer, int radix )
{
	if ( radix == 10 )
	{
		Q_snprintf( buffer, 32, "%d", value );
	}
	else
	{
		buffer[0] = '\0';
	}
	return buffer;
}
#endif

#ifndef null
#define null NULL
#endif

namespace vgui
{
	class Frame;
	class TextEntry;
	class Panel;
	class Label;
	class Button;
	class IScheme;
}

#endif
