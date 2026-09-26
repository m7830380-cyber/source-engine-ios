#include "cbase.h"
#include "convar.h"
#include <string.h>
#include "vgui/IInputInternal.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "vgui/ISurface.h"
#include "touch.h"
#include "cdll_int.h"
#include "ienginevgui.h"
#include "in_buttons.h"
#include "filesystem.h"
#include "tier0/icommandline.h"
#include "vgui_controls/Button.h"
#include "viewrender.h"
#include "iinput.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

extern ConVar cl_sidespeed;
extern ConVar cl_forwardspeed;
extern ConVar cl_upspeed;
extern ConVar default_fov;

extern IMatSystemSurface *g_pMatSystemSurface;

#if ANDROID || IOS
#define TOUCH_DEFAULT "1"
#else
#define TOUCH_DEFAULT "0"
#endif

extern ConVar sensitivity;

#define TOUCH_DEFAULT_CFG "touch_default.cfg"
#define MIN_ALPHA_IN_CUTSCENE 20

ConVar touch_enable( "touch_enable", TOUCH_DEFAULT, FCVAR_ARCHIVE );
ConVar touch_draw( "touch_draw", "1", FCVAR_ARCHIVE );
ConVar touch_filter( "touch_filter", "0", FCVAR_ARCHIVE );
ConVar touch_forwardzone( "touch_forwardzone", "0.06", FCVAR_ARCHIVE, "forward touch zone" );
ConVar touch_sidezone( "touch_sidezone", "0.06", FCVAR_ARCHIVE, "side touch zone" );
ConVar touch_pitch( "touch_pitch", "90", FCVAR_ARCHIVE, "touch pitch sensitivity" );
ConVar touch_yaw( "touch_yaw", "120", FCVAR_ARCHIVE, "touch yaw sensitivity" );
ConVar touch_config_file( "touch_config_file", "touch.cfg", FCVAR_ARCHIVE, "current touch profile file" );
ConVar touch_grid_count( "touch_grid_count", "50", FCVAR_ARCHIVE, "touch grid count" );
ConVar touch_grid_enable( "touch_grid_enable", "1", FCVAR_ARCHIVE, "enable touch grid" );
ConVar touch_precise_amount( "touch_precise_amount", "0.5", FCVAR_ARCHIVE, "sensitivity multiplier for precise-look" );

ConVar touch_button_info( "touch_button_info", "0", FCVAR_ARCHIVE );
ConVar touch_debug( "touch_debug", "1", FCVAR_NONE, "log touch presses and the buttons they hit" );

#define boundmax( num, high ) ( (num) < (high) ? (num) : (high) )
#define boundmin( num, low )  ( (num) >= (low) ? (num) : (low)  )
#define bound( low, num, high ) ( boundmin( boundmax(num, high), low ))
#define S

extern IVEngineClient *engine;

CTouchControls gTouch;
static VTouchPanel g_TouchPanel;
VTouchPanel *touch_panel = &g_TouchPanel;

CTouchPanel::CTouchPanel( vgui::VPANEL parent ) : BaseClass( NULL, "TouchPanel" )
{
	SetParent( parent );

	int w, h;
	engine->GetScreenSize(w, h);
	SetBounds( 0, 0, w, h );

	SetFgColor( Color( 0, 0, 0, 255 ) );
	SetPaintBackgroundEnabled( false );

	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( false );

	SetVisible( true );
}

void CTouchPanel::Paint()
{
	static int s_nPaints = 0;
	if ( ( s_nPaints++ % 600 ) == 0 )
	{
		int x, y, w, h;
		GetBounds( x, y, w, h );
		printf( "[touch] panel paint #%d, bounds %d %d %d %d, gameui visible %d, touch_enable %d\n",
				s_nPaints, x, y, w, h, enginevgui->IsGameUIVisible() ? 1 : 0, touch_enable.GetInt() );
	}
	gTouch.Frame();
}

void CTouchPanel::OnScreenSizeChanged(int iOldWide, int iOldTall)
{
	BaseClass::OnScreenSizeChanged(iOldWide, iOldTall);

	int w,h;
	w = ScreenWidth();
	h = ScreenHeight();
	gTouch.screen_w = ScreenWidth(); gTouch.screen_h = h;

	SetBounds( 0, 0, w, h );
}

void CTouchPanel::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	int w,h;
	w = ScreenWidth();
	h = ScreenHeight();
	gTouch.screen_w = ScreenWidth(); gTouch.screen_h = h;

	SetBounds( 0, 0, w, h );
}

// Weapon cycling without a HUD: invnext/invprev are handled by CS:GO's
// Scaleform weapon selection panel, which does not exist on iOS. Select the
// weapon directly, the way the HUD does it (via the usercmd).
static int WeaponSortKey( C_BaseCombatWeapon *pWeapon )
{
	return pWeapon->GetSlot() * 100 + pWeapon->GetPosition();
}

static void CycleWeapon( int nDir )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer || !pPlayer->IsAlive() )
		return;

	CUtlVector< C_BaseCombatWeapon * > weapons;
	for ( int i = 0; i < pPlayer->WeaponCount(); i++ )
	{
		C_BaseCombatWeapon *pWeapon = pPlayer->GetWeapon( i );
		if ( pWeapon && pWeapon->CanBeSelected() )
		{
			// insertion sort by HUD slot/position
			int j = 0;
			while ( j < weapons.Count() && WeaponSortKey( weapons[j] ) <= WeaponSortKey( pWeapon ) )
				j++;
			weapons.InsertBefore( j, pWeapon );
		}
	}
	if ( weapons.Count() < 2 )
		return;

	int nActive = weapons.Find( pPlayer->GetActiveWeapon() );
	int nNext = ( nActive < 0 ) ? 0 : ( nActive + nDir + weapons.Count() ) % weapons.Count();
	::input->MakeWeaponSelection( weapons[nNext] );
}

CON_COMMAND( ios_weapnext, "select the next weapon (no weapon selection HUD on iOS)" )
{
	CycleWeapon( 1 );
}

CON_COMMAND( ios_weapprev, "select the previous weapon (no weapon selection HUD on iOS)" )
{
	CycleWeapon( -1 );
}

