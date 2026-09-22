/*
 * Minimal fontconfig for iOS.
 *
 * CS:GO's Linux font code (vgui2/vgui_surfacelib/linuxfont.cpp), which the
 * iOS build uses, finds font files through fontconfig. iOS has no fontconfig,
 * so this implements the handful of calls that code makes: fonts are
 * discovered by scanning directories with FreeType (the system fonts plus
 * directories the game adds, e.g. platform/vgui/fonts) and matched by family
 * name, slant and weight. Like real fontconfig, a match always returns some
 * font when any font is known.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H

/* ---- patterns: a small list of (object, value) pairs ---- */

typedef struct
{
	char *object;
	FcValue value;
} FcElt;

struct _FcPattern
{
	int nelt;
	int selt;
	FcElt *elts;
};

static char *dupstr( const char *s )
{
	size_t n = strlen( s ) + 1;
	char *d = (char *)malloc( n );
	if ( d )
		memcpy( d, s, n );
	return d;
}

FcPattern *FcPatternCreate( void )
{
	return (FcPattern *)calloc( 1, sizeof( FcPattern ) );
}

void FcPatternDestroy( FcPattern *p )
{
	int i;
	if ( !p )
		return;
	for ( i = 0; i < p->nelt; i++ )
	{
		free( p->elts[i].object );
		if ( p->elts[i].value.type == FcTypeString )
			free( (void *)p->elts[i].value.u.s );
	}
	free( p->elts );
	free( p );
}

FcBool FcPatternAdd( FcPattern *p, const char *object, FcValue value, FcBool append )
{
	FcElt *e;
	(void)append;
	if ( !p || !object )
		return FcFalse;
	if ( p->nelt == p->selt )
	{
		int n = p->selt ? p->selt * 2 : 8;
		FcElt *elts = (FcElt *)realloc( p->elts, n * sizeof( FcElt ) );
		if ( !elts )
			return FcFalse;
		p->elts = elts;
		p->selt = n;
	}
	e = &p->elts[p->nelt++];
	e->object = dupstr( object );
	e->value = value;
	if ( value.type == FcTypeString && value.u.s )
		e->value.u.s = (const FcChar8 *)dupstr( (const char *)value.u.s );
	return FcTrue;
}

static const FcValue *pattern_get( const FcPattern *p, const char *object, int n, FcType type )
{
	int i;
	if ( !p )
		return NULL;
	for ( i = 0; i < p->nelt; i++ )
	{
		if ( strcmp( p->elts[i].object, object ) || p->elts[i].value.type != type )
			continue;
		if ( n-- == 0 )
			return &p->elts[i].value;
	}
	return NULL;
}

FcResult FcPatternGetString( const FcPattern *p, const char *object, int n, FcChar8 **s )
{
	const FcValue *v = pattern_get( p, object, n, FcTypeString );
	if ( !v )
		return FcResultNoMatch;
	*s = (FcChar8 *)v->u.s;
	return FcResultMatch;
}

FcResult FcPatternGetInteger( const FcPattern *p, const char *object, int n, int *i )
{
	const FcValue *v = pattern_get( p, object, n, FcTypeInteger );
	if ( !v )
		return FcResultNoMatch;
	*i = v->u.i;
	return FcResultMatch;
}

FcResult FcPatternGetBool( const FcPattern *p, const char *object, int n, FcBool *b )
{
	const FcValue *v = pattern_get( p, object, n, FcTypeBool );
	if ( !v )
		return FcResultNoMatch;
	*b = v->u.b;
	return FcResultMatch;
}

static void add_string( FcPattern *p, const char *object, const char *s )
{
	FcValue v;
	v.type = FcTypeString;
	v.u.s = (const FcChar8 *)s;
	FcPatternAdd( p, object, v, FcTrue );
}

static void add_int( FcPattern *p, const char *object, int i )
{
	FcValue v;
	v.type = FcTypeInteger;
	v.u.i = i;
	FcPatternAdd( p, object, v, FcTrue );
}

