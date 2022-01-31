/*
	Neutrino-GUI  -   DBoxII-Project

	Copyright (C) 2001 Steffen Hehn 'McClean'
	Homepage: http://dbox.cyberphoria.org/

	Kommentar:

	Diese GUI wurde von Grund auf neu programmiert und sollte nun vom
	Aufbau und auch den Ausbaumoeglichkeiten gut aussehen. Neutrino basiert
	auf der Client-Server Idee, diese GUI ist also von der direkten DBox-
	Steuerung getrennt. Diese wird dann von Daemons uebernommen.


	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>
#include <neutrino.h>
#include <gui/widget/icons.h>

#include <gui/epg_menu.h>

#include <gui/eventlist.h>
#include <gui/infoviewer.h>
#include <gui/epgplus.h>
#include <gui/streaminfo2.h>

#include <system/debug.h>


int CEPGMenuHandler::exec(CMenuTarget* parent, const std::string &)
{
	dprintf(DEBUG_NORMAL, "CEPGMenuHandler::exec:\n");
	
	int res = RETURN_EXIT_ALL;

	if (parent)
		parent->hide();

	doMenu();
	
	return res;
}

int CEPGMenuHandler::doMenu()
{
	CMenuWidget redMenu(_("EPG - Program Information"), NEUTRINO_ICON_BUTTON_EPG);
	
	id = WIDGET_EPGTIMER;
	name = "epgtimer";

	redMenu.setWidgetMode(MODE_MENU);
	redMenu.setWidgetType(WIDGET_TYPE_CLASSIC);
	redMenu.setMenuPosition(MENU_POSITION_CENTER);
	redMenu.enableShrinkMenu();
	redMenu.enableSaveScreen();

	// eventlist
	redMenu.addItem(new ClistBoxItem(_("Eventlist current programm"), true, NULL, new CEventListHandler(), "", RC_red, NEUTRINO_ICON_BUTTON_RED, NEUTRINO_ICON_MENUITEM_SLEEPTIMER));

	// epg view
	redMenu.addItem(new ClistBoxItem(_("Details current program"), true, NULL, new CEPGDataHandler(), "", RC_green, NEUTRINO_ICON_BUTTON_GREEN, NEUTRINO_ICON_MENUITEM_RESTART));
			
	// epgplus
	redMenu.addItem(new ClistBoxItem(_("Eventlist overview"), true, NULL, new CEPGplusHandler(), "", RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW, NEUTRINO_ICON_MENUITEM_STANDBY));

	//tech info
	redMenu.addItem(new ClistBoxItem(_("Stream information"), true, NULL, new CStreamInfo2Handler(), "", RC_blue, NEUTRINO_ICON_BUTTON_BLUE, NEUTRINO_ICON_MENUITEM_TIMERLIST));
	
	// timerlis
	redMenu.addItem(new ClistBoxItem(_("Timer list"), true, NULL, new CTimerList(), "", RC_1, NEUTRINO_ICON_BUTTON_1, NEUTRINO_ICON_MENUITEM_TIMERLIST));
					
	return redMenu.exec(NULL, "");
}





