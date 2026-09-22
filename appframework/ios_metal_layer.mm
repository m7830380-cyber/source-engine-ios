#include <QuartzCore/CAMetalLayer.h>
#include <CoreGraphics/CoreGraphics.h>
#include <Metal/Metal.h>
#include <UIKit/UIKit.h>

extern "C" float IOS_NativeScreenScale( void )
{
	CGFloat scale = [UIScreen mainScreen].nativeScale;
	if ( scale < 1.0 )
		scale = [UIScreen mainScreen].scale;
	if ( scale < 1.0 )
		scale = 2.0;
	return (float)scale;
}

extern "C" void IOS_ConfigureMetalLayer( void *layerPtr )
{
	CAMetalLayer *layer = (__bridge CAMetalLayer *)layerPtr;
	if ( !layer )
		return;

	// Linear BGRA matches FakeSRGBWrite (shaders encode sRGB). Do not use
	// BGRA8Unorm_sRGB here — that double-encodes and stays dark.
	layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	layer.framebufferOnly = YES;

	CGFloat scale = IOS_NativeScreenScale();
	layer.contentsScale = scale;

	CGSize points = CGSizeZero;
	if ( [layer.delegate isKindOfClass:[UIView class]] )
		points = ((UIView *)layer.delegate).bounds.size;
	if ( points.width < 1.0 || points.height < 1.0 )
		points = [UIScreen mainScreen].bounds.size;
	if ( points.width > 0.0 && points.height > 0.0 )
		layer.drawableSize = CGSizeMake( points.width * scale, points.height * scale );

	// Pin the drawable to sRGB so present is not at the mercy of ReplayKit /
	// screen-recording flipping the compositor colorspace.
	CGColorSpaceRef srgb = CGColorSpaceCreateWithName( kCGColorSpaceSRGB );
	if ( srgb )
	{
		layer.colorspace = srgb;
		CGColorSpaceRelease( srgb );
	}
}