static void add_bool( FcPattern *p, const char *object, FcBool b )
{
	FcValue v;
	v.type = FcTypeBool;
	v.u.b = b;
	FcPatternAdd( p, object, v, FcTrue );
}

/* ---- object sets (only used to shape FcFontList output) ---- */

FcObjectSet *FcObjectSetCreate( void )
{
	return (FcObjectSet *)calloc( 1, sizeof( FcObjectSet ) );
}

FcBool FcObjectSetAdd( FcObjectSet *os, const char *object )
{
	(void)os;
	(void)object;
	return FcTrue;
}

void FcObjectSetDestroy( FcObjectSet *os )
{
	free( os );
}

/* ---- font database ---- */

typedef struct
{
	char *file;
	char *family;
	char *fullname;
	int index;
	int slant;
	int weight;
	FcBool scalable;
} FontEntry;

static FontEntry *g_fonts;
static int g_nfonts, g_sfonts;
static FT_Library g_ft;

static int has_font_ext( const char *name )
{
	const char *dot = strrchr( name, '.' );
	return dot && ( !strcasecmp( dot, ".ttf" ) || !strcasecmp( dot, ".otf" ) || !strcasecmp( dot, ".ttc" ) );
}

static void add_font_file( const char *path )
{
	FT_Face face;
	FT_Long nfaces, i;
	int k;

	for ( k = 0; k < g_nfonts; k++ )
	{
		if ( !strcmp( g_fonts[k].file, path ) )
			return;
	}
	if ( FT_New_Face( g_ft, path, 0, &face ) )
		return;
	nfaces = face->num_faces;
	FT_Done_Face( face );

	for ( i = 0; i < nfaces; i++ )
	{
		FontEntry *e;
		char fullname[512];

		if ( FT_New_Face( g_ft, path, i, &face ) )
			continue;
		if ( !face->family_name )
		{
			FT_Done_Face( face );
			continue;
		}
		if ( g_nfonts == g_sfonts )
		{
			int n = g_sfonts ? g_sfonts * 2 : 64;
			FontEntry *fonts = (FontEntry *)realloc( g_fonts, n * sizeof( FontEntry ) );
			if ( !fonts )
			{
				FT_Done_Face( face );
				return;
			}
			g_fonts = fonts;
			g_sfonts = n;
		}
		e = &g_fonts[g_nfonts++];
		e->file = dupstr( path );
		e->family = dupstr( face->family_name );
		if ( face->style_name )
		{
			snprintf( fullname, sizeof( fullname ), "%s %s", face->family_name, face->style_name );
			e->fullname = dupstr( fullname );
		}
		else
		{
			e->fullname = dupstr( face->family_name );
		}
		e->index = (int)i;
		e->slant = ( face->style_flags & FT_STYLE_FLAG_ITALIC ) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
		e->weight = ( face->style_flags & FT_STYLE_FLAG_BOLD ) ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL;
		e->scalable = FT_IS_SCALABLE( face ) ? FcTrue : FcFalse;
		FT_Done_Face( face );
	}
}

static void scan_dir( const char *dir, int depth )
{
	DIR *d;
	struct dirent *ent;

	if ( depth > 4 || !( d = opendir( dir ) ) )
		return;
	while ( ( ent = readdir( d ) ) )
	{
		char path[1024];
		struct stat st;

		if ( ent->d_name[0] == '.' )
			continue;
		snprintf( path, sizeof( path ), "%s/%s", dir, ent->d_name );
		if ( stat( path, &st ) )
			continue;
		if ( S_ISDIR( st.st_mode ) )
			scan_dir( path, depth + 1 );
		else if ( has_font_ext( ent->d_name ) )
			add_font_file( path );
	}
	closedir( d );
}

static struct _FcConfig
{
	int unused;
} g_config;

FcBool FcInit( void )
{
	if ( g_ft )
		return FcTrue;
	if ( FT_Init_FreeType( &g_ft ) )
		return FcFalse;
	scan_dir( "/System/Library/Fonts", 0 );
	return FcTrue;
}

