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
	// the weapon's viewmodel with its sticker meshes, each drawn with the sticker
	// material pStickers built for that slot (pItem's own, or a preview copy with
	// one more sticker); the view is kept while the model stays the same
	void ShowItemWithStickers( CEconItemView *pItem, CEconItemView *pStickers );
	void Hide() { SetVisible( false ); }

	virtual void OnThink();

protected:
	virtual void OnModelDrawPassStart( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) OVERRIDE;
	virtual void OnModelDrawPassFinished( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) OVERRIDE;

private:
	void ShowModel( const char *pszModel, CEconItemView *pItem, bool bKeepView );
	bool UpdatePlacement();	// false: hidden
	void UpdateModelTransform();
	void ApplyZoom();		// camera distance for the model's size and m_flZoom
	bool UpdatePinch( int x, int y, int w, int h, int sw, int sh );	// true while two fingers zoom

	struct StickerMerge_t
	{
		const studiohdr_t *m_pHdr;
		CMaterialReference m_Material;
	};
	CUtlVector< StickerMerge_t > m_vecStickerMerges;
	bool	m_bStickerOverride;
	CUtlString m_strModel;

	PlacementFn_t m_pfnPlacement;
	bool	m_bTouchToTurn, m_bHideInGame;
	Vector	m_vecCenter;
	float	m_flRadius;
	float	m_flYaw, m_flPitch;
	double	m_flLastTime;
	bool	m_bDragging, m_bUserTurned;
	float	m_flDragX, m_flDragY;
	float	m_flZoom;						// 1 = the whole model fits
	bool	m_bPinching, m_bWaitRelease;	// after a pinch, no turning until the fingers lift
	float	m_flPinchStartDist, m_flPinchStartZoom;
};

#endif // FLASH_ITEM_MODEL_PANEL_H