CON_COMMAND( touch_addbutton, "add native touch button" )
{
	rgba_t color;
	int argc = args.ArgC();

	if( argc >= 12 )
	{
		float aspect = 1.f;
		int flags = 0;

		if( argc >= 13 )
			flags = Q_atoi( args[12] );
		if( argc >= 14 )
			aspect = Q_atof( args[13] );

		color = rgba_t(Q_atoi(args[8]), Q_atoi(args[9]), Q_atoi(args[10]), Q_atoi(args[11])); 
		gTouch.AddButton( args[1], args[2], args[3],
			Q_atof( args[4] ), Q_atof( args[5] ), 
			Q_atof( args[6] ), Q_atof( args[7] ) ,
			color, round_aspect, aspect, flags );

		return;
	}

	if( argc >= 8 )
	{
		color = rgba_t(255,255,255);

		gTouch.AddButton( args[1], args[2], args[3],
			Q_atof( args[4] ), Q_atof( args[5] ), 
			Q_atof( args[6] ), Q_atof( args[7] ),
			color );
		return;
	}
	if( argc >= 4 )
	{
		color = rgba_t(255,255,255);
		gTouch.AddButton( args[1], args[2], args[3], 0.4, 0.4, 0.6, 0.6 );
		return;
	}

	Msg( "Usage: touch_addbutton <name> <texture> <command> [<x1> <y1> <x2> <y2> [ r g b a ] ]\n" );
}

CON_COMMAND( touch_removebutton, "remove native touch button" )
{
	if( args.ArgC() > 1 )
		gTouch.RemoveButton( args[1] );
	else
		Msg( "Usage: touch_removebutton <name>\n" );
}

#if 0
CON_COMMAND( touch_settexture, "set button texture" )
{
	if( args.ArgC() >= 3 )
	{
		gTouch.SetTexture( args[1], args[2] );
		return;
	}
	Msg( "Usage: touch_settexture <name> <file>\n" );
}
#endif

CON_COMMAND( touch_enableedit, "enable button editing mode" )
{
	gTouch.EnableTouchEdit(true);
}

CON_COMMAND( touch_disableedit, "disable button editing mode" )
{
	gTouch.EnableTouchEdit(false);
}

CON_COMMAND( touch_setcolor, "change button color" )
{
	if( args.ArgC() >= 6 )
	{
		rgba_t color( Q_atoi( args[2] ), Q_atoi( args[3] ), Q_atoi( args[4] ), Q_atoi( args[5] ) );
		gTouch.SetColor( args[1], color );
	}
	else
		Msg( "Usage: touch_setcolor <name> <r> <g> <b> <a>\n" );
}

CON_COMMAND( touch_setcommand, "change button command" )
{
	if( args.ArgC() >= 3 )
		gTouch.SetCommand( args[1], args[2] );
	else
		Msg( "Usage: touch_setcommand <name> <command>\n" );
}

CON_COMMAND( touch_setflags, "change button flags" )
{
	if( args.ArgC() >= 3 )
		gTouch.SetFlags( args[1], Q_atoi(args[2]) );
	else
		Msg( "Usage: touch_setflags <name> <flags>\n" );
}

CON_COMMAND( touch_show, "show button" )
{
	if( args.ArgC() >= 2 )
		gTouch.ShowButton( args[1] );
	else
		Msg( "Usage: touch_show <name>\n" );
}

CON_COMMAND( touch_hide, "hide button" )
{
	if( args.ArgC() >= 2 )
		gTouch.HideButton( args[1] );
	else
		Msg( "Usage: touch_hide <name>\n" );
}

CON_COMMAND( touch_list, "list buttons" )
{
	gTouch.ListButtons();
}

CON_COMMAND( touch_removeall, "remove all buttons" )
{
	gTouch.RemoveButtons();
}

CON_COMMAND( touch_writeconfig, "save current config" )
{
	gTouch.WriteConfig();
}


CON_COMMAND( touch_loaddefaults, "generate config from defaults" )
{
	gTouch.ResetToDefaults();
}

CON_COMMAND( touch_setgridcolor, "change grid color" )
{
	if( args.ArgC() >= 5 )
		gTouch.gridcolor = rgba_t( Q_atoi( args[1] ), Q_atoi( args[2] ), Q_atoi( args[3] ), Q_atoi( args[4] ) );
	else
		Msg( "Usage: touch_setgridcolor <r> <g> <b> <a>\n" );
}

/*
CON_COMMAND( touch_roundall, "round all buttons coordinates to grid" )
{
	
}

CON_COMMAND( touch_exportconfig, "export config keeping aspect ratio" )
{
	
}

CON_COMMAND( touch_reloadconfig, "load config, not saving changes" )
{
	
}
*/

/*
CON_COMMAND( touch_fade, "start fade animation for selected buttons" )
{
	
}

CON_COMMAND( touch_toggleselection, "toggle visibility on selected button in editor" )
{

}*/

void CTouchControls::GetTouchAccumulators( float *side, float *forward, float *yaw, float *pitch )
{
	*forward = this->forward;
	*side = this->side;
	*pitch = this->pitch;
	*yaw = this->yaw;
	this->yaw = 0.f;
	this->pitch = 0.f;
}

void CTouchControls::GetTouchDelta( float yaw, float pitch, float *dx, float *dy )
{
	// Apply filtering?
	if( touch_filter.GetBool() )
	{
		// Average over last two samples
		*dx = ( yaw + m_flPreviousYaw ) * 0.5f;
		*dy = ( pitch + m_flPreviousPitch ) * 0.5f;
	}
	else
	{
		*dx = yaw;
		*dy = pitch;
	}

	// Latch previous
	m_flPreviousYaw = yaw;
	m_flPreviousPitch = pitch;
}

// CS:GO layout: move stick on the left half, look pad on the right half,
// action buttons along the right edge and weapon switching on the left.
// Buttons for the (Scaleform) menus: buy menu, pause menu, scoreboard, team
// menu, chat. Along the top edge, clear of the movement and look areas' buttons.
// Returns how many were added.
static int AddMenuButtons( rgba_t color, bool bOnlyMissing )
{
	struct MenuButton_t { const char *name, *texture, *command; float x1, y1, x2, y2; };
	static const MenuButton_t s_MenuButtons[] =
	{
		{ "scores",   "vgui/touch/changeclass",  "+showscores",     0.090000, 0.000000, 0.170000, 0.142222 },
		{ "teammenu", "vgui/touch/changeteam",   "teammenu",        0.180000, 0.000000, 0.260000, 0.142222 },
		{ "chat",     "vgui/touch/chat",         "messagemode",     0.270000, 0.000000, 0.350000, 0.142222 },
		{ "buymenu",  "vgui/touch/show_weapons", "ios_buymenu_toggle", 0.790000, 0.000000, 0.870000, 0.142222 },
		{ "pause",    "vgui/touch/menu",         "gameui_activate", 0.900000, 0.000000, 0.980000, 0.142222 },
	};

	int nAdded = 0;
	for ( int i = 0; i < ARRAYSIZE( s_MenuButtons ); i++ )
	{
		const MenuButton_t &b = s_MenuButtons[i];
		if ( bOnlyMissing && gTouch.FindButton( b.name ) )
			continue;
		gTouch.AddButton( b.name, b.texture, b.command, b.x1, b.y1, b.x2, b.y2, color );
		++nAdded;
	}
	return nAdded;
}

