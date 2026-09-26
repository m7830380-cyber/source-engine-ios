//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A 3D item model (with its painted materials) drawn over part of a
//          Scaleform menu, placed from that menu's own on-screen layout. Used
//          for the inventory "Inspect" dialog and the buy menu's weapon view,
//          where the retail client drew the model itself. VGUI paints after
//          Scaleform, so the model shows on top; the panel takes no VGUI input.
//
//===========================================================================//
#ifndef FLASH_ITEM_MODEL_PANEL_H
#define FLASH_ITEM_MODEL_PANEL_H

#include "matsys_controls/mdlpanel.h"
#include "scaleformui/scaleformui.h"

class CEconItemView;

// Screen rect of a Flash movie clip's own bounds, or of a rect given in its local
// coordinates. The menu movies are SM_NoScale/TopLeft, so stage coordinates are
// screen pixels. nSlot is the Scaleform slot the clip lives in.
bool GetFlashClipScreenRect( SFVALUE clip, int nSlot, float &x0, float &y0, float &x1, float &y1 );
bool GetFlashLocalRectOnScreen( SFVALUE clip, int nSlot, float lx0, float ly0, float lx1, float ly1, float &x0, float &y0, float &x1, float &y1 );

class CFlashItemModelPanel : public CMDLPanel
{
	DECLARE_CLASS_SIMPLE( CFlashItemModelPanel, CMDLPanel );
public:
	// Where the model goes, in screen pixels: return 1 to use x/y/w/h, 0 to keep the
	// last placement, -1 to hide the model (its menu panel isn't showing)
	typedef int ( *PlacementFn_t )( int &x, int &y, int &w, int &h );

	CFlashItemModelPanel( const char *pszName, PlacementFn_t pfnPlacement, bool bTouchToTurn, bool bHideInGame );

	void ShowItem( CEconItemView *pItem );
	void Hide() { SetVisible( false ); }

	virtual void OnThink();

private:
	bool UpdatePlacement();	// false: hidden
	void UpdateModelTransform();

	PlacementFn_t m_pfnPlacement;
	bool	m_bTouchToTurn, m_bHideInGame;
	Vector	m_vecCenter;
	float	m_flRadius;
	float	m_flYaw, m_flPitch;
	double	m_flLastTime;
	bool	m_bDragging, m_bUserTurned;
	float	m_flDragX, m_flDragY;
};

#endif // FLASH_ITEM_MODEL_PANEL_H
