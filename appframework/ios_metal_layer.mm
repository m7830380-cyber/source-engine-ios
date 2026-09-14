#include <QuartzCore/CAMetalLayer.h>
#include <CoreGraphics/CoreGraphics.h>

extern "C" void IOS_ConfigureMetalLayer( void *layerPtr )
{
	CAMetalLayer *layer = (__bridge CAMetalLayer *)layerPtr;
	if ( !layer )
		return;

	// Pin the drawable to sRGB so present is not at the mercy of ReplayKit /
	// screen-recording flipping the compositor colorspace.
	CGColorSpaceRef srgb = CGColorSpaceCreateWithName( kCGColorSpaceSRGB );
	if ( srgb )
	{
		layer.colorspace = srgb;
		CGColorSpaceRelease( srgb );
	}
}
