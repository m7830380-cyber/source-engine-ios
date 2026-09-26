//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: see flash_item_model_panel.h
//
//===========================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )
#include "flash_item_model_panel.h"
#include "econ_item_view.h"
#include "vgui/ISurface.h"
#include "ienginevgui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#if defined( IOS )
extern bool IOS_GetTouch( float &x, float &y );
#endif

bool GetFlashLocalRectOnScreen( SFVALUE clip, int nSlot, float lx0, float ly0, float lx1, float ly1, float &x0, float &y0, float &x1, float &y1 )
{
	IScaleformUI *pui = g_pScaleformUI;
	if ( !pui || !clip )
		return false;

	const float in[4] = { lx0, ly0, lx1, ly1 };
	float out[4];
	for ( int corner = 0; corner < 2; corner++ )
	{
		SFVALUE pt = pui->CreateNewObject( nSlot );
		if ( !pt )
			return false;
		pui->Value_SetMember( pt, "x", in[ corner * 2 ] );
		pui->Value_SetMember( pt, "y", in[ corner * 2 + 1 ] );
		pui->Value_InvokeWithoutReturn( clip, "localToGlobal", pt, 1 );
		SFVALUE vx = pui->Value_GetMember( pt, "x" ), vy = pui->Value_GetMember( pt, "y" );
		out[ corner * 2 ] = vx ? (float)pui->Value_GetNumber( vx ) : 0.0f;
		out[ corner * 2 + 1 ] = vy ? (float)pui->Value_GetNumber( vy ) : 0.0f;
		if ( vx ) pui->ReleaseValue( vx );
		if ( vy ) pui->ReleaseValue( vy );
		pui->ReleaseValue( pt );
	}
	x0 = out[0]; y0 = out[1]; x1 = out[2]; y1 = out[3];
	return x1 > x0 && y1 > y0;
}

bool GetFlashClipScreenRect( SFVALUE clip, int nSlot, float &x0, float &y0, float &x1, float &y1 )
{
	IScaleformUI *pui = g_pScaleformUI;
	if ( !pui || !clip )
		return false;

	SFVALUE bounds = pui->Value_Invoke( clip, "getBounds", clip, 1 );	// in its own space
	if ( !bounds )
		return false;
	float b[4] = { 0, 0, 0, 0 };
	static const char *s_pszKeys[4] = { "xMin", "yMin", "xMax", "yMax" };
	for ( int i = 0; i < 4; i++ )
	{
		SFVALUE v = pui->Value_GetMember( bounds, s_pszKeys[i] );
		if ( v )
		{
			b[i] = (float)pui->Value_GetNumber( v );
			pui->ReleaseValue( v );
		}
	}
	pui->ReleaseValue( bounds );
	if ( b[2] <= b[0] || b[3] <= b[1] )
		return false;

	return GetFlashLocalRectOnScreen( clip, nSlot, b[0], b[1], b[2], b[3], x0, y0, x1, y1 );
}

CFlashItemModelPanel::CFlashItemModelPanel( const char *pszName, PlacementFn_t pfnPlacement, bool bTouchToTurn, bool bHideInGame )
	: BaseClass( NULL, pszName )
{
	m_pfnPlacement = pfnPlacement;
	m_bTouchToTurn = bTouchToTurn;
	m_bHideInGame = bHideInGame;

	SetParent( enginevgui->GetPanel( PANEL_ROOT ) );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetVisible( false );
	SetBackgroundColor( Color( 22, 25, 29, 255 ) );
	SetCameraFOV( 40.0f );
	m_vecCenter.Init();
	m_flRadius = 1.0f;
	m_flYaw = m_flPitch = 0.0f;
	m_flLastTime = 0.0;
	m_bDragging = m_bUserTurned = false;
	m_flDragX = m_flDragY = 0.0f;
}

