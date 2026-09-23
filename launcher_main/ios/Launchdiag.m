/*
 Launchdiag.m - iOS launch dialog + boot diagnostics
 Copyright (C) 2016 mittorn
 iOS boot diagnostics 2026

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 ---------------------------------------------------------------------------
 NOTE ON FILE TYPE: this is a pure Objective-C (.m) translation unit. It must
 NEVER include Valve headers. basetypes.h does `typedef int BOOL` while
 <objc/objc.h> does `typedef bool BOOL`; mixing them breaks the ObjC runtime
 in subtle ways. Everything the C++ side needs is exported through plain-C
 entry points declared in Launchdiag.h.
 ---------------------------------------------------------------------------
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <pthread.h>
#include <signal.h>
#include <execinfo.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <mach/mach.h>

#import "Launchdiag.h"

#ifndef IOS_DEFAULT_GAME
#define IOS_DEFAULT_GAME "hl2"
#endif

// ---------------------------------------------------------------------------
// UIAlertView delegate (deprecated API, but it is what this dialog uses)
// ---------------------------------------------------------------------------

@interface XashPromptAlertViewDelegate : NSObject <UIAlertViewDelegate>
@property (nonatomic, assign) int *button;
@end

@implementation XashPromptAlertViewDelegate
- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
	*_button = (int)buttonIndex;
}
@end

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------

int szArgc;
char **szArgv;
char *g_szLibrarySuffix;
float g_iOSVer;
bool isdark;

static UIWindow     *g_window     = nil;
static UIViewController *g_controller = nil;
static UITextView   *g_logView    = nil;   // on-screen log mirror
static FILE         *g_logFile    = NULL;
static char          g_logPath[PATH_MAX];
static int           g_bOnScreenLog = 0;

#define SETTINGS_MAGIC 111

typedef struct settings_s
{
	unsigned char magic;
	char args[1024];
	unsigned int port;
	char suffix[32];
	unsigned int ftpserver;
} settings_t;

// ---------------------------------------------------------------------------
// Paths
//
// BUGFIX: the old code did
//     dir = [[[NSBundle mainBundle] bundleURL] fileSystemRepresentation];
// and cached that in a static. -fileSystemRepresentation returns a buffer
// owned by an autoreleased object; once the pool drains the static points at
// freed memory. Every later use (dlopen path, basedir, ...) then reads
// garbage. We strdup() into permanently-owned storage instead.
// ---------------------------------------------------------------------------

const char *IOS_GetDocsDir( void )
{
	static char *dir = NULL;

	if( dir )
		return dir;

	@autoreleasepool {
		NSArray *paths = NSSearchPathForDirectoriesInDomains( NSDocumentDirectory, NSUserDomainMask, YES );
		if( [paths count] == 0 )
		{
			NSLog( @"IOS_GetDocsDir: NSSearchPathForDirectoriesInDomains returned nothing!" );
			dir = strdup( "/tmp" );
			return dir;
		}

		NSString *documentsDirectory = [paths objectAtIndex:0];
		NSError *err = nil;
		if( ![[NSFileManager defaultManager] createDirectoryAtPath:documentsDirectory
									  withIntermediateDirectories:YES
													   attributes:nil
															error:&err] )
		{
			NSLog( @"IOS_GetDocsDir: createDirectory failed: %@", err );
		}

		// strdup: survive the autorelease pool
		dir = strdup( [documentsDirectory fileSystemRepresentation] );
	}

	NSLog( @"IOS_GetDocsDir: %s", dir );
	return dir;
}

const char *IOS_GetExecDir( void )
{
	static char *dir = NULL;

	if( dir )
		return dir;

	@autoreleasepool {
		NSString *bundlePath = [[NSBundle mainBundle] bundlePath];
		if( bundlePath == nil )
		{
			NSLog( @"IOS_GetExecDir: mainBundle has no bundlePath!" );
			dir = strdup( "." );
			return dir;
		}
		// strdup: survive the autorelease pool
		dir = strdup( [bundlePath fileSystemRepresentation] );
	}

	NSLog( @"IOS_GetExecDir: %s", dir );
	return dir;
}

// ---------------------------------------------------------------------------
// Logging
//
// printf()/stderr go nowhere visible on a sideloaded device build. We tee the
// real fds into Documents/launch_log.txt so the user can pull it out with the
// Files app, and mirror key lines onto an on-screen UITextView so a failure is
// visible without a Mac attached.
// ---------------------------------------------------------------------------

static void IOS_AppendOnScreen( const char *line )
{
	if( !g_bOnScreenLog )
		return;

	NSString *s = [NSString stringWithUTF8String:line];
	if( !s )
		return;

	dispatch_block_t work = ^{
		if( !g_logView )
			return;
		g_logView.text = [g_logView.text stringByAppendingString:
						  [NSString stringWithFormat:@"%@\n", s]];
		// keep the tail visible
		NSUInteger len = g_logView.text.length;
		if( len > 1 )
			[g_logView scrollRangeToVisible:NSMakeRange( len - 1, 1 )];
	};

	if( [NSThread isMainThread] )
		work();
	else
		dispatch_async( dispatch_get_main_queue(), work );
}

void IOS_Log( const char *fmt, ... )
{
	char buf[2048];
	va_list ap;
	va_start( ap, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );
	buf[sizeof( buf ) - 1] = 0;

	NSLog( @"[boot] %s", buf );

	if( g_logFile )
	{
		fprintf( g_logFile, "%s\n", buf );
		fflush( g_logFile );
	}

	IOS_AppendOnScreen( buf );
}

void IOS_LogInit( void )
{
	const char *docs = IOS_GetDocsDir();
	snprintf( g_logPath, sizeof( g_logPath ), "%s/launch_log.txt", docs );
	g_logPath[sizeof( g_logPath ) - 1] = 0;

	g_logFile = fopen( g_logPath, "w" );
	if( !g_logFile )
	{
		NSLog( @"IOS_LogInit: cannot open %s: %s", g_logPath, strerror( errno ) );
		return;
	}
	setvbuf( g_logFile, NULL, _IOLBF, 0 );

	// Point the real stdout/stderr fds at the same file so every printf() in
	// the engine (and dlerror prints, assert output, ...) is captured too.
	int fd = fileno( g_logFile );
	if( fd >= 0 )
	{
		dup2( fd, STDOUT_FILENO );
		dup2( fd, STDERR_FILENO );
		setvbuf( stdout, NULL, _IOLBF, 0 );
		setvbuf( stderr, NULL, _IOLBF, 0 );
	}

	IOS_Log( "=== CS:GO iOS boot log ===" );
	IOS_Log( "log file: %s", g_logPath );
}

const char *IOS_GetLogPath( void )
{
	return g_logPath;
}

// ---------------------------------------------------------------------------
// Environment / diagnostics dump
// ---------------------------------------------------------------------------

static void IOS_LogMemory( void )
{
	mach_task_basic_info_data_t info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if( task_info( mach_task_self(), MACH_TASK_BASIC_INFO,
				   (task_info_t)&info, &count ) == KERN_SUCCESS )
	{
		IOS_Log( "memory: resident %.1f MB, limit ~%.0f MB",
				 info.resident_size / ( 1024.0 * 1024.0 ),
				 [NSProcessInfo processInfo].physicalMemory / ( 1024.0 * 1024.0 ) );
	}
}

void IOS_LogDeviceInfo( void )
{
	@autoreleasepool {
		UIDevice *dev = [UIDevice currentDevice];
		IOS_Log( "device: %s / iOS %s",
				 [[dev model] UTF8String], [[dev systemVersion] UTF8String] );
		IOS_Log( "bundle: %s", IOS_GetExecDir() );
		IOS_Log( "docs:   %s", IOS_GetDocsDir() );
		IOS_LogMemory();
	}
}

// List what actually shipped in the .app — catches "the dylib was never
// packaged" instantly instead of guessing from a NULL dlopen.
void IOS_LogBundleContents( void )
{
	@autoreleasepool {
		NSString *root = [NSString stringWithUTF8String:IOS_GetExecDir()];
		NSError *err = nil;
		NSArray *items = [[NSFileManager defaultManager]
						  contentsOfDirectoryAtPath:root error:&err];
		if( !items )
		{
			IOS_Log( "bundle listing FAILED: %s",
					 [[err localizedDescription] UTF8String] );
			return;
		}

		IOS_Log( "--- bundle contents (%lu entries) ---",
				 (unsigned long)[items count] );

		NSArray *sorted = [items sortedArrayUsingSelector:@selector(compare:)];
		for( NSString *name in sorted )
		{
			NSString *full = [root stringByAppendingPathComponent:name];
			NSDictionary *attr = [[NSFileManager defaultManager]
								  attributesOfItemAtPath:full error:nil];
			unsigned long long sz = [attr fileSize];
			BOOL isDir = NO;
			[[NSFileManager defaultManager] fileExistsAtPath:full isDirectory:&isDir];

			if( isDir )
				IOS_Log( "  [dir]  %s", [name UTF8String] );
			else if( [name hasSuffix:@".dylib"] )
				IOS_Log( "  [lib]  %-32s %8llu KB",
						 [name UTF8String], sz / 1024 );
			else
				IOS_Log( "  [file] %-32s %8llu KB",
						 [name UTF8String], sz / 1024 );
		}
		IOS_Log( "--- end bundle contents ---" );

		// Frameworks/ holds SDL2 + ANGLE; a missing Mach-O here is a classic
		// "dyld: image not found" silent death.
		NSString *fw = [root stringByAppendingPathComponent:@"Frameworks"];
		NSArray *fws = [[NSFileManager defaultManager]
						contentsOfDirectoryAtPath:fw error:nil];
		if( fws )
		{
			IOS_Log( "--- Frameworks ---" );
			for( NSString *name in [fws sortedArrayUsingSelector:@selector(compare:)] )
			{
				NSString *inner = [[fw stringByAppendingPathComponent:name]
								   stringByAppendingPathComponent:
								   [name stringByDeletingPathExtension]];
				NSDictionary *a = [[NSFileManager defaultManager]
								   attributesOfItemAtPath:inner error:nil];
				if( a )
					IOS_Log( "  %-24s binary %llu KB",
							 [name UTF8String], [a fileSize] / 1024 );
				else
					IOS_Log( "  %-24s *** NO MACH-O BINARY INSIDE ***",
							 [name UTF8String] );
			}
			IOS_Log( "--- end Frameworks ---" );
		}
	}
}

// Probe each shipped dylib individually. dlopen(launcher) pulls the whole
// dependency graph with RTLD_NOW, so one bad symbol anywhere returns NULL with
// a single opaque message. Loading them one at a time names the real culprit.
//
// IMPORTANT: dlopen always runs a module's C++ static initializers. The game
// modules (client/server) register their vscript class descriptions from
// static ScriptClassDesc_t objects, which allocate through g_pMemAlloc and
// expect the engine's tier0/vscript state to already exist. Loading them here,
// standalone and out of order, traps inside InitC_BaseEntityScriptDesc().
//
// That is a probe artifact, not a packaging fault: the engine itself loads
// these later, after tier0 is up. So they are skipped by default. Pass
// -probeall on the command line to force them (expect the trap).
static int IOS_IsEngineLoadedModule( NSString *name )
{
	// Modules the engine dlopens itself once tier0/vscript are initialized.
	static NSArray *skip = nil;
	if( !skip )
		skip = @[ @"client.dylib", @"server.dylib" ];
	return [skip containsObject:name];
}

void IOS_LogGameContent( void )
{
	@autoreleasepool {
		NSFileManager *fm = [NSFileManager defaultManager];
		NSString *game = [[NSString stringWithUTF8String:IOS_GetDocsDir()]
						  stringByAppendingPathComponent:@"csgo"];
		BOOL isDir = NO;
		if( ![fm fileExistsAtPath:game isDirectory:&isDir] || !isDir )
		{
			IOS_Log( "game content: Documents/csgo MISSING -- copy the csgo folder from the CS:GO depots into the app's Documents" );
			return;
		}

		IOS_Log( "--- game content (Documents/csgo) ---" );
		for( NSString *name in @[ @"gameinfo.txt", @"gamemodes.txt", @"pak01_dir.vpk" ] )
		{
			NSDictionary *a = [fm attributesOfItemAtPath:[game stringByAppendingPathComponent:name] error:nil];
			if( a )
				IOS_Log( "  %-16s %10llu KB", [name UTF8String], [a fileSize] / 1024 );
			else
				IOS_Log( "  %-16s MISSING", [name UTF8String] );
		}

		NSArray *items = [fm contentsOfDirectoryAtPath:game error:nil];
		unsigned long nChunks = 0;
		unsigned long long nChunkBytes = 0;
		NSMutableArray *dirs = [NSMutableArray array];
		for( NSString *name in items )
		{
			NSString *full = [game stringByAppendingPathComponent:name];
			if( [name hasPrefix:@"pak01_"] && [name hasSuffix:@".vpk"] && ![name isEqualToString:@"pak01_dir.vpk"] )
			{
				nChunks++;
				nChunkBytes += [[fm attributesOfItemAtPath:full error:nil] fileSize];
			}
			else if( [fm fileExistsAtPath:full isDirectory:&isDir] && isDir )
				[dirs addObject:name];
		}
		IOS_Log( "  pak01_###.vpk    %lu files, %llu MB", nChunks, nChunkBytes / ( 1024 * 1024 ) );
		IOS_Log( "  dirs: %s", [[[dirs sortedArrayUsingSelector:@selector(compare:)]
								  componentsJoinedByString:@" "] UTF8String] );
		IOS_Log( "--- end game content ---" );
	}
}

void IOS_ProbeDylibs( void )
{
	@autoreleasepool {
		NSString *root = [NSString stringWithUTF8String:IOS_GetExecDir()];
		NSArray *items = [[NSFileManager defaultManager]
						  contentsOfDirectoryAtPath:root error:nil];
		if( !items )
			return;

		int bProbeAll = 0;
		for( int i = 0; i < szArgc; i++ )
			if( szArgv[i] && strcmp( szArgv[i], "-probeall" ) == 0 )
				bProbeAll = 1;

		// Load order roughly follows the engine's dependency chain so the
		// first failure is the most informative one.
		NSArray *preferred = @[ @"libtier0.dylib", @"libvstdlib.dylib",
								@"libtogl.dylib", @"filesystem_stdio.dylib",
								@"datacache.dylib", @"studiorender.dylib",
								@"vphysics.dylib", @"materialsystem.dylib",
								@"shaderapidx9.dylib", @"inputsystem.dylib",
								@"soundsystem.dylib", @"vgui2.dylib",
								@"vguimatsurface.dylib", @"engine.dylib",
								@"launcher.dylib" ];

		NSMutableArray *order = [NSMutableArray array];
		for( NSString *p in preferred )
			if( [items containsObject:p] )
				[order addObject:p];
		for( NSString *p in [items sortedArrayUsingSelector:@selector(compare:)] )
			if( [p hasSuffix:@".dylib"] && ![order containsObject:p] )
				[order addObject:p];

		IOS_Log( "--- dlopen probe (%lu dylibs, probeall=%d) ---",
				 (unsigned long)[order count], bProbeAll );

		int nOk = 0, nFail = 0, nSkip = 0;
		for( NSString *name in order )
		{
			if( !bProbeAll && IOS_IsEngineLoadedModule( name ) )
			{
				IOS_Log( "  SKIP  %s (engine loads this after tier0 init)",
						 [name UTF8String] );
				nSkip++;
				continue;
			}

			NSString *full = [root stringByAppendingPathComponent:name];
			IOS_Log( "  ...   %s", [name UTF8String] );   // breadcrumb: survives a trap
			dlerror(); // clear
			void *h = dlopen( [full fileSystemRepresentation], RTLD_NOW | RTLD_LOCAL );
			if( h )
			{
				IOS_Log( "  OK    %s", [name UTF8String] );
				nOk++;
			}
			else
			{
				const char *e = dlerror();
				IOS_Log( "  FAIL  %s", [name UTF8String] );
				IOS_Log( "        %s", e ? e : "(no dlerror)" );
				nFail++;
			}
		}
		IOS_Log( "--- probe done: %d ok, %d failed, %d skipped ---",
				 nOk, nFail, nSkip );
	}
}

// ---------------------------------------------------------------------------
// Fatal error reporting
//
// BUGFIX: the old code did `printf(...); while(1);` on dlopen failure. On a
// device that means: no visible message, no crash report, main thread pegged
// at 100% forever, and the user staring at the grey placeholder window. We
// show the reason and keep the runloop alive so UIKit can actually draw it.
// ---------------------------------------------------------------------------

void IOS_FatalError( const char *title, const char *msg )
{
	IOS_Log( "FATAL: %s", title );
	IOS_Log( "%s", msg );
	IOS_Log( "(log saved to %s)", g_logPath );

	if( g_logFile )
		fflush( g_logFile );

	@autoreleasepool {
		NSString *nsTitle = [NSString stringWithUTF8String:title] ?: @"Fatal error";
		NSString *nsMsg   = [NSString stringWithUTF8String:msg] ?: @"(no detail)";
		NSString *full    = [NSString stringWithFormat:
							 @"%@\n\nLog written to Documents/launch_log.txt "
							 @"(open the Files app to retrieve it).", nsMsg];

		// Show the on-screen log so the detail is readable even behind the alert.
		IOS_ShowOnScreenLog();
		IOS_AppendOnScreen( "" );
		IOS_AppendOnScreen( "*** FATAL ***" );
		IOS_AppendOnScreen( title );
		IOS_AppendOnScreen( msg );

		if( g_controller )
		{
			UIAlertController *ac =
				[UIAlertController alertControllerWithTitle:nsTitle
													message:full
											 preferredStyle:UIAlertControllerStyleAlert];
			[ac addAction:[UIAlertAction actionWithTitle:@"Copy log"
												   style:UIAlertActionStyleDefault
												 handler:^(UIAlertAction *a) {
				NSString *txt = [NSString stringWithContentsOfFile:
								 [NSString stringWithUTF8String:g_logPath]
														  encoding:NSUTF8StringEncoding
															 error:nil];
				if( txt )
					[UIPasteboard generalPasteboard].string = txt;
			}]];
			[ac addAction:[UIAlertAction actionWithTitle:@"Quit"
												   style:UIAlertActionStyleDestructive
												 handler:^(UIAlertAction *a) {
				exit( 1 );
			}]];
			[g_controller presentViewController:ac animated:YES completion:nil];
		}

		// Spin the runloop (NOT a busy `while(1)`) so UIKit keeps drawing and
		// the user can read/copy the error instead of watching a frozen screen.
		while( 1 )
		{
			@autoreleasepool {
				[[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
										 beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
			}
		}
	}
}

// ---------------------------------------------------------------------------
// On-screen log view
// ---------------------------------------------------------------------------

void IOS_ShowOnScreenLog( void )
{
	if( g_bOnScreenLog )
		return;

	dispatch_block_t work = ^{
		if( !g_controller )
			return;
		CGRect b = [[UIScreen mainScreen] bounds];
		g_logView = [[UITextView alloc] initWithFrame:CGRectInset( b, 8, 28 )];
		g_logView.backgroundColor = [UIColor blackColor];
		g_logView.textColor       = [UIColor greenColor];
		g_logView.font            = [UIFont fontWithName:@"Menlo" size:9.0];
		g_logView.editable        = NO;
		g_logView.text            = @"";
		g_logView.autoresizingMask = UIViewAutoresizingFlexibleWidth |
									 UIViewAutoresizingFlexibleHeight;
		[[g_controller view] addSubview:g_logView];

		// Backfill whatever is already in the log file.
		NSString *txt = [NSString stringWithContentsOfFile:
						 [NSString stringWithUTF8String:g_logPath]
												  encoding:NSUTF8StringEncoding
													 error:nil];
		if( txt )
			g_logView.text = txt;
	};

	if( [NSThread isMainThread] )
		work();
	else
		dispatch_sync( dispatch_get_main_queue(), work );

	g_bOnScreenLog = 1;
}

// ---------------------------------------------------------------------------
// Root window
// ---------------------------------------------------------------------------

void IOS_PrepareView( void )
{
	g_window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
	g_controller = [[UIViewController alloc] init];

	// Grey, as before: this is the pre-engine placeholder. Once the engine
	// takes over it draws into its own layer. Keeping the original colour so
	// "grey" still means "engine has not rendered yet" and is comparable with
	// earlier builds.
	[[g_controller view] setBackgroundColor:[UIColor grayColor]];
	[g_window setRootViewController:g_controller];
	[g_window makeKeyAndVisible];

	if( g_iOSVer >= 13.0 )
		isdark = ( [[g_controller traitCollection] userInterfaceStyle] == UIUserInterfaceStyleDark );
	else
		isdark = false;
}

// ---------------------------------------------------------------------------
// Launch dialog
// ---------------------------------------------------------------------------

void IOS_LaunchDialog( void )
{
	@autoreleasepool {
		NSString *ver = [[UIDevice currentDevice] systemVersion];
		g_iOSVer = [ver floatValue];

		// --- paths & environment -------------------------------------------
		// BUGFIX: the old code built exec_dir in a *stack* buffer, appended
		// "/extras_dir.vpk" to it, and then stored that same pointer into
		// szArgv[0]. So argv[0] was (a) a dangling stack address and (b) the
		// vpk path rather than the bundle path. Use a static buffer and keep
		// the bundle path pristine.
		static char exec_dir[PATH_MAX];
		static char vpk_path[PATH_MAX];

		strlcpy( exec_dir, IOS_GetExecDir(), sizeof( exec_dir ) );
		snprintf( vpk_path, sizeof( vpk_path ), "%s/extras_dir.vpk", exec_dir );

		setenv( "APP_LIB_PATH", exec_dir, 1 );
		setenv( "APP_MOD_LIB", exec_dir, 1 );
		setenv( "EXTRAS_VPK_PATH", vpk_path, 1 );
		setenv( "VALVE_GAME_PATH", IOS_GetDocsDir(), 1 );

		IOS_Log( "APP_LIB_PATH   = %s", exec_dir );
		IOS_Log( "EXTRAS_VPK_PATH= %s", vpk_path );
		IOS_Log( "VALVE_GAME_PATH= %s", IOS_GetDocsDir() );

		IOS_PrepareView();

		const char *docsDir = IOS_GetDocsDir();

		// working directory = Documents, so engine-relative writes land there
		NSString *workingDir = [NSString stringWithUTF8String:docsDir];
		if( ![[NSFileManager defaultManager] changeCurrentDirectoryPath:workingDir] )
			IOS_Log( "WARNING: chdir to %s failed", docsDir );

		char settingspath[PATH_MAX];
		snprintf( settingspath, sizeof( settingspath ), "%s/settings.bin", docsDir );

		settings_t settings;
		memset( &settings, 0, sizeof( settings ) );

		// --- dialog ---------------------------------------------------------
		int button = -1, bExit, bStart;
		UIAlertView *alert = [[UIAlertView alloc] init];
		bExit  = [alert addButtonWithTitle:@"Exit"];
		bStart = [alert addButtonWithTitle:@"Start"];

		XashPromptAlertViewDelegate *delegate = [[XashPromptAlertViewDelegate alloc] init];
		delegate.button = &button;
		alert.delegate = delegate;

		[alert setTransform:CGAffineTransformMakeTranslation( 0, 109 )];

		UIScrollView *scroll = [[UIScrollView alloc] initWithFrame:CGRectMake( 0, 0, 300, 230 )];

		UILabel *argstitle = [[UILabel alloc] initWithFrame:CGRectMake( 0, 0, 300, 30 )];
		[argstitle setText:@"Command-line arguments:"];

		UITextField *args = [[UITextField alloc] initWithFrame:CGRectMake( 0, 30, 300, 30 )];
		[args setBackgroundColor:isdark ? [UIColor blackColor] : [UIColor whiteColor]];
		[args setTextColor:isdark ? [UIColor whiteColor] : [UIColor blackColor]];
		[args setAutocorrectionType:UITextAutocorrectionTypeNo];
		[args setAutocapitalizationType:UITextAutocapitalizationTypeNone];

		UILabel *suffixtitle = [[UILabel alloc] initWithFrame:CGRectMake( 0, 90, 140, 30 )];
		[suffixtitle setText:@"Library suffix"];

		UITextField *suffix = [[UITextField alloc] initWithFrame:CGRectMake( 140, 90, 160, 30 )];
		[suffix setBackgroundColor:isdark ? [UIColor blackColor] : [UIColor whiteColor]];
		[suffix setTextColor:isdark ? [UIColor whiteColor] : [UIColor blackColor]];
		[suffix setAutocorrectionType:UITextAutocorrectionTypeNo];
		[suffix setAutocapitalizationType:UITextAutocapitalizationTypeNone];

		UILabel *diagtitle = [[UILabel alloc] initWithFrame:CGRectMake( 0, 140, 220, 30 )];
		[diagtitle setText:@"Show boot log on screen"];

		UISwitch *diagSwitch = [[UISwitch alloc] initWithFrame:CGRectMake( 230, 140, 60, 30 )];
		[diagSwitch setOn:YES];   // default ON: this build exists to diagnose

		[scroll addSubview:argstitle];
		[scroll addSubview:args];
		[scroll addSubview:suffix];
		[scroll addSubview:suffixtitle];
		[scroll addSubview:diagtitle];
		[scroll addSubview:diagSwitch];

		FILE *settingsfile = fopen( settingspath, "rb" );
		if( settingsfile && ( fread( &settings, sizeof( settings ), 1, settingsfile ) == 1 )
			&& ( settings.magic == SETTINGS_MAGIC ) )
		{
			settings.args[1023] = 0;
			settings.suffix[31] = 0;
			[args setText:@(settings.args)];
			[suffix setText:@(settings.suffix)];
		}
		else
		{
			if( strcmp( IOS_DEFAULT_GAME, "hl2" ) == 0 )
				[args setText:@"-dev 2 -log -condebug"];
			else
				[args setText:[NSString stringWithFormat:@"-game %s -dev 2 -log -condebug",
							   IOS_DEFAULT_GAME]];
		}
		if( settingsfile )
			fclose( settingsfile );

		scroll.contentSize = CGSizeMake( 250, 230 );
		[alert setValue:scroll forKey:@"accessoryView"];
		[alert show];

		@autoreleasepool {
			while( button == -1 ) {
				[[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
										 beforeDate:[NSDate distantFuture]];
			}
		}

		if( ( settingsfile = fopen( settingspath, "wb" ) ) )
		{
			strlcpy( settings.args, [args.text UTF8String] ?: "", sizeof( settings.args ) );
			strlcpy( settings.suffix, [suffix.text UTF8String] ?: "", sizeof( settings.suffix ) );
			settings.magic = SETTINGS_MAGIC;
			fwrite( &settings, sizeof( settings ), 1, settingsfile );
			fclose( settingsfile );
		}

		if( button == bExit )
		{
			IOS_Log( "Exit selected" );
			exit( 0 );
		}

		if( [diagSwitch isOn] )
			IOS_ShowOnScreenLog();

		// --- build argv -----------------------------------------------------
		// Drop empty tokens: "-game csgo  -dev 2" used to produce a stray ""
		// argument, which CommandLine() then treats as a positional parameter.
		NSArray *raw = [args.text componentsSeparatedByString:@" "];
		NSMutableArray *argvList = [NSMutableArray array];
		for( NSString *a in raw )
		{
			NSString *t = [a stringByTrimmingCharactersInSet:
						   [NSCharacterSet whitespaceCharacterSet]];
			if( [t length] > 0 )
				[argvList addObject:t];
		}

		int count = (int)[argvList count];
		szArgv = calloc( count + 2, sizeof( char * ) );
		if( szArgv == NULL )
		{
			IOS_FatalError( "Out of memory", "calloc for argv failed" );
			return;
		}

		szArgv[0] = exec_dir;                      // static: stays valid
		for( int i = 0; i < count; i++ )
			szArgv[i + 1] = strdup( [argvList[i] UTF8String] );
		szArgv[count + 1] = NULL;
		szArgc = count + 1;

		// BUGFIX: g_szLibrarySuffix was declared but never populated.
		if( [suffix.text length] > 0 )
			g_szLibrarySuffix = strdup( [suffix.text UTF8String] );

		IOS_Log( "argc = %d", szArgc );
		for( int i = 0; i < szArgc; i++ )
			IOS_Log( "  argv[%d] = %s", i, szArgv[i] ? szArgv[i] : "(null)" );

		alert.delegate = nil;
	}
}

// BUGFIX: the old version had no return statement on the szArgv == NULL path,
// so argc was whatever happened to be in the return register.
int IOS_GetArgs( char ***out )
{
	if( szArgv != NULL )
	{
		*out = szArgv;
		return szArgc;
	}

	IOS_Log( "IOS_GetArgs: szArgv is NULL — launch dialog did not run?" );
	*out = NULL;
	return 0;
}

// iOS starts apps with a soft limit of 256 file descriptors. The filesystem
// keeps every VPK chunk it has read from open (pak01 alone has 80+ chunks),
// and once the limit is reached every fopen() fails, so loose files such as
// scripts/soundscapes_manifest.txt "exist" (stat works) but cannot be opened.
void IOS_RaiseFileLimit( void )
{
	struct rlimit rl;
	if( getrlimit( RLIMIT_NOFILE, &rl ) != 0 )
	{
		IOS_Log( "getrlimit(RLIMIT_NOFILE) failed: %s", strerror( errno ) );
		return;
	}

	rlim_t old = rl.rlim_cur;
	rlim_t want = rl.rlim_max;
	if( want == RLIM_INFINITY || want > OPEN_MAX )
		want = OPEN_MAX;

	rl.rlim_cur = want;
	if( setrlimit( RLIMIT_NOFILE, &rl ) != 0 )
	{
		// some kernels reject values above kern.maxfilesperproc; fall back
		rl.rlim_cur = 4096;
		setrlimit( RLIMIT_NOFILE, &rl );
	}

	getrlimit( RLIMIT_NOFILE, &rl );
	IOS_Log( "file descriptor limit: %llu -> %llu (max %llu)",
			 (unsigned long long)old, (unsigned long long)rl.rlim_cur,
			 (unsigned long long)rl.rlim_max );
}

// Hang finder: a background thread periodically interrupts the main thread
// and has it write its own call stack into the launch log (stdout is
// redirected there). Only async-signal-safe calls are used in the handler.
static pthread_t g_mainThread;

static void IOS_WatchdogSignal( int sig )
{
	(void)sig;
	void *frames[64];
	int n = backtrace( frames, 64 );
	static const char header[] = "\n[watchdog] main thread stack:\n";
	write( STDOUT_FILENO, header, sizeof( header ) - 1 );
	backtrace_symbols_fd( frames, n, STDOUT_FILENO );
}

static void *IOS_WatchdogThread( void *arg )
{
	(void)arg;
	// first dump after the normal startup work, then every 20 seconds
	sleep( 40 );
	for( int i = 0; i < 30; i++ )
	{
		pthread_kill( g_mainThread, SIGUSR2 );
		sleep( 20 );
	}
	return NULL;
}

// Silent deaths leave neither a crash report nor a jetsam event: exit()
// somewhere, or a signal whose default action terminates quietly. Log them.
static void IOS_LogStack( const char *why )
{
	void *frames[64];
	int n = backtrace( frames, 64 );
	write( STDOUT_FILENO, why, strlen( why ) );
	backtrace_symbols_fd( frames, n, STDOUT_FILENO );
}

static void IOS_AtExit( void )
{
	IOS_LogStack( "\n[exit] process is exiting via exit(), stack:\n" );
}

static void IOS_FatalSignal( int sig )
{
	char msg[96];
	snprintf( msg, sizeof( msg ), "\n[signal] terminating signal %d received, stack:\n", sig );
	IOS_LogStack( msg );
	signal( sig, SIG_DFL );
	raise( sig );
}

void IOS_StartWatchdog( void )
{
	g_mainThread = pthread_self();

	// Writing to a socket whose peer went away raises SIGPIPE, which kills
	// the process without a crash report. Get EPIPE from the call instead.
	signal( SIGPIPE, SIG_IGN );

	atexit( IOS_AtExit );
	const int quietSignals[] = { SIGTERM, SIGHUP, SIGINT, SIGQUIT, SIGUSR1, SIGALRM, SIGVTALRM, SIGPROF, SIGXCPU, SIGXFSZ, SIGSYS };
	for( size_t i = 0; i < sizeof( quietSignals ) / sizeof( quietSignals[0] ); i++ )
		signal( quietSignals[i], IOS_FatalSignal );

	struct sigaction sa;
	memset( &sa, 0, sizeof( sa ) );
	sa.sa_handler = IOS_WatchdogSignal;
	sa.sa_flags = SA_RESTART;
	sigemptyset( &sa.sa_mask );
	sigaction( SIGUSR2, &sa, NULL );

	pthread_t t;
	if( pthread_create( &t, NULL, IOS_WatchdogThread, NULL ) == 0 )
	{
		pthread_detach( t );
		IOS_Log( "watchdog: main thread stack dumps every 20s after 40s" );
	}
}
