//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Touch-friendly VGUI buy menu (PANEL_BUY)
//
//===========================================================================//

#ifndef IOS_BUYMENU_H
#define IOS_BUYMENU_H

class IViewPort;
class IViewPortPanel;

IViewPortPanel *IOS_CreateBuyMenu( IViewPort *pViewPort );
bool IOS_IsBuyMenuVisible();

#endif // IOS_BUYMENU_H
