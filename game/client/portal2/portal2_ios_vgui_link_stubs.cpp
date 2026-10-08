#include "cbase.h"

#if defined( PORTAL2 ) && defined( CLIENT_DLL )

#include "ifpspanel.h"

// client_portal2_rubberwar.vpc excludes vgui_fpspanel.cpp; provide a no-op FPS panel.
#if defined( IOS )
class CPortal2IOSNullFPSPanel : public IFPSPanel
{
public:
	virtual void Create( vgui::VPANEL parent ) { (void)parent; }
	virtual void Destroy( void ) {}
};

static CPortal2IOSNullFPSPanel s_Portal2IOSNullFPSPanel;
IFPSPanel *fps = &s_Portal2IOSNullFPSPanel;
#endif

// VGUI symbols are provided by radialmenu.cpp and sdk_vgui_music_importer.cpp.

#endif