FcConfig *FcConfigGetCurrent( void )
{
	return &g_config;
}

FcBool FcConfigAppFontAddDir( FcConfig *config, const FcChar8 *dir )
{
	(void)config;
	if ( !FcInit() || !dir )
		return FcFalse;
	scan_dir( (const char *)dir, 0 );
	return FcTrue;
}

FcBool FcConfigAppFontAddFile( FcConfig *config, const FcChar8 *file )
{
	(void)config;
	if ( !FcInit() || !file )
		return FcFalse;
	add_font_file( (const char *)file );
	return FcTrue;
}

static FcPattern *entry_pattern( const FontEntry *e )
{
	FcPattern *p = FcPatternCreate();
	if ( !p )
		return NULL;
	add_string( p, FC_FILE, e->file );
	add_string( p, FC_FAMILY, e->family );
	add_string( p, FC_FULLNAME, e->fullname );
	add_int( p, FC_INDEX, e->index );
	add_int( p, FC_SLANT, e->slant );
	add_int( p, FC_WEIGHT, e->weight );
	add_bool( p, FC_SCALABLE, e->scalable );
	add_bool( p, FC_OUTLINE, e->scalable );
	return p;
}

FcFontSet *FcFontList( FcConfig *config, FcPattern *p, FcObjectSet *os )
{
	FcFontSet *fs;
	int i;
	(void)config;
	(void)p;
	(void)os;

	FcInit();
	fs = (FcFontSet *)calloc( 1, sizeof( FcFontSet ) );
	if ( !fs )
		return NULL;
	fs->fonts = (FcPattern **)calloc( g_nfonts ? g_nfonts : 1, sizeof( FcPattern * ) );
	fs->sfont = g_nfonts;
	for ( i = 0; i < g_nfonts; i++ )
	{
		FcPattern *fp = entry_pattern( &g_fonts[i] );
		if ( fp )
			fs->fonts[fs->nfont++] = fp;
	}
	return fs;
}

void FcFontSetDestroy( FcFontSet *fs )
{
	int i;
	if ( !fs )
		return;
	for ( i = 0; i < fs->nfont; i++ )
		FcPatternDestroy( fs->fonts[i] );
	free( fs->fonts );
	free( fs );
}

FcBool FcConfigSubstitute( FcConfig *config, FcPattern *p, FcMatchKind kind )
{
	(void)config;
	(void)p;
	(void)kind;
	return FcTrue;
}

void FcDefaultSubstitute( FcPattern *p )
{
	(void)p;
}

/* lower is better; exact family dominates, then slant, then weight */
static int match_score( const FontEntry *e, const char *family, int slant, int weight )
{
	int score = 0;
	if ( family )
	{
		if ( strcasecmp( e->family, family ) )
			score += strcasestr( e->family, family ) ? 1000 : 100000;
	}
	if ( e->slant != slant )
		score += 100;
	score += abs( e->weight - weight ) / 10;
	if ( !e->scalable )
		score += 10000;
	return score;
}

FcPattern *FcFontMatch( FcConfig *config, FcPattern *p, FcResult *result )
{
	FcChar8 *family = NULL;
	int slant = FC_SLANT_ROMAN, weight = FC_WEIGHT_NORMAL;
	int i, best = -1, bestScore = 0;
	(void)config;

	FcInit();
	FcPatternGetString( p, FC_FAMILY, 0, &family );
	FcPatternGetInteger( p, FC_SLANT, 0, &slant );
	FcPatternGetInteger( p, FC_WEIGHT, 0, &weight );

	for ( i = 0; i < g_nfonts; i++ )
	{
		int score = match_score( &g_fonts[i], (const char *)family, slant, weight );
		if ( best < 0 || score < bestScore )
		{
			best = i;
			bestScore = score;
		}
	}

	if ( best < 0 )
	{
		if ( result )
			*result = FcResultNoMatch;
		return NULL;
	}
	if ( result )
		*result = FcResultMatch;
	return entry_pattern( &g_fonts[best] );
}
