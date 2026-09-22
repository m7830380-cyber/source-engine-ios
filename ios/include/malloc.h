// <malloc.h> for Apple platforms.
//
// Valve's code includes the glibc/MSVC header, which Darwin does not ship;
// the allocator declarations live in <malloc/malloc.h> and <stdlib.h>.
#ifndef IOS_COMPAT_MALLOC_H
#define IOS_COMPAT_MALLOC_H

#include <stdlib.h>
#include <malloc/malloc.h>

#endif // IOS_COMPAT_MALLOC_H