// Temporary diagnostic buttons for the white tint on agent models, bottom middle:
// left = specular (phong) off/on, right = rim light off/on. Both apply instantly.
// Earlier builds added six more (chardiag1-6); those are removed from saved layouts.
// Returns how many buttons were added or removed.
static int AddCharDiagButtons( rgba_t color, bool bOnlyMissing )
{
	int nChanged = 0;
	for ( int i = 1; i <= 6; i++ )
	{
		char szName[32];
		Q_snprintf( szName, sizeof( szName ), "chardiag%d", i );
		if ( gTouch.FindButton( szName ) )
		{
			gTouch.RemoveButton( szName );
			++nChanged;
		}
	}

	static const char *s_Toggles[][2] = { { "chardiag7", "toggle ios_char_phong 1 0" }, { "chardiag8", "toggle ios_char_rim 1 0" } };
	for ( int i = 0; i < ARRAYSIZE( s_Toggles ); i++ )
	{
		if ( bOnlyMissing && gTouch.FindButton( s_Toggles[i][0] ) )
			continue;
		float x1 = 0.43f + i * 0.08f;
		gTouch.AddButton( s_Toggles[i][0], "vgui/touch/settings", s_Toggles[i][1], x1, 0.86f, x1 + 0.06f, 0.99f, color );
		++nChanged;
	}
	return nChanged;
}

static void AddDefaultButtons( rgba_t color )
{
	gTouch.AddButton( "look", "", "_look", 0.5, 0, 1, 1, color, 0, 0, 0 );
	gTouch.AddButton( "move", "", "_move", 0, 0, 0.5, 1, color, 0, 0, 0 );

	gTouch.AddButton( "use", "vgui/touch/use", "+use", 0.880000, 0.213333, 1.000000, 0.426667, color );
	gTouch.AddButton( "jump", "vgui/touch/jump", "+jump", 0.880000, 0.462222, 1.000000, 0.675556, color );
	gTouch.AddButton( "attack", "vgui/touch/shoot", "+attack", 0.760000, 0.583333, 0.880000, 0.796667, color );
	gTouch.AddButton( "attack2", "vgui/touch/shoot_alt", "+attack2", 0.760000, 0.320000, 0.880000, 0.533333, color );
	gTouch.AddButton( "duck", "vgui/touch/crouch", "+duck", 0.880000, 0.746667, 1.000000, 0.960000, color );
	gTouch.AddButton( "speed", "vgui/touch/speed", "+speed", 0.180000, 0.568889, 0.280000, 0.746667, color );
	gTouch.AddButton( "reload", "vgui/touch/reload", "+reload", 0.000000, 0.320000, 0.120000, 0.533333, color );
	gTouch.AddButton( "invnext", "vgui/touch/next_weap", "ios_weapnext", 0.000000, 0.533333, 0.120000, 0.746667, color );
	gTouch.AddButton( "invprev", "vgui/touch/prev_weap", "ios_weapprev", 0.000000, 0.071111, 0.120000, 0.284444, color );
	gTouch.AddButton( "drop", "vgui/touch/back", "drop", 0.680000, 0.000000, 0.760000, 0.142222, color );
	gTouch.AddButton( "console", "vgui/touch/showconsole", "toggleconsole", 0.000000, 0.000000, 0.080000, 0.142222, color );
	gTouch.AddButton( "edit", "vgui/touch/settings", "touch_enableedit", 0.420000, 0.000000, 0.500000, 0.151486, color );
	AddMenuButtons( color, false );
	AddCharDiagButtons( color, false );
}

void CTouchControls::ResetToDefaults()
{
	rgba_t color(255, 255, 255, 155);
	char buf[MAX_PATH];
	gridcolor = rgba_t(255, 0, 0, 30);

	RemoveButtons();

	Q_snprintf(buf, sizeof buf, "cfg/%s", TOUCH_DEFAULT_CFG);
	if( !filesystem->FileExists(buf) )
	{
		AddDefaultButtons( color );
	}
	else
	{
		Q_snprintf(buf, sizeof buf, "exec %s", TOUCH_DEFAULT_CFG);
		engine->ExecuteClientCmd(buf);
	}

	WriteConfig();
}

void CTouchControls::Init()
{
	int w,h;
	engine->GetScreenSize( w, h );
	screen_w = w; screen_h = h;

	touchTextureID = 0;
	configchanged = false;
	config_loaded = false;
	btns.EnsureCapacity( 64 );
	look_finger = move_finger = resize_finger = -1;
	forward = side = 0.f;
	pitch = yaw = 0.f;
	scolor = rgba_t( -1, -1, -1, -1 );
	state = state_none;
	swidth = 1;
	move_button = edit = selection = NULL;
	showbuttons = true;
	clientonly = false;
	precision = false;
	mouse_events = 0;
	move_start_x = move_start_y = 0.0f;
	m_flPreviousYaw = m_flPreviousPitch = 0.f;
	gridcolor = rgba_t(255, 0, 0, 30);

	m_bCutScene = false;
	showtexture = hidetexture = resettexture = closetexture = joytexture = 0;
	configchanged = false;

	rgba_t color(255, 255, 255, 155);

	AddDefaultButtons( color );

	char buf[256];

	Q_snprintf(buf, sizeof buf, "cfg/%s", touch_config_file.GetString());
	if( filesystem->FileExists(buf, "MOD") )
	{
		Q_snprintf(buf, sizeof buf, "exec %s\n", touch_config_file.GetString());
		engine->ExecuteClientCmd(buf);
	}
	else
		ResetToDefaults();

	// Layouts saved before the menus worked have no menu buttons; add them
	if ( AddMenuButtons( color, true ) + AddCharDiagButtons( color, true ) > 0 )
		WriteConfig();

	// Configs saved by earlier builds use invnext/invprev, which go through
	// CS:GO's Scaleform weapon selection HUD and do nothing here.
	for( int i = 0; i < btns.Count(); i++ )
	{
		if( !Q_strcmp( btns[i]->command, "buymenu" ) )
			Q_strncpy( btns[i]->command, "ios_buymenu_toggle", sizeof( btns[i]->command ) );
		else if( !Q_strcmp( btns[i]->command, "invnext" ) )
			Q_strncpy( btns[i]->command, "ios_weapnext", sizeof( btns[i]->command ) );
		else if( !Q_strcmp( btns[i]->command, "invprev" ) )
			Q_strncpy( btns[i]->command, "ios_weapprev", sizeof( btns[i]->command ) );
		// the diagnostic buttons 7 and 8 were repurposed
		else if( !Q_strcmp( btns[i]->name, "chardiag7" ) )
			Q_strncpy( btns[i]->command, "toggle ios_char_phong 1 0", sizeof( btns[i]->command ) );
		else if( !Q_strcmp( btns[i]->name, "chardiag8" ) )
			Q_strncpy( btns[i]->command, "toggle ios_char_rim 1 0", sizeof( btns[i]->command ) );
	}

	CTouchTexture *texture = new CTouchTexture;
	texture->isInAtlas = false;
	texture->textureID = 0;
	texture->X0 = 0; texture->X1 = 0; texture->Y0 = 0; texture->Y1 = 0;

	Q_strncpy( texture->szName, "vgui/touch/back", sizeof(texture->szName) );
	textureList.AddToTail(texture);

	CreateAtlasTexture();
	m_flHideTouch = 0.f;

	initialized = true;
}

