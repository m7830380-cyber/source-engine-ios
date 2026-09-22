//========= Source Engine iOS modernization ==============================//
//
// Purpose: Reconcile vertex/fragment varyings before linking.
//          See glmvaryingfixup.h for the full rationale.
//
//=======================================================================//

#include "togles/rendermechanism.h"
#include "glmvaryingfixup.h"

#include "tier0/dbg.h"
#include "tier1/strtools.h"

#include <stdlib.h>
#include <string.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// oT0..oT31 is the full DX9 interpolator range.
#define GLMVARYING_MAX_INDEX	32

//-----------------------------------------------------------------------------
// Scan GLSL for varying declarations of the form
//
//     out vec4 oT3;
//     centroid out vec4 oT2;
//     in vec4 oT6;
//     centroid in vec4 oT1;
//
// and set a bit per index found. pszQualifier is "out" for a vertex
// shader or "in" for a fragment shader.
//
// We look for declarations only - a bare mention of oTn in the body does
// not count, because that is a use, not a declaration.
//-----------------------------------------------------------------------------
static unsigned int GLMScanVaryings( const char *pText, const char *pszQualifier )
{
	unsigned int nMask = 0;

	if ( !pText || !pszQualifier )
		return 0;

	const int nQualLen = V_strlen( pszQualifier );
	const char *pCursor = pText;

	while ( ( pCursor = V_strstr( pCursor, pszQualifier ) ) != NULL )
	{
		const char *pTokenStart = pCursor;
		pCursor += nQualLen;

		// Must be a standalone token: the previous character has to be
		// start-of-text or whitespace, so "in" does not match inside
		// "int", "sin", "main" and friends.
		if ( pTokenStart != pText )
		{
			const char chPrev = pTokenStart[ -1 ];
			if ( chPrev != ' ' && chPrev != '\t' && chPrev != '\n' && chPrev != '\r' )
				continue;
		}

		// Expect: <qualifier> vec4 oT<n>;
		const char *p = pCursor;

		while ( *p == ' ' || *p == '\t' )
			p++;

		if ( V_strncmp( p, "vec4", 4 ) != 0 )
			continue;
		p += 4;

		while ( *p == ' ' || *p == '\t' )
			p++;

		if ( p[ 0 ] != 'o' || p[ 1 ] != 'T' )
			continue;
		p += 2;

		if ( *p < '0' || *p > '9' )
			continue;

		const int nIndex = atoi( p );
		if ( nIndex >= 0 && nIndex < GLMVARYING_MAX_INDEX )
			nMask |= ( 1u << nIndex );
	}

	return nMask;
}

//-----------------------------------------------------------------------------
// Find the insertion point for new declarations: just after the last
// #define / precision / #version line at the top of the shader, so we do
// not land before the #version directive (illegal) or inside main().
//-----------------------------------------------------------------------------
static int GLMFindDeclarationInsertOffset( const char *pText )
{
	const char *pBest = pText;
	const char *pLine = pText;

	while ( *pLine )
	{
		const char *pEnd = strchr( pLine, '\n' );
		if ( !pEnd )
			break;

		// Skip leading whitespace for the test.
		const char *pTest = pLine;
		while ( *pTest == ' ' || *pTest == '\t' )
			pTest++;

		if ( V_strncmp( pTest, "#version", 8 ) == 0 ||
			 V_strncmp( pTest, "#define", 7 ) == 0 ||
			 V_strncmp( pTest, "precision", 9 ) == 0 )
		{
			pBest = pEnd + 1;
		}

		pLine = pEnd + 1;
	}

	return (int)( pBest - pText );
}

//-----------------------------------------------------------------------------
// Find "void main" and return the offset just past its opening brace, so
// we can inject the zero-assignments at the very start of main().
//-----------------------------------------------------------------------------
static int GLMFindMainBodyOffset( const char *pText )
{
	const char *pMain = V_strstr( pText, "void main" );
	if ( !pMain )
		return -1;

	const char *pBrace = strchr( pMain, '{' );
	if ( !pBrace )
		return -1;

	return (int)( pBrace - pText ) + 1;
}

