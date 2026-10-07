#include "cbase.h"
#include "vgui_controls/Panel.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

class IViewPort;

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

class MusicImporterDialog
{
public:
	static void OpenImportDialog( vgui::Panel *parent )
	{
		(void)parent;
	}
};

#endif