void CTouchControls::LevelInit()
{
	m_bCutScene = false;
	m_AlphaDiff = 0;
	m_flHideTouch = 0;
}

int nextPowerOfTwo(int x)
{
	if( (x & (x - 1)) == 0)
		return x;

	int t = 1 << 30;
	while (x < t) t >>= 1;

	return t << 1;
}

void CTouchControls::CreateAtlasTexture()
{
	char fullFileName[MAX_PATH];

	int atlasSize = 0;

	stbrp_rect *rects = (stbrp_rect*)malloc(textureList.Count()*sizeof(stbrp_rect));
	memset(rects, 0, sizeof(stbrp_node)*textureList.Count());

	if( touchTextureID )
		vgui::surface()->DeleteTextureByID( touchTextureID );

	int rectCount = 0;

	for( int i = 0; i < textureList.Count(); i++ )
	{
		CTouchTexture *t = textureList[i];
		Q_snprintf(fullFileName, MAX_PATH, "materials/%s.vtf", t->szName);

		FileHandle_t fp;
		fp = ::filesystem->Open( fullFileName, "rb" );
		if( !fp )
		{
			t->textureID = vgui::surface()->CreateNewTextureID();
			vgui::surface()->DrawSetTextureFile( t->textureID, t->szName, true, false );
			continue;
		}

		::filesystem->Seek( fp, 0, FILESYSTEM_SEEK_TAIL );
		int srcVTFLength = ::filesystem->Tell( fp );
		::filesystem->Seek( fp, 0, FILESYSTEM_SEEK_HEAD );

		CUtlBuffer buf;
		buf.EnsureCapacity( srcVTFLength );
		int bytesRead = ::filesystem->Read( buf.Base(), srcVTFLength, fp );
		::filesystem->Close( fp );

		buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 ); // Need to set these explicitly since ->Read goes straight to memory and skips them.
		buf.SeekPut( CUtlBuffer::SEEK_HEAD, bytesRead );

		t->vtf = CreateVTFTexture();
		if (t->vtf->Unserialize(buf))
		{
			if( t->vtf->Format() != IMAGE_FORMAT_RGBA8888 && t->vtf->Format() != IMAGE_FORMAT_BGRA8888 )
			{
				t->textureID = vgui::surface()->CreateNewTextureID();
				vgui::surface()->DrawSetTextureFile( t->textureID, t->szName, true, false);
				DestroyVTFTexture(t->vtf);
				continue;
			}
			if( t->vtf->Height() != t->vtf->Width() || (t->vtf->Height() & (t->vtf->Height() - 1)) != 0 )
				Error("%s texture is wrong! Don't use npot textures for touch.");

			t->height = t->vtf->Height();
			t->width = t->vtf->Width();
			t->isInAtlas = true;

			atlasSize += t->width*t->height;
		}
		else
		{
			DestroyVTFTexture(t->vtf);
			t->textureID = vgui::surface()->CreateNewTextureID();
			vgui::surface()->DrawSetTextureFile( t->textureID, t->szName, true, false);
			continue;
		}

		rects[rectCount].h = t->height;
		rects[rectCount].w = t->width;
		rectCount++;
	}

	if( !textureList.Count() || rectCount == 0 )
	{
		free(rects);
		return;
	}

	int atlasHeight = nextPowerOfTwo(sqrt((double)atlasSize));
	int sizeInBytes = atlasHeight*atlasHeight*4;
	unsigned char *dest = new unsigned char[sizeInBytes];
	memset(dest, 0, sizeInBytes);

	int nodesCount = atlasHeight * 2;
	stbrp_node *nodes = (stbrp_node*)malloc(nodesCount*sizeof(stbrp_node));
	memset(nodes, 0, sizeof(stbrp_node)*nodesCount);

	stbrp_context context;
	stbrp_init_target( &context, atlasHeight, atlasHeight, nodes, nodesCount );
	stbrp_pack_rects(&context, rects, rectCount);

	rectCount = 0;
	for( int i = 0; i < textureList.Count(); i++ )
	{
		CTouchTexture *t = textureList[i];
		if( t->textureID )
			continue;

		t->X0 = rects[rectCount].x / (float)atlasHeight;
		t->Y0 = rects[rectCount].y / (float)atlasHeight;
		t->X1 = t->X0 + t->width / (float)atlasHeight;
		t->Y1 = t->Y0 + t->height / (float)atlasHeight;

		unsigned char *src = t->vtf->ImageData(0, 0, 0);
		for( int row = 0; row < t->height; row++)
		{
			unsigned char *row_dest = dest+(row+rects[rectCount].y)*atlasHeight*4+rects[rectCount].x*4;
			unsigned char *row_src = src+row*t->height*4;

			memcpy(row_dest, row_src, t->height*4);
		}
		rectCount++;

		DestroyVTFTexture(t->vtf);
	}

	touchTextureID = vgui::surface()->CreateNewTextureID( true );
	vgui::surface()->DrawSetTextureRGBA( touchTextureID, dest, atlasHeight, atlasHeight );

	free(nodes);
	free(rects);
	delete[] dest;
}

void CTouchControls::Shutdown( )
{
	textureList.PurgeAndDeleteElements();
	btns.PurgeAndDeleteElements();
}

void CTouchControls::RemoveButtons()
{
	btns.PurgeAndDeleteElements();	
}