//-----------------------------------------------------------------------------
char *GLMFixupMissingVaryings( const char *pVertexText, int nVertexLen,
							   const char *pFragmentText, int nFragmentLen )
{
	if ( !pVertexText || !pFragmentText || nVertexLen <= 0 || nFragmentLen <= 0 )
		return NULL;

	// The source slices are not NUL-terminated, so work on private copies.
	char *pVertexCopy = (char *)malloc( nVertexLen + 1 );
	if ( !pVertexCopy )
		return NULL;
	memcpy( pVertexCopy, pVertexText, nVertexLen );
	pVertexCopy[ nVertexLen ] = '\0';

	char *pFragmentCopy = (char *)malloc( nFragmentLen + 1 );
	if ( !pFragmentCopy )
	{
		free( pVertexCopy );
		return NULL;
	}
	memcpy( pFragmentCopy, pFragmentText, nFragmentLen );
	pFragmentCopy[ nFragmentLen ] = '\0';

	// What does each side declare?
	const unsigned int nVertexOut  = GLMScanVaryings( pVertexCopy, "out" );
	const unsigned int nFragmentIn = GLMScanVaryings( pFragmentCopy, "in" );

	free( pFragmentCopy );

	// Fragment inputs with no matching vertex output are the link errors.
	const unsigned int nMissing = nFragmentIn & ~nVertexOut;
	if ( nMissing == 0 )
	{
		free( pVertexCopy );
		return NULL;
	}

	const int nDeclOffset = GLMFindDeclarationInsertOffset( pVertexCopy );
	const int nMainOffset = GLMFindMainBodyOffset( pVertexCopy );

	if ( nMainOffset < 0 || nMainOffset <= nDeclOffset )
	{
		// Shader is not shaped the way we expect; leave it alone rather
		// than emit something that will not compile.
		free( pVertexCopy );
		return NULL;
	}

	// Build the two injected blocks.
	char szDecls[ 1024 ];
	char szAssigns[ 1024 ];
	szDecls[ 0 ]   = '\0';
	szAssigns[ 0 ] = '\0';

	V_strncat( szDecls, "\n// --- injected: fragment reads these, vertex did not write them ---\n",
			   sizeof( szDecls ) );

	for ( int i = 0; i < GLMVARYING_MAX_INDEX; ++i )
	{
		if ( !( nMissing & ( 1u << i ) ) )
			continue;

		char szLine[ 64 ];

		V_snprintf( szLine, sizeof( szLine ), "out vec4 oT%d;\n", i );
		V_strncat( szDecls, szLine, sizeof( szDecls ) );

		// Defined value rather than garbage, matching what D3D9 callers
		// effectively relied on.
		V_snprintf( szLine, sizeof( szLine ), "\toT%d = vec4( 0.0 );\n", i );
		V_strncat( szAssigns, szLine, sizeof( szAssigns ) );
	}

	const int nOriginalLen = nVertexLen;
	const int nDeclLen     = V_strlen( szDecls );
	const int nAssignLen   = V_strlen( szAssigns );
	// +1 for the newline we splice in before the assignments, +1 for the
	// terminator. Getting this wrong is a heap overflow, so it is spelled
	// out rather than implied.
	const int nNewLen      = nOriginalLen + nDeclLen + nAssignLen + 1 + 1;

	char *pOut = (char *)malloc( nNewLen );
	if ( !pOut )
	{
		free( pVertexCopy );
		return NULL;
	}

	// Splice: [head][decls][middle][assigns][tail]
	int nWritten = 0;

	memcpy( pOut + nWritten, pVertexCopy, nDeclOffset );
	nWritten += nDeclOffset;

	memcpy( pOut + nWritten, szDecls, nDeclLen );
	nWritten += nDeclLen;

	const int nMiddleLen = nMainOffset - nDeclOffset;
	memcpy( pOut + nWritten, pVertexCopy + nDeclOffset, nMiddleLen );
	nWritten += nMiddleLen;

	memcpy( pOut + nWritten, "\n", 1 );
	nWritten += 1;

	memcpy( pOut + nWritten, szAssigns, nAssignLen );
	nWritten += nAssignLen;

	const int nTailLen = nOriginalLen - nMainOffset;
	memcpy( pOut + nWritten, pVertexCopy + nMainOffset, nTailLen );
	nWritten += nTailLen;

	pOut[ nWritten ] = '\0';

	free( pVertexCopy );

	return pOut;
}
