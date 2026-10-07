#include "cbase.h"
#include "vgui_controls/Panel.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

#include "iviewport.h"

class CRadialMenuPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CRadialMenuPanel, vgui::Panel );
public:
	CRadialMenuPanel( IViewPort *pViewPort )
		: BaseClass( NULL, "RadialMenu" )
	{
		(void)pViewPort;
	}
};

#endif