void CTouchControls::ListButtons()
{
	CUtlVector<CTouchButton*>::iterator it;
	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *b = *it;
	
		Msg( "%s %s %s %f %f %f %f %d %d %d %d %d\n",
			b->name, b->texturefile, b->command,
			b->x1, b->y1, b->x2, b->y2,
			b->color.r, b->color.g, b->color.b, b->color.a, b->flags );
	}
}

void CTouchControls::IN_CheckCoords( float *x1, float *y1, float *x2, float *y2  )
{
	/// TODO: grid check here
	if( *x2 - *x1 < GRID_X * 2 )
		*x2 = *x1 + GRID_X * 2;
	if( *y2 - *y1 < GRID_Y * 2)
		*y2 = *y1 + GRID_Y * 2;
	if( *x1 < 0 )
		*x2 -= *x1, *x1 = 0;
	if( *y1 < 0 )
		*y2 -= *y1, *y1 = 0;
	if( *y2 > 1 )
		*y1 -= *y2 - 1, *y2 = 1;
	if( *x2 > 1 )
		*x1 -= *x2 - 1, *x2 = 1;

	if ( touch_grid_enable.GetBool() )
	{
		*x1 = GRID_ROUND_X( *x1 );
		*x2 = GRID_ROUND_X( *x2 );
		*y1 = GRID_ROUND_Y( *y1 );
		*y2 = GRID_ROUND_Y( *y2 );
	}
}

void CTouchControls::Move( float /*frametime*/, CUserCmd *cmd )
{
}

void CTouchControls::IN_Look()
{
}

void CTouchControls::Frame()
{
	if (!initialized)
		return;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if( pPlayer && (pPlayer->GetFlags() & FL_FROZEN || g_pIntroData != NULL) )
	{
		if( !m_bCutScene )
		{
			m_bCutScene = true;
			m_AlphaDiff = 0;
		}
	}
	else if( !pPlayer )
	{
		m_bCutScene = false;
		m_AlphaDiff = 0;
		m_flHideTouch = 0;
	}
	else
		m_bCutScene = false;

#if defined( IOS )
	// CS:GO's GameUI is Scaleform (stubbed on iOS) and can report itself as
	// visible while nothing is shown; draw the controls regardless.
	if( touch_enable.GetBool() && touch_draw.GetBool() ) Paint();
#else
	if( touch_enable.GetBool() && touch_draw.GetBool() && !enginevgui->IsGameUIVisible() ) Paint();
#endif
}

#if defined( IOS )
static ConVar touch_show_in_menus( "touch_show_in_menus", "0", FCVAR_ARCHIVE, "Show all touch buttons in menus too (otherwise only the console button)" );

// While a menu has the input only the console button is shown and pressable;
// the rest of the controls are for playing.
static bool TouchButtonAvailable( const CTouchButton *btn )
{
	extern bool IOS_IsMenuActive();
	if ( touch_show_in_menus.GetBool() || !IOS_IsMenuActive() )
		return true;
	if ( !Q_strcmp( btn->command, "toggleconsole" ) )
		return true;
	// the buy button stays while the buy menu is open, to close it again
	extern bool IOS_IsBuyMenuVisible();
	return !Q_strcmp( btn->command, "ios_buymenu_toggle" ) && IOS_IsBuyMenuVisible();
}

// Opens the buy menu, or closes it when it is actually open (the panel's own
// state, not a press count, so a missed or doubled press can't desync it).
// Diagnostic for the white tint on agent models: toggle one "character" shader
// feature (ios_char_<name>) and reload the agent materials so it takes effect.
CON_COMMAND( ios_char_toggle, "Toggle a character shader feature: envmap fakerim ambientreflection masks1 masks2 phongwarp" )
{
	if ( args.ArgC() < 2 )
		return;
	char szVar[64];
	Q_snprintf( szVar, sizeof( szVar ), "ios_char_%s", args[1] );
	ConVarRef var( szVar );
	if ( !var.IsValid() )
	{
		printf( "[chardiag] unknown %s\n", szVar );
		return;
	}
	var.SetValue( var.GetBool() ? 0 : 1 );
	g_pMaterialSystem->ReloadMaterials( "models/player/custom_player" );
	printf( "[chardiag] %s = %d (agent materials reloaded)\n", szVar, var.GetInt() );
	fflush( stdout );
}

CON_COMMAND( ios_buymenu_toggle, "Open the buy menu, or close it if it is open" )
{
	extern bool IOS_IsBuyMenuVisible();
	bool bOpen = IOS_IsBuyMenuVisible();
	engine->ClientCmd_Unrestricted( bOpen ? "buymenu 0" : "buymenu" );
	if ( touch_debug.GetBool() )
		printf( "[touch] buy menu toggle: %s\n", bOpen ? "close" : "open" );
}
#else
static bool TouchButtonAvailable( const CTouchButton *btn ) { return true; }
#endif

