// <Carbon/Carbon.h> for iOS.
//
// CS:GO's OSX code includes the Carbon umbrella mostly for CoreFoundation and
// CoreGraphics types. iOS ships those frameworks but not Carbon, so pull in
// the parts that exist; any real Carbon/AppKit API use still fails to compile
// and has to be guarded with IOS in the source.
#ifndef IOS_COMPAT_CARBON_H
#define IOS_COMPAT_CARBON_H

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#endif // IOS_COMPAT_CARBON_H