void CFlashItemModelPanel::ShowItem( CEconItemView *pItem )
{
	const char *pszModel = pItem ? pItem->GetWorldDisplayModel() : NULL;
	if ( !pszModel || !pszModel[0] )
	{
		SetVisible( false );
		return;
	}

	// starts building the painted materials; they show once ready
	pItem->UpdateGeneratedMaterial();
	SetMDL( pszModel, pItem );

	// bounding sphere in model space (identity transform first)
	SetModelAnglesAndPosition( vec3_angle, vec3_origin );
	if ( !GetBoundingSphere( m_vecCenter, m_flRadius ) )
	{
		m_vecCenter.Init();
		m_flRadius = 16.0f;
	}

	m_flYaw = m_flPitch = 0.0f;
	m_bDragging = m_bUserTurned = false;
	m_flLastTime = Plat_FloatTime();
	SetVisible( true );
	if ( !UpdatePlacement() )
		return;
	UpdateModelTransform();
	MoveToFront();
}

void CFlashItemModelPanel::OnThink()
{
	BaseClass::OnThink();
	if ( !IsVisible() )
		return;
	if ( m_bHideInGame && engine->IsConnected() )
	{
		SetVisible( false );
		return;
	}

	// follows the menu (its panels animate in)
	if ( !UpdatePlacement() )
		return;

	double flNow = Plat_FloatTime();
	float flDelta = (float)( flNow - m_flLastTime );
	m_flLastTime = flNow;

	int x, y, w, h;
	GetBounds( x, y, w, h );
	int sw, sh;
	vgui::surface()->GetScreenSize( sw, sh );

	float tx = 0.0f, ty = 0.0f;
	bool bTouch = false;
#if defined( IOS )
	if ( m_bTouchToTurn )
		bTouch = IOS_GetTouch( tx, ty );
#endif
	if ( bTouch )
	{
		float px = tx * sw, py = ty * sh;
		if ( !m_bDragging )
		{
			// a drag starts on the model
			if ( px >= x && px < x + w && py >= y && py < y + h )
			{
				m_bDragging = true;
				m_bUserTurned = true;
			}
		}
		else
		{
			float flScale = 360.0f / MAX( w, 1 );	// a panel-width drag turns it once around
			m_flYaw = fmodf( m_flYaw + ( px - m_flDragX ) * flScale, 360.0f );
			m_flPitch = clamp( m_flPitch + ( py - m_flDragY ) * flScale * 0.5f, -60.0f, 60.0f );
		}
		m_flDragX = px;
		m_flDragY = py;
	}
	else
	{
		m_bDragging = false;
	}

	if ( !m_bUserTurned )
		m_flYaw = fmodf( m_flYaw + 30.0f * flDelta, 360.0f );

	UpdateModelTransform();
}

bool CFlashItemModelPanel::UpdatePlacement()
{
	int nx, ny, nw, nh;
	int nResult = m_pfnPlacement ? m_pfnPlacement( nx, ny, nw, nh ) : 0;
	if ( nResult < 0 )
	{
		SetVisible( false );
		return false;
	}
	if ( nResult == 0 || nw <= 0 || nh <= 0 )
	{
		if ( GetWide() > 0 && GetTall() > 0 )
			return true;	// keep the last placement
		int sw, sh;
		vgui::surface()->GetScreenSize( sw, sh );
		nx = sw * 30 / 100; ny = sh * 28 / 100; nw = sw * 40 / 100; nh = sh * 44 / 100;
	}

	int ox, oy, ow, oh;
	GetBounds( ox, oy, ow, oh );
	if ( ox != nx || oy != ny || ow != nw || oh != nh )
	{
		SetBounds( nx, ny, nw, nh );
		LookAt( vec3_origin, m_flRadius );	// refit to the new size
	}
	return true;
}

// turn about the model's own center, kept at the camera's pivot
void CFlashItemModelPanel::UpdateModelTransform()
{
	QAngle ang( m_flPitch, m_flYaw, 0.0f );
	matrix3x4_t mat;
	AngleMatrix( ang, mat );
	Vector vecRotatedCenter;
	VectorRotate( m_vecCenter, mat, vecRotatedCenter );
	SetModelAnglesAndPosition( ang, -vecRotatedCenter );
}

#endif // INCLUDE_SCALEFORM
