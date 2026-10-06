#ifndef PORTAL_STATS_PANEL_H
#define PORTAL_STATS_PANEL_H
#ifdef _WIN32
#pragma once
#endif

namespace vgui
{
	class Panel;
}

class CPortalStatsPanel : public vgui::Panel
{
public:
	CPortalStatsPanel( vgui::Panel *parent, const char *name ) : vgui::Panel( parent, name ) {}
};

#endif
