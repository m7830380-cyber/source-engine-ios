//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $  
//=============================================================================//

#include <dirent.h>
#include <sys/stat.h>
#include <strings.h>

#include "tier1/strtools.h"
#include "tier0/memdbgoff.h"
#include "linux_support.h"

#ifdef OSX
#include <AvailabilityMacros.h>
#endif

char selectBuf[PATH_MAX];

#if defined(OSX) && !defined(MAC_OS_X_VERSION_10_9)
int FileSelect(struct dirent *ent)
#else
int FileSelect(const struct dirent *ent)
#endif
{
	const char *mask=selectBuf;
	const char *name=ent->d_name;
	
	//printf("Test:%s %s\n",mask,name);
	
	if(!strcmp(name,".") || !strcmp(name,"..") ) return 0;
	
	if(!strcmp(selectBuf,"*.*")) return 1;
	
	while( *mask && *name )
	{
		if(*mask=='*')
		{
			mask++; // move to the next char in the mask
			if(!*mask) // if this is the end of the mask its a match 
			{
				return 1;
			}
			while(*name && toupper(*name)!=toupper(*mask)) 
			{ // while the two don't meet up again
				name++;
			}
			if(!*name) 
			{ // end of the name
				break; 
			}
		}
		else if (*mask!='?')
		{
			if( toupper(*mask) != toupper(*name) )
			{	// mismatched!
				return 0;
			}
			else
			{	
				mask++;
				name++;
				if( !*mask && !*name) 
				{ // if its at the end of the buffer
					return 1;
				}
				
			}
			
		}
		else /* mask is "?", we don't care*/
		{
			mask++;
			name++;
		}
	}	
	
	return( !*mask && !*name ); // both of the strings are at the end
}

int FillDataStruct(FIND_DATA *dat)
{
	struct stat fileStat;
	
	if(dat->numMatches<0)
		return -1;
	
	char szFullPath[MAX_PATH];
	Q_snprintf( szFullPath, sizeof(szFullPath), "%s/%s", dat->cBaseDir, dat->namelist[dat->numMatches]->d_name );  
	
	if(!stat(szFullPath,&fileStat))
	{
		dat->dwFileAttributes=fileStat.st_mode;           
	}
	else
	{
		dat->dwFileAttributes=0;
	}	
	
	// now just put the filename in the output data
	Q_snprintf( dat->cFileName, sizeof(dat->cFileName), "%s", dat->namelist[dat->numMatches]->d_name );  
	
	//printf("%s\n", dat->namelist[dat->numMatches]->d_name);
	free(dat->namelist[dat->numMatches]);
	
  	dat->numMatches--;
	return 1;
}


HANDLE FindFirstFile( const char *fileName, FIND_DATA *dat)
{
	char nameStore[PATH_MAX];
	char *dir=NULL;
	int n,iret=-1;
	
	Q_strncpy(nameStore,fileName, sizeof( nameStore ) );
	
	if(strrchr(nameStore,'/') )
	{
		dir=nameStore;
		while(strrchr(dir,'/') )
		{
			struct stat dirChk;
			
			// zero this with the dir name
			dir=strrchr(nameStore,'/');
			*dir='\0';
			
			dir=nameStore;
			stat(dir,&dirChk);
			
			if( S_ISDIR( dirChk.st_mode ) )
			{
				break;	
			}
		}
	}
	else
	{
		// couldn't find a dir seperator...
		return (HANDLE)-1;
	}
	
	if( strlen(dir)>0 )
	{
		Q_strncpy(selectBuf,fileName+strlen(dir)+1, sizeof( selectBuf ) );
		Q_strncpy(dat->cBaseDir,dir, sizeof( dat->cBaseDir ) );
		dat->namelist = NULL;
		n = scandir(dir, &dat->namelist, FileSelect, alphasort);
		if (n < 0)
		{
			// silently return, nothing interesting
			dat->namelist = NULL;
		}
		else 
		{
			dat->numMatches=n-1; // n is the number of matches
			iret=FillDataStruct(dat);
			if ( ( iret<0 ) && dat->namelist )
			{
				free(dat->namelist);
				dat->namelist = NULL;
			}
			
		}
	}
	
	//	printf("Returning: %i \n",iret);
	return (HANDLE)(intp)iret;
}

