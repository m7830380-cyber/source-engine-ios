/*
 Launchdiag.h - plain-C interface to the iOS launch dialog and boot diagnostics.

 IMPORTANT: this header is included from C++ (launcher_main/main.cpp) which
 also pulls in Valve's basetypes.h. It must therefore stay free of any
 Objective-C or UIKit types — `typedef int BOOL` (Valve) and `typedef bool
 BOOL` (<objc/objc.h>) collide. Plain C declarations only.
 */

#ifndef LAUNCHDIAG_H
#define LAUNCHDIAG_H

#ifdef __cplusplus
extern "C" {
#endif

// Paths. Both return permanently-owned strings (never freed, never stale).
const char *IOS_GetExecDir( void );
const char *IOS_GetDocsDir( void );

// Logging. IOS_LogInit() must be called before anything else; it redirects
// stdout/stderr into Documents/launch_log.txt.
void        IOS_LogInit( void );
void        IOS_Log( const char *fmt, ... ) __attribute__((format(printf,1,2)));
const char *IOS_GetLogPath( void );

// Diagnostics.
void IOS_LogDeviceInfo( void );
void IOS_LogBundleContents( void );
void IOS_ProbeDylibs( void );
void IOS_LogGameContent( void );
void IOS_RaiseFileLimit( void );
void IOS_StartWatchdog( void );
void IOS_ShowOnScreenLog( void );

// Reports the failure on screen and spins the runloop forever. Never returns.
void IOS_FatalError( const char *title, const char *msg ) __attribute__((noreturn));

// Launch dialog / argv.
void IOS_PrepareView( void );
void IOS_LaunchDialog( void );
int  IOS_GetArgs( char ***out );

#ifdef __cplusplus
}
#endif

#endif // LAUNCHDIAG_H