void CTouchControls::Paint()
{
	if (!initialized)
		return;

	CUtlVector<CTouchButton*>::iterator it;

	const rgba_t buttonEditClr = rgba_t( 61, 153, 0, 40 );

	if( state == state_edit )
	{
		vgui::surface()->DrawSetColor(gridcolor.r, gridcolor.g, gridcolor.b, gridcolor.a*3); // 255, 0, 0, 200 <- default here
		float x,y;

		for( x = 0.0f; x < 1.0f; x += GRID_X )
			vgui::surface()->DrawLine( screen_w*x, 0, screen_w*x, screen_h );

		for( y = 0.0f; y < 1.0f; y += GRID_Y )
			vgui::surface()->DrawLine( 0, screen_h*y, screen_w, screen_h*y );

		for( it = btns.begin(); it != btns.end(); it++ )
		{
			CTouchButton *btn = *it;

			if( !(btn->flags & TOUCH_FL_NOEDIT) )
			{
				if( touch_button_info.GetInt() )
				{
					g_pMatSystemSurface->DrawColoredText( 2, btn->x1*screen_w, btn->y1*screen_h, 255, 255, 255, 255, "N: %s", btn->name );			// name
					g_pMatSystemSurface->DrawColoredText( 2, btn->x1*screen_w, btn->y1*screen_h+10, 255, 255, 255, 255, "T: %s", btn->texturefile );	// texture
					g_pMatSystemSurface->DrawColoredText( 2, btn->x1*screen_w, btn->y1*screen_h+20, 255, 255, 255, 255, "C: %s", btn->command );		// command
					g_pMatSystemSurface->DrawColoredText( 2, btn->x1*screen_w, btn->y1*screen_h+30, 255, 255, 255, 255, "F: %i", btn->flags );		// flags
					g_pMatSystemSurface->DrawColoredText( 2, btn->x1*screen_w, btn->y1*screen_h+40, 255, 255, 255, 255, "RGBA: %d %d %d %d", btn->color.r, btn->color.g, btn->color.b, btn->color.a );// color
				}

				vgui::surface()->DrawSetColor(buttonEditClr.r, buttonEditClr.g, buttonEditClr.b, buttonEditClr.a); // 255, 0, 0, 50 <- default here
				vgui::surface()->DrawFilledRect( btn->x1*screen_w, btn->y1*screen_h, btn->x2*screen_w, btn->y2*screen_h );
			}
		}
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int meshCount = 0;

	// Draw non-atlas touch textures
	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *btn = *it;

		if( btn->texture != NULL && !(btn->flags & TOUCH_FL_HIDE) && ( state == state_edit || TouchButtonAvailable( btn ) ) )
		{
			CTouchTexture *t = btn->texture;

			if( t->textureID )
			{
				m_pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, g_pMatSystemSurface->DrawGetTextureMaterial(t->textureID) );

				meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, 1 );

				int alpha = (btn->color.a > MIN_ALPHA_IN_CUTSCENE) ? max(MIN_ALPHA_IN_CUTSCENE, btn->color.a-m_AlphaDiff) : btn->color.a;
				rgba_t color(btn->color.r, btn->color.g, btn->color.b, alpha);

				meshBuilder.Position3f( btn->x1*screen_w, btn->y1*screen_h, 0 );
				meshBuilder.Color4ubv( color );
				meshBuilder.TexCoord2f( 0, 0, 0 );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.Position3f( btn->x2*screen_w, btn->y1*screen_h, 0 );
				meshBuilder.Color4ubv( color );
				meshBuilder.TexCoord2f( 0, 1, 0 );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.Position3f( btn->x2*screen_w, btn->y2*screen_h, 0 );
				meshBuilder.Color4ubv( color );
				meshBuilder.TexCoord2f( 0, 1, 1 );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.Position3f( btn->x1*screen_w, btn->y2*screen_h, 0 );
				meshBuilder.Color4ubv( color );
				meshBuilder.TexCoord2f( 0, 0, 1 );
				meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

				meshBuilder.End();

				m_pMesh->Draw();
			}
			else if( !btn->texture->isInAtlas )
				CreateAtlasTexture();

			if( !t->textureID )
				meshCount++;
		}
	}

	m_pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, g_pMatSystemSurface->DrawGetTextureMaterial(touchTextureID) );
	meshBuilder.Begin( m_pMesh, MATERIAL_QUADS, meshCount );

	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *btn = *it;

		if( btn->texture != NULL && !(btn->flags & TOUCH_FL_HIDE) && !btn->texture->textureID && ( state == state_edit || TouchButtonAvailable( btn ) ) )
		{
			CTouchTexture *t = btn->texture;

			int alpha = (btn->color.a > MIN_ALPHA_IN_CUTSCENE) ? max(MIN_ALPHA_IN_CUTSCENE, btn->color.a-m_AlphaDiff) : btn->color.a;
			rgba_t color(btn->color.r, btn->color.g, btn->color.b, alpha);

			meshBuilder.Position3f( btn->x1*screen_w, btn->y1*screen_h, 0 );
			meshBuilder.Color4ubv( color );
			meshBuilder.TexCoord2f( 0, t->X0, t->Y0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( btn->x2*screen_w, btn->y1*screen_h, 0 );
			meshBuilder.Color4ubv( color );
			meshBuilder.TexCoord2f( 0, t->X1, t->Y0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( btn->x2*screen_w, btn->y2*screen_h, 0 );
			meshBuilder.Color4ubv( color );
			meshBuilder.TexCoord2f( 0, t->X1, t->Y1 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( btn->x1*screen_w, btn->y2*screen_h, 0 );
			meshBuilder.Color4ubv( color );
			meshBuilder.TexCoord2f( 0, t->X0, t->Y1 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();
		}
	}

	meshBuilder.End();
	m_pMesh->Draw();


	if( m_flHideTouch < gpGlobals->curtime )
	{
		if( m_bCutScene && m_AlphaDiff < 255-MIN_ALPHA_IN_CUTSCENE )
			m_AlphaDiff++;
		else if( !m_bCutScene && m_AlphaDiff > 0 )
			m_AlphaDiff--;

		m_flHideTouch = gpGlobals->curtime + 0.002f;
	}
}

void CTouchControls::AddButton( const char *name, const char *texturefile, const char *command, float x1, float y1, float x2, float y2, rgba_t color, int round, float aspect, int flags )
{
	CTouchButton *btn = new CTouchButton;
	ETouchButtonType type = touch_command;

	Q_strncpy( btn->name, name, sizeof(btn->name) );
	Q_strncpy( btn->texturefile, texturefile, sizeof(btn->texturefile) );
	Q_strncpy( btn->command, command, sizeof(btn->command) );

	if( round )
		IN_CheckCoords(&x1, &y1, &x2, &y2);

	if( round == round_aspect )
		y2 = y1 + ( x2 - x1 ) * (((float)screen_w)/screen_h) * aspect;

	btn->x1 = x1;
	btn->y1 = y1;
	btn->x2 = x2;
	btn->y2 = y2;
	btn->flags = flags;

	//IN_CheckCoords(&btn->x1, &btn->y1, &btn->x2, &btn->y2);

	if( Q_strcmp(command, "_look") == 0 )
		type = touch_look;
	else if( Q_strcmp(command, "_move") == 0 )
		type = touch_move;

	btn->color = color;
	btn->type = type;
	btn->finger = -1;

	if( btn->texturefile[0] == 0 )
	{
		btn->texture = NULL;
		btns.AddToTail(btn);
		return;
	}

	for( int i = 0; i < textureList.Count(); i++ )
	{
		if( strcmp( textureList[i]->szName, btn->texturefile) == 0 )
		{
			btn->texture = textureList[i];
			btns.AddToTail(btn);
			return;
		}
	}

	CTouchTexture *texture = new CTouchTexture;
	btn->texture = texture;
	texture->isInAtlas = false;
	texture->textureID = 0;
	texture->X0 = 0; texture->X1 = 0; texture->Y0 = 0; texture->Y1 = 0;
	Q_strncpy( texture->szName, btn->texturefile, sizeof(btn->texturefile) );
	textureList.AddToTail(texture);

	btns.AddToTail(btn);
}

void CTouchControls::ShowButton( const char *name )
{
	CTouchButton *btn =	FindButton( name );
	if( btn )
		btn->flags &= ~TOUCH_FL_HIDE;
}

void CTouchControls::HideButton(const char *name)
{
	CTouchButton *btn =	FindButton( name );
	if( btn )
		btn->flags |= TOUCH_FL_HIDE;
}

void CTouchControls::SetTexture(const char *name, const char *file)
{
	CTouchButton *btn = FindButton(name);

	if( btn )
	{
		Q_strncpy( btn->texturefile, file, sizeof(btn->texturefile) );

//		btn->textureID = vgui::surface()->CreateNewTextureID();
//		vgui::surface()->DrawSetTextureFile( btn->textureID, file, true, false);
	}
}

void CTouchControls::SetColor(const char *name, rgba_t color)
{
	CTouchButton *btn = FindButton(name);
	if( btn ) btn->color = color;
}

void CTouchControls::SetCommand(const char *name, const char *cmd)
{
	CTouchButton *btn = FindButton(name);
	if( btn ) Q_strncpy(btn->command, cmd, sizeof btn->command);
}

void CTouchControls::SetFlags(const char *name, int flags)
{
	CTouchButton *btn = FindButton(name);
	if( btn ) btn->flags = flags;
}

void CTouchControls::RemoveButton( const char *name )
{
	for( int i = 0; i < btns.Count(); i++ )
	{
		if( Q_strncmp( btns[i]->name, name, sizeof(btns[i]->name)) == 0 )
		{
			delete btns[i];
			btns.Remove(i);
			i--;
		}
	}
}

CTouchButton *CTouchControls::FindButton( const char *name )
{
	CUtlVector<CTouchButton*>::iterator it;
	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *button = *it;

		if( Q_strncmp( button->name, name, sizeof(button->name)) == 0 )
			return button;
	}
	return NULL;
}

void CTouchControls::ProcessEvent(touch_event_t *ev)
{
	if( !touch_enable.GetBool() )
		return;

	if( state == state_edit )
	{
		EditEvent(ev);
		return;
	}
	
	if( ev->type == IE_FingerMotion )
		FingerMotion( ev );
	else
		FingerPress( ev );
}

void CTouchControls::EditEvent(touch_event_t *ev)
{
	const float x = ev->x;
	const float y = ev->y;

	//CUtlVector<CTouchButton*>::iterator it;
	
	if( ev->type == IE_FingerDown )
	{		
		//for( it = btns.end(); it != btns.begin(); it-- ) unexpected, doesn't work
		for( int i = btns.Count()-1; i >= 0; i-- )
		{
			CTouchButton *btn = btns[i];
			if( x > btn->x1 && x < btn->x2 && y > btn->y1 && y < btn->y2 )
			{
				if( btn->flags & TOUCH_FL_HIDE )
					continue;
				
				if( btn->flags & TOUCH_FL_NOEDIT )
				{
					engine->ClientCmd_Unrestricted( btn->command );
					continue;
				}

				if( move_finger == -1 )
				{
					move_finger = ev->fingerid;
					selection = btn;
					break;
				}
				else if( resize_finger == -1 )
				{
					resize_finger = ev->fingerid;
				}
			}
		}
	}
	else if( ev->type == IE_FingerUp )
	{
		if( ev->fingerid == move_finger )
		{
			move_finger = -1;
			IN_CheckCoords( &selection->x1, &selection->y1, &selection->x2, &selection->y2 );
			selection = nullptr;
		}
		else if( ev->fingerid == resize_finger )
			resize_finger = -1;
	}
	else // IE_FingerMotion
	{
		if( !selection )
			return;

		if( move_finger == ev->fingerid )
		{
			selection->x1 += ev->dx;
			selection->x2 += ev->dx;
			selection->y1 += ev->dy;
			selection->y2 += ev->dy;
		}
		else if( resize_finger == ev->fingerid )
		{
			selection->x2 += ev->dx;
			selection->y2 += ev->dy;
		}
	}
}


void CTouchControls::FingerMotion(touch_event_t *ev) // finger in my ass
{
	const float x = ev->x;
	const float y = ev->y;

	float f, s;

	CUtlVector<CTouchButton*>::iterator it;
	for( it = btns.begin(); it != btns.end(); it++ )
	{
		CTouchButton *btn = *it;
		if( btn->finger == ev->fingerid )
		{
			if( btn->type == touch_move )
			{
				f = ( move_start_y - y ) / touch_forwardzone.GetFloat();
				s = ( move_start_x - x ) / touch_sidezone.GetFloat();
				forward = bound( -1, f, 1 );
				side = bound( -1, s, 1 );
			}
			else if( btn->type == touch_look )
			{
				yaw += ev->dx;
				pitch += ev->dy;
			}
		}
	}
}

void CTouchControls::FingerPress(touch_event_t *ev)
{
	const float x = ev->x;
	const float y = ev->y;

	CUtlVector<CTouchButton*>::iterator it;

	if( ev->type == IE_FingerDown )
	{
		for( it = btns.begin(); it != btns.end(); it++ )
		{
			CTouchButton *btn = *it;
			if(  x > btn->x1 && x < btn->x2 && y > btn->y1 && y < btn->y2 )
			{
				if( btn->flags & TOUCH_FL_HIDE )
					continue;
				if( !TouchButtonAvailable( btn ) )
					continue;

				btn->finger = ev->fingerid;
				if( btn->type == touch_move  )
				{
					if( move_finger == -1 )
					{
						move_start_x = x;
						move_start_y = y;
						move_finger = ev->fingerid;
					}
					else
						btn->finger = move_finger;
				}
				else if( btn->type == touch_look )
				{
					if( look_finger == -1 )
						look_finger = ev->fingerid;
					else
						btn->finger = look_finger;
				}
				else
				{
					engine->ClientCmd_Unrestricted( btn->command );
					if( touch_debug.GetBool() )
						printf( "[touch] button '%s' -> %s\n", btn->name, btn->command );
				}
			}
		}
		if( touch_debug.GetBool() )
			printf( "[touch] finger %d down at %.3f %.3f (screen %.0fx%.0f)\n", ev->fingerid, x, y, screen_w, screen_h );
	}
	else if( ev->type == IE_FingerUp )
	{
		for( it = btns.begin(); it != btns.end(); it++ )
		{
			CTouchButton *btn = *it;

			if( btn->flags & TOUCH_FL_HIDE )
				continue;

			if( btn->finger == ev->fingerid )
			{
				btn->finger = -1;

				if( btn->type == touch_move )
				{
					forward = side = 0;
					move_finger = -1;
				}
				else if( btn->type == touch_look )
					look_finger = -1;
				else if( btn->command[0] == '+' )
				{
					char cmd[256];

					snprintf( cmd, sizeof cmd, "%s", btn->command );
					cmd[0] = '-';
					engine->ClientCmd_Unrestricted( cmd );
				}
			}
		}
	}
}

void CTouchControls::EnableTouchEdit(bool enable)
{
	if( enable )
	{
		state = state_edit;
		resize_finger = move_finger = look_finger = wheel_finger = -1;
		move_button = NULL;
		configchanged = true;
		AddButton( "close_edit", "vgui/touch/back", "touch_disableedit", 0.020000, 0.800000, 0.100000, 0.977778, rgba_t(255,255,255,255), 0, 1.f, TOUCH_FL_NOEDIT );
	}
	else
	{
		state = state_none;
		resize_finger = move_finger = look_finger = wheel_finger = -1;
		move_button = NULL;
		configchanged = false;
		RemoveButton("close_edit");
		WriteConfig();
	}
}

void CTouchControls::WriteConfig()
{
	FileHandle_t f;
	char newconfigfile[128];
	char oldconfigfile[128];
	char configfile[128];

	if( btns.IsEmpty() )
		return;

	if( CommandLine()->FindParm("-nowriteconfig") )
		return;

	DevMsg( "Touch_WriteConfig(): %s\n", touch_config_file.GetString() );

	Q_snprintf( newconfigfile, 64, "cfg/%s.new", touch_config_file.GetString() );
	Q_snprintf( oldconfigfile, 64, "cfg/%s.bak", touch_config_file.GetString() );
	Q_snprintf( configfile, 64, "cfg/%s", touch_config_file.GetString() );

	f = filesystem->Open( newconfigfile , "w+");

	if( f )
	{
		filesystem->FPrintf( f, "//=======================================================================\n");
		filesystem->FPrintf( f, "//\t\t\ttouchscreen config\n" );
		filesystem->FPrintf( f, "//=======================================================================\n" );
		filesystem->FPrintf( f, "\ntouch_config_file \"%s\"\n", touch_config_file.GetString() );
		filesystem->FPrintf( f, "\n// touch cvars\n" );
		filesystem->FPrintf( f, "\n// sensitivity settings\n" );
		filesystem->FPrintf( f, "touch_pitch \"%f\"\n", touch_pitch.GetFloat() );
		filesystem->FPrintf( f, "touch_yaw \"%f\"\n", touch_yaw.GetFloat() );
		filesystem->FPrintf( f, "touch_forwardzone \"%f\"\n", touch_forwardzone.GetFloat() );
		filesystem->FPrintf( f, "touch_sidezone \"%f\"\n", touch_sidezone.GetFloat() );
/*		filesystem->FPrintf( f, "touch_nonlinear_look \"%d\"\n",touch_nonlinear_look.GetBool() );
		filesystem->FPrintf( f, "touch_pow_factor \"%f\"\n", touch_pow_factor->value );
		filesystem->FPrintf( f, "touch_pow_mult \"%f\"\n", touch_pow_mult->value );
		filesystem->FPrintf( f, "touch_exp_mult \"%f\"\n", touch_exp_mult->value );*/
		filesystem->FPrintf( f, "\n// grid settings\n" );
		filesystem->FPrintf( f, "touch_grid_count \"%d\"\n", touch_grid_count.GetInt() );
		filesystem->FPrintf( f, "touch_grid_enable \"%d\"\n", touch_grid_enable.GetInt() );

		filesystem->FPrintf( f, "touch_setgridcolor \"%d\" \"%d\" \"%d\" \"%d\"\n", gridcolor.r, gridcolor.g, gridcolor.b, gridcolor.a );
		filesystem->FPrintf( f, "touch_button_info \"%d\"\n", touch_button_info.GetInt() );
/*
		filesystem->FPrintf( f, "\n// global overstroke (width, r, g, b, a)\n" );
		filesystem->FPrintf( f, "touch_set_stroke %d %d %d %d %d\n", touch.swidth, touch.scolor[0], touch.scolor[1], touch.scolor[2], touch.scolor[3] );
		filesystem->FPrintf( f, "\n// highlight when pressed\n" );
		filesystem->FPrintf( f, "touch_highlight_r \"%f\"\n", touch_highlight_r->value );
		filesystem->FPrintf( f, "touch_highlight_g \"%f\"\n", touch_highlight_g->value );
		filesystem->FPrintf( f, "touch_highlight_b \"%f\"\n", touch_highlight_b->value );
		filesystem->FPrintf( f, "touch_highlight_a \"%f\"\n", touch_highlight_a->value );
		filesystem->FPrintf( f, "\n// _joy and _dpad options\n" );
		filesystem->FPrintf( f, "touch_dpad_radius \"%f\"\n", touch_dpad_radius->value );
		filesystem->FPrintf( f, "touch_joy_radius \"%f\"\n", touch_joy_radius->value );
*/
		filesystem->FPrintf( f, "\n// how much slowdown when Precise Look button pressed\n" );
		filesystem->FPrintf( f, "touch_precise_amount \"%f\"\n", touch_precise_amount.GetFloat() );
//		filesystem->FPrintf( f, "\n// enable/disable move indicator\n" );
//		filesystem->FPrintf( f, "touch_move_indicator \"%f\"\n", touch_move_indicator );

		filesystem->FPrintf( f, "\n// reset menu state when execing config\n" );
		//filesystem->FPrintf( f, "touch_setclientonly 0\n" );
		filesystem->FPrintf( f, "\n// touch buttons\n" );
		filesystem->FPrintf( f, "touch_removeall\n" );

		CUtlVector<CTouchButton*>::iterator it;
		for( it = btns.begin(); it != btns.end(); it++ )
		{
			CTouchButton *b = *it;
		
			if( b->flags & TOUCH_FL_CLIENT )
				continue; //skip temporary buttons

			if( b->flags & TOUCH_FL_DEF_SHOW )
				b->flags &= ~TOUCH_FL_HIDE;

			if( b->flags & TOUCH_FL_DEF_HIDE )
				b->flags |= TOUCH_FL_HIDE;
			
			float aspect = ( b->y2 - b->y1 ) / ( ( b->x2 - b->x1 )/(screen_h/screen_w) );
	
			filesystem->FPrintf( f, "touch_addbutton \"%s\" \"%s\" \"%s\" %f %f %f %f %d %d %d %d %d %f\n",
				b->name, b->texturefile, b->command,
				b->x1, b->y1, b->x2, b->y2,
				b->color.r, b->color.g, b->color.b, b->color.a, b->flags, aspect );
		}

		filesystem->Close(f);

		filesystem->RemoveFile(oldconfigfile);
		filesystem->RenameFile(configfile, oldconfigfile );
		filesystem->RenameFile(newconfigfile, configfile);
	}
	else DevMsg( "Couldn't write %s.\n", configfile );
}