bool FindNextFile(HANDLE handle, FIND_DATA *dat)
{
	if(dat->numMatches<0)
	{	
		if ( dat->namelist != NULL )
		{
			free( dat->namelist );
			dat->namelist = NULL;
		}
		return false; // no matches left
	}	
	
	FillDataStruct(dat);
	return true;
}

bool FindClose(HANDLE handle)
{
	return true;
}



// Case-insensitive lookup for case-sensitive file systems (Linux, iOS app
// containers). Returns the path as it exists on disk in pFileNameOut
// (MAX_PATH), or NULL if there is no such file.
//
// The old version only matched the last component (scandir of the parent,
// which had to exist with the right case) and otherwise lowercased the whole
// path, so "csgo/Resource/valve_english.txt" (the file is resource/...) failed
// and absolute paths got broken; it also passed the name to scandir's filter
// through a static, which isn't thread-safe.
static bool FindEntryCaseInsensitive( const char *pszDir, const char *pszName, char *pszOut, size_t nOut )
{
	DIR *pDir = opendir( pszDir[0] ? pszDir : "." );
	if ( !pDir )
		return false;
	bool bFound = false;
	while ( struct dirent *pEnt = readdir( pDir ) )
	{
		if ( !strcasecmp( pEnt->d_name, pszName ) )
		{
			Q_strncpy( pszOut, pEnt->d_name, nOut );
			bFound = true;
			break;
		}
	}
	closedir( pDir );
	return bFound;
}

const char *findFileInDirCaseInsensitive(const char *file, char *pFileNameOut)
{
	if ( !file || !file[0] )
		return NULL;

	char szPath[MAX_PATH];
	Q_strncpy( szPath, file, sizeof( szPath ) );
	for ( char *p = szPath; *p; ++p )
	{
		if ( *p == '\\' )
			*p = '/';
	}

	struct stat st;
	char szEntry[MAX_PATH];

	// fast path: the directory exists as written, only the file name's case is off
	char *pSlash = strrchr( szPath, '/' );
	if ( pSlash && pSlash != szPath )
	{
		*pSlash = 0;
		bool bDirExists = stat( szPath, &st ) == 0;
		if ( bDirExists )
		{
			bool bFound = FindEntryCaseInsensitive( szPath, pSlash + 1, szEntry, sizeof( szEntry ) );
			if ( bFound )
				Q_snprintf( pFileNameOut, MAX_PATH, "%s/%s", szPath, szEntry );
			*pSlash = '/';
			return bFound ? pFileNameOut : NULL;
		}
		*pSlash = '/';
	}

	// walk the path, fixing the case of each component that doesn't exist as written
	char szOut[MAX_PATH];
	szOut[0] = 0;
	const char *p = szPath;
	if ( *p == '/' )
	{
		Q_strncpy( szOut, "/", sizeof( szOut ) );
		++p;
	}
	while ( *p )
	{
		const char *pEnd = strchr( p, '/' );
		size_t nLen = pEnd ? (size_t)( pEnd - p ) : strlen( p );
		if ( nLen > 0 && nLen < sizeof( szEntry ) )
		{
			memcpy( szEntry, p, nLen );
			szEntry[nLen] = 0;

			size_t nOutLen = strlen( szOut );
			const char *pszSep = ( nOutLen && szOut[nOutLen - 1] != '/' ) ? "/" : "";
			char szCandidate[MAX_PATH];
			Q_snprintf( szCandidate, sizeof( szCandidate ), "%s%s%s", szOut, pszSep, szEntry );
			if ( stat( szCandidate, &st ) != 0 )
			{
				char szReal[MAX_PATH];
				if ( !FindEntryCaseInsensitive( szOut, szEntry, szReal, sizeof( szReal ) ) )
					return NULL;
				Q_snprintf( szCandidate, sizeof( szCandidate ), "%s%s%s", szOut, pszSep, szReal );
			}
			Q_strncpy( szOut, szCandidate, sizeof( szOut ) );
		}
		if ( !pEnd )
			break;
		p = pEnd + 1;
	}

	Q_strncpy( pFileNameOut, szOut, MAX_PATH );
	return pFileNameOut;
}

