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

#include <gui/channellist.h>

#include <global.h>
#include <neutrino.h>

#include <driver/fontrenderer.h>
#include <driver/screen_max.h>
#include <driver/rcinput.h>

#include <gui/color.h>
#include <gui/eventlist.h>
#include <gui/infoviewer.h>
#include <gui/widget/buttons.h>
#include <gui/widget/icons.h>
#include <gui/widget/menue.h>
#include <gui/widget/messagebox.h>

#include <system/settings.h>
#include <system/lastchannel.h>
#include "gui/filebrowser.h"

#include <gui/bouquetlist.h>
#include <daemonc/remotecontrol.h>
#include <driver/vcrcontrol.h>
#include <gui/pictureviewer.h>

/*zapit includes*/
#include <client/zapittools.h>
#include <bouquets.h>
#include <satconfig.h>
#include <getservices.h>
#include <frontend_c.h>

#include <gui/epgplus.h>
#include <system/debug.h>

#include <video_cs.h>


extern CBouquetList * bouquetList;      		/* neutrino.cpp */
extern CRemoteControl * g_RemoteControl; 		/* neutrino.cpp */
extern SMSKeyInput * c_SMSKeyInput;			// defined in neutrino
extern CPictureViewer * g_PicViewer;
extern CBouquetList   * TVbouquetList;
extern CBouquetList   * TVsatList;
extern CBouquetList   * TVfavList;
extern CBouquetList   * TVallList;
extern CBouquetList   * RADIObouquetList;
extern CBouquetList   * RADIOsatList;
extern CBouquetList   * RADIOfavList;
extern CBouquetList   * RADIOallList;

#define PIC_W 78

extern t_channel_id rec_channel_id;
extern t_channel_id live_channel_id;
bool pip_selected = false;
extern bool autoshift;
int info_height = 0;
bool new_mode_active = 0;

extern int FrontendCount;			// defined in zapit.cpp
extern CFrontend * getFE(int index);		// defined in zapit.cpp
extern bool feCanTune(CZapitChannel * thischannel);

extern CBouquetManager * g_bouquetManager;

extern cVideo * videoDecoder;
extern CZapitChannel * live_channel;

/* events */
void sectionsd_getChannelEvents(CChannelEventList &eList, const bool tv_mode, t_channel_id *chidlist, int clen);
void sectionsd_getEventsServiceKey(t_channel_id serviceUniqueKey, CChannelEventList &eList, char search = 0, std::string search_text = "");
void addChannelToBouquet(const unsigned int bouquet, const t_channel_id channel_id);

extern int old_b_id;

CChannelList::CChannelList(const char * const Name, bool historyMode, bool _vlist)
{
	frameBuffer = CFrameBuffer::getInstance();
	name = Name;
	selected = 0;
	liststart = 0;
	tuned = 0xfffffff;
	zapProtection = NULL;
	this->historyMode = historyMode;
	vlist = _vlist;
	
	//printf("CChannelList::CChannelList: add %s : %x\n", name.c_str(), this);fflush(stdout);
}

CChannelList::~CChannelList()
{
	//printf("DELETE LIST %s : %x\n", name.c_str(), this);fflush(stdout);
	
	chanlist.clear();
}

void CChannelList::ClearList(void)
{
	//printf("CLEAR LIST %s : %x\n", name.c_str(), this);fflush(stdout);
	
	chanlist.clear();
	chanlist.resize(1);
}

void CChannelList::setSize(int newsize)
{
	chanlist.reserve(newsize);
}

void CChannelList::addChannel(CZapitChannel * channel, int num)
{
	//printf("CChannelList::addChannel: %s %s\n", name.c_str(), channel->getName().c_str());fflush(stdout);
	
	if(num)
		channel->number = num;
	
	chanlist.push_back(channel);
}

void CChannelList::putChannel(CZapitChannel * channel)
{
	int num = channel->number - 1;
	
	if(num < 0) 
	{
		//printf("CChannelList::addChannel error inserting at %d\n", num);
		return;
	}
	
	if(num >= (int) chanlist.size()) 
	{
		chanlist.resize((unsigned) num + 1);
	}
	chanlist[num] = channel;

	//printf("%s : me %x putChannel: %d: %s %x -> %x [0] %x\n", name.c_str(), this, num, channel->getName().c_str(), channel, chanlist[num], chanlist[0]);fflush(stdout);
}

void CChannelList::updateEvents(void)
{
	CChannelEventList events;

	if (displayNext) 
	{
		if (chanlist.size()) 
		{
			time_t atime = time(NULL);
			unsigned int count;
			
			for (count = 0; count < chanlist.size(); count++)
			{		
				//
				events.clear();
				//
				sectionsd_getEventsServiceKey(chanlist[count]->channel_id, events);
				chanlist[count]->nextEvent.startTime = (long)0x7fffffff;
				
				for ( CChannelEventList::iterator e = events.begin(); e != events.end(); ++e ) 
				{
					if (((long)(e->startTime) > atime) && ((e->startTime) < (long)(chanlist[count]->nextEvent.startTime)))
					{
						chanlist[count]->nextEvent= *e;
						break;
					}
				}
			}
		}
	} 
	else 
	{
		t_channel_id * p_requested_channels = NULL;
		int size_requested_channels = 0;

		if (chanlist.size()) 
		{
			size_requested_channels = chanlist.size()*sizeof(t_channel_id);
			p_requested_channels    = (t_channel_id*)malloc(size_requested_channels);
			
			for (uint32_t count = 0; count < chanlist.size(); count++)
			{
				p_requested_channels[count] = chanlist[count]->channel_id&0xFFFFFFFFFFFFULL;
			}

			CChannelEventList events;
			sectionsd_getChannelEvents(events, (CNeutrinoApp::getInstance()->getMode()) != NeutrinoMessages::mode_radio, p_requested_channels, size_requested_channels);
			
			for (uint32_t count = 0; count < chanlist.size(); count++) 
			{
				chanlist[count]->currentEvent = CChannelEvent();
				
				for ( CChannelEventList::iterator e = events.begin(); e != events.end(); ++e )
				{
					if ((chanlist[count]->channel_id&0xFFFFFFFFFFFFULL) == e->get_channel_id())
					{
						chanlist[count]->currentEvent= *e;
						break;
					}
				}
			}
			if (p_requested_channels != NULL) 
				free(p_requested_channels);
		}
	}
	
	events.clear();
}

struct CmpChannelBySat: public binary_function <const CZapitChannel * const, const CZapitChannel * const, bool>
{
        static bool comparetolower(const char a, const char b)
        {
		return tolower(a) < tolower(b);
        };

        bool operator() (const CZapitChannel * const c1, const CZapitChannel * const c2)
        {
		if(c1->getSatellitePosition() == c2->getSatellitePosition())
			return std::lexicographical_compare(c1->getName().begin(), c1->getName().end(), c2->getName().begin(), c2->getName().end(), comparetolower);
		else
			return c1->getSatellitePosition() < c2->getSatellitePosition();
;
	};
};

struct CmpChannelByFreq: public binary_function <const CZapitChannel * const, const CZapitChannel * const, bool>
{
        static bool comparetolower(const char a, const char b)
        {
                return tolower(a) < tolower(b);
        };

        bool operator() (const CZapitChannel * const c1, const CZapitChannel * const c2)
        {
		if(c1->getFreqId() == c2->getFreqId())
			return std::lexicographical_compare(c1->getName().begin(), c1->getName().end(), c2->getName().begin(), c2->getName().end(), comparetolower);
		else
			return c1->getFreqId() < c2->getFreqId();
;
	};
};

void CChannelList::SortAlpha(void)
{
	sort(chanlist.begin(), chanlist.end(), CmpChannelByChName());
}

void CChannelList::SortSat(void)
{
	sort(chanlist.begin(), chanlist.end(), CmpChannelBySat());
}

CZapitChannel * CChannelList::getChannel(int number)
{
	for (uint32_t i = 0; i< chanlist.size();i++) 
	{
		if (chanlist[i]->number == number)
			return chanlist[i];
	}
	
	return(NULL);
}

CZapitChannel * CChannelList::getChannel(t_channel_id channel_id)
{
	for (uint32_t i = 0; i< chanlist.size();i++) 
	{
		if (chanlist[i]->channel_id == channel_id)
			return chanlist[i];
	}
	
	return(NULL);
}

int CChannelList::getKey(int id)
{
	return chanlist[id]->number;
}

static const std::string empty_string;

const std::string & CChannelList::getActiveChannelName(void) const
{
	if (selected < chanlist.size())
		return chanlist[selected]->name;
	else
		return empty_string;
}

t_satellite_position CChannelList::getActiveSatellitePosition(void) const
{
	if (selected < chanlist.size())
		return chanlist[selected]->getSatellitePosition();
	else
		return 0;
}

t_channel_id CChannelList::getActiveChannel_ChannelID(void) const
{
	if (selected < chanlist.size()) 
	{
		//printf("CChannelList::getActiveChannel_ChannelID me %x selected = %d %llx\n", (int) this, selected, chanlist[selected]->channel_id);
		return chanlist[selected]->channel_id;
	} 
	else
		return 0;
}

int CChannelList::getActiveChannelNumber(void) const
{
	return (selected + 1);
}

int CChannelList::doChannelMenu(void)
{
	int i = 0;
	int select = -1;
	static int old_selected = 0;
	int ret = menu_return::RETURN_NONE;
	signed int bouquet_id, old_bouquet_id, new_bouquet_id;
	int result;
	char cnt[5];
	t_channel_id channel_id;
	
	if( !bouquetList )
		return 0;

	CMenuWidget * menu = new CMenuWidget(LOCALE_CHANNELLIST_EDIT, NEUTRINO_ICON_SETTINGS);
	menu->enableSaveScreen(true);
	
	CMenuSelectorTarget * selector = new CMenuSelectorTarget(&select);

	sprintf(cnt, "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_BOUQUETEDITOR_DELETE, true, NULL, selector, cnt, CRCInput::RC_red, NEUTRINO_ICON_BUTTON_RED), old_selected == i++);

	sprintf(cnt, "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_BOUQUETEDITOR_MOVE, true, NULL, selector, cnt, CRCInput::RC_green, NEUTRINO_ICON_BUTTON_GREEN), old_selected == i++);

	sprintf(cnt, "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_EXTRA_ADD_TO_BOUQUET, true, NULL, selector, cnt, CRCInput::RC_yellow, NEUTRINO_ICON_BUTTON_YELLOW), old_selected == i++);

	sprintf(cnt, "%d", i);
	menu->addItem(new CMenuForwarder(LOCALE_FAVORITES_MENUEADD, true, NULL, selector, cnt, CRCInput::RC_blue, NEUTRINO_ICON_BUTTON_BLUE), old_selected == i++);

	ret = menu->exec(NULL, "");
	delete menu;
	delete selector;

	if(select >= 0) 
	{
		old_selected = select;
		channel_id = chanlist[selected]->channel_id;
		
		switch(select) 
		{
			case 0: //delete
				hide();
				result = ShowMsgUTF ( LOCALE_BOUQUETEDITOR_DELETE, "Delete channel from bouquet?", CMessageBox::mbrNo, CMessageBox::mbYes | CMessageBox::mbNo );

				if(result == CMessageBox::mbrYes) 
				{
					bouquet_id = bouquetList->getActiveBouquetNumber();
					bouquet_id = g_bouquetManager->existsBouquet(bouquetList->Bouquets[bouquet_id]->channelList->getName());
					if (bouquet_id == -1)
						return 0;
					
					if(g_bouquetManager->existsChannelInBouquet(bouquet_id, channel_id)) 
					{
						g_bouquetManager->Bouquets[bouquet_id]->removeService(channel_id);
						return 1;
					}
				}
				break;
				
			case 1: // move
				old_bouquet_id = bouquetList->getActiveBouquetNumber();
				old_bouquet_id = g_bouquetManager->existsBouquet(bouquetList->Bouquets[old_bouquet_id]->channelList->getName());

				do {
					new_bouquet_id = bouquetList->exec(false);
				} while(new_bouquet_id == -3);

				hide();
				if(new_bouquet_id < 0)
					return 0;
				new_bouquet_id = g_bouquetManager->existsBouquet(bouquetList->Bouquets[new_bouquet_id]->channelList->getName());
				if ((new_bouquet_id == -1) || (new_bouquet_id == old_bouquet_id))
					return 0;

				if(!g_bouquetManager->existsChannelInBouquet(new_bouquet_id, channel_id)) 
				{
					addChannelToBouquet(new_bouquet_id, channel_id);
				}
				
				if(g_bouquetManager->existsChannelInBouquet(old_bouquet_id, channel_id)) 
				{
					g_bouquetManager->Bouquets[old_bouquet_id]->removeService(channel_id);
				}
				return 1;

				break;
				
			case 2: // add to
				do {
					bouquet_id = bouquetList->exec(false);
				} while(bouquet_id == -3);
				
				hide();
				
				if(bouquet_id < 0)
					return 0;
				
				bouquet_id = g_bouquetManager->existsBouquet(bouquetList->Bouquets[bouquet_id]->channelList->getName());
				if (bouquet_id == -1)
					return 0;
				
				if(!g_bouquetManager->existsChannelInBouquet(bouquet_id, channel_id)) 
				{
					addChannelToBouquet(bouquet_id, channel_id);
					return 1;
				}
				break;
				
			case 3: // add to my favorites
				bouquet_id = g_bouquetManager->existsUBouquet(g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME), true);
				if(bouquet_id == -1) 
				{
					g_bouquetManager->addBouquet(g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME), true);
					bouquet_id = g_bouquetManager->existsUBouquet(g_Locale->getText(LOCALE_FAVORITES_BOUQUETNAME), true);
				}
				
				if(!g_bouquetManager->existsChannelInBouquet(bouquet_id, channel_id)) 
				{
					addChannelToBouquet(bouquet_id, channel_id);
					return 1;
				}
				
				break;
				
			default:
				break;
		}
	}
	
	return 0;
}

int CChannelList::exec()
{
	displayNext = 0; // always start with current events
	
	int nNewChannel = show();
	
	// zapto
	if ( nNewChannel > -1 && nNewChannel < (int) chanlist.size()) 
		CNeutrinoApp::getInstance()->channelList->zapTo(getKey(nNewChannel)-1);

	return nNewChannel;
}

#define CHANNEL_SMSKEY_TIMEOUT 800
/* return: >= 0 to zap, -1 on cancel, -3 on list mode change, -4 list edited, -2 zap but no restore old list/chan ?? */ //TODO:add return value for pip
int CChannelList::show()
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;
	bool actzap = 0;
	int res = -1;

	new_mode_active = 0;
	
	// windows size
	if(g_settings.mini_tv)
	{
		width  = 755;
		height = 600;
	}
	else
	{
		width  = w_max ( (frameBuffer->getScreenWidth() / 20 * 17), (frameBuffer->getScreenWidth() / 20 ));
		height = h_max ( (frameBuffer->getScreenHeight() / 20 * 16), (frameBuffer->getScreenHeight() / 20));
	}

	if (chanlist.empty()) 
	{
		return res;
	}

	// display channame in vfd	
	CVFD::getInstance()->setMode(CVFD::MODE_MENU_UTF8 );	

	buttonHeight = 7 + std::min(16, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL]->getHeight());
	theight = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getHeight();

	fheight = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getHeight();
	listmaxshow = (height - theight - buttonHeight -0)/fheight;
	height = theight + buttonHeight + listmaxshow * fheight;
	info_height = fheight + g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getHeight() + 10;
	
	if(g_settings.mini_tv)
	{
		x = frameBuffer->getScreenX() + 10;
		y = frameBuffer->getScreenY() + 10;
	}
	else
	{
		x = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - width) / 2;
		y = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - (height+ info_height)) / 2;
	}

	displayNext = false;

	// head
	paintHead();
	
	// update events
	updateEvents();
	
	// minitv
	if(g_settings.mini_tv)
		paintMiniTV();
	
	// paint all
	paint();
		
#if !defined USE_OPENGL
	frameBuffer->blit();
#endif	

	int oldselected = selected;
	int zapOnExit = false;
	bool bShowBouquetList = false;

	// loop control
	unsigned long long timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);

	bool loop = true;
	while (loop) 
	{
		g_RCInput->getMsgAbsoluteTimeout(&msg, &data, &timeoutEnd );
		
		if ( msg <= CRCInput::RC_MaxRC )
			timeoutEnd = CRCInput::calcTimeoutEnd(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);

		if ( ( msg == CRCInput::RC_timeout ) || ( msg == (neutrino_msg_t)g_settings.key_channelList_cancel) ) 
		{
			if(!actzap)
				selected = oldselected;
			
			loop = false;
			res = -1;
		}
		else if ((msg == CRCInput::RC_red) || (msg == CRCInput::RC_epg)) // epg
		{
			hide();

			if ( g_EventList->exec(chanlist[selected]->channel_id, chanlist[selected]->name) == menu_return::RETURN_EXIT_ALL) 
			{
				res = -2;
				loop = false;
			}
			
			paintHead();
			if(g_settings.mini_tv)
				paintMiniTV();
			paint();
		}
		else if ( msg == CRCInput::RC_yellow && ( bouquetList != NULL ) ) //bouquets
		{ 
			//FIXME: show bqt list
			bShowBouquetList = true;
			loop = false;
		}
		else if( msg == CRCInput::RC_blue ) //epgplus
		{
			hide();
			
			CEPGplusHandler eplus;
			eplus.exec(NULL, "");
			
			//FIXME: think about this ???
			//loop = false;
			paintHead();
			updateEvents();
			if(g_settings.mini_tv)
				paintMiniTV();
			paint();
		}
		else if ( msg == CRCInput::RC_sat || msg == CRCInput::RC_favorites)
		{
			g_RCInput->postMsg (msg, 0);
			loop = false;
			res = -1;
		}
		else if ( msg == CRCInput::RC_setup ) 
		{
			// chan list setup (add/move)
			old_b_id = bouquetList->getActiveBouquetNumber();
			int ret = doChannelMenu();
			
			if(ret) 
			{
				res = -4;
				loop = false;
			} 
			else 
			{
				old_b_id = -1;
				paintHead();
				if(g_settings.mini_tv)
					paintMiniTV();
				paint();
			}
			
		}
		else if (msg == (neutrino_msg_t) g_settings.key_list_start) 
		{
			selected = 0;
			liststart = (selected/listmaxshow)*listmaxshow;
			paint();
			
			if(new_mode_active) 
			{ 
				actzap = true; 
				zapTo(selected); 
			}
		}
		else if (msg == (neutrino_msg_t) g_settings.key_list_end) 
		{
			selected=chanlist.size()-1;
			liststart = (selected/listmaxshow)*listmaxshow;
			paint();
			
			if(new_mode_active) 
			{ 
				actzap = true; 
				zapTo(selected); 
			}
		}
                else if ( msg == CRCInput::RC_up || (int) msg == g_settings.key_channelList_pageup )
                {
                        int step = 0;
                        int prev_selected = selected;

                        step =  ((int) msg == g_settings.key_channelList_pageup) ? listmaxshow : 1;  // browse or step 1
                        selected -= step;
                        if((prev_selected-step) < 0)            // because of uint
                                selected = chanlist.size() - 1;

                        paintItem(prev_selected - liststart);
                        unsigned int oldliststart = liststart;
                        liststart = (selected/listmaxshow)*listmaxshow;
                        if(oldliststart!=liststart)
                                paint();
                        else
                                paintItem(selected - liststart);

			if(new_mode_active) 
			{ 
				actzap = true; 
				zapTo(selected); 
			}
                }
                else if ( msg == CRCInput::RC_down || (int) msg == g_settings.key_channelList_pagedown )
                {
                        unsigned int step = 0;
                        int prev_selected = selected;

                        step =  ((int) msg == g_settings.key_channelList_pagedown) ? listmaxshow : 1;  // browse or step 1
                        selected += step;

                        if(selected >= chanlist.size()) 
			{
                                if (((chanlist.size() / listmaxshow) + 1) * listmaxshow == chanlist.size() + listmaxshow) // last page has full entries
                                        selected = 0;
                                else
                                        selected = ((step == listmaxshow) && (selected < (((chanlist.size() / listmaxshow)+1) * listmaxshow))) ? (chanlist.size() - 1) : 0;
			}

                        paintItem(prev_selected - liststart);
                        unsigned int oldliststart = liststart;
                        liststart = (selected/listmaxshow)*listmaxshow;
                        if(oldliststart!=liststart)
                                paint();
                        else
                                paintItem(selected - liststart);

			if(new_mode_active) 
			{ 
				actzap = true; 
				zapTo(selected); 
			}
                }
		else if ((msg == (neutrino_msg_t)g_settings.key_bouquet_up) && (bouquetList != NULL)) 
		{
			if (bouquetList->Bouquets.size() > 0) 
			{
				bool found = true;
				uint32_t nNext = (bouquetList->getActiveBouquetNumber()+1) % bouquetList->Bouquets.size();
				if(bouquetList->Bouquets[nNext]->channelList->getSize() <= 0) 
				{
					found = false;
					nNext = nNext < bouquetList->Bouquets.size() - 1 ? nNext + 1 : 0;
					for(uint32_t i = nNext; i < bouquetList->Bouquets.size(); i++) 
					{
						if(bouquetList->Bouquets[i]->channelList->getSize() > 0) 
						{
							found = true;
							nNext = i;
							break;
						}
					}
				}
				
				if(found) 
				{
					bouquetList->activateBouquet(nNext, false);
					res = bouquetList->showChannelList();
					loop = false;
				}
			}
		}
		else if ((msg == (neutrino_msg_t)g_settings.key_bouquet_down) && (bouquetList != NULL)) 
		{
			if (bouquetList->Bouquets.size() > 0) 
			{
				bool found = true;
				int nNext = (bouquetList->getActiveBouquetNumber()+bouquetList->Bouquets.size()-1) % bouquetList->Bouquets.size();
				if(bouquetList->Bouquets[nNext]->channelList->getSize() <= 0) 
				{
					found = false;
					nNext = nNext > 0 ? nNext-1 : bouquetList->Bouquets.size()-1;
					
					for(int i = nNext; i > 0; i--) 
					{
						if(bouquetList->Bouquets[i]->channelList->getSize() > 0) 
						{
							found = true;
							nNext = i;
							break;
						}
					}
				}
				
				if(found) 
				{
					bouquetList->activateBouquet(nNext, false);
					res = bouquetList->showChannelList();
					loop = false;
				}
			}
		}
		else if ( msg == CRCInput::RC_ok ) 
		{	  
			zapOnExit = true;
			
			loop = false;
		}
		else if ( msg == CRCInput::RC_spkr ) 
		{
			new_mode_active = (new_mode_active ? 0 : 1);
			paintHead();

			// refresh logo box
			frameBuffer->paintBoxRel(x + width - 90 - PIC_W, y, PIC_W, theight, COL_MENUHEAD_PLUS_0);
	
			// paint logo
			g_PicViewer->DisplayLogo(chanlist[selected]->channel_id, x + width - 90 - PIC_W, y, PIC_W, theight);
		}
		else if (CRCInput::isNumeric(msg) && ( this->historyMode || g_settings.sms_channel)) 
		{ 
			if (this->historyMode) 
			{ 
				selected = CRCInput::getNumericValue(msg);
				zapOnExit = true;
				loop = false;
    			}
			else if(g_settings.sms_channel) 
			{
				uint32_t i;
				unsigned char smsKey = 0;
				c_SMSKeyInput->setTimeout(CHANNEL_SMSKEY_TIMEOUT);

				do {
					smsKey = c_SMSKeyInput->handleMsg(msg);
					//printf("SMS new key: %c\n", smsKey);
					g_RCInput->getMsg_ms(&msg, &data, CHANNEL_SMSKEY_TIMEOUT-100);
				} while ((msg >= CRCInput::RC_1) && (msg <= CRCInput::RC_9));

				if (msg == CRCInput::RC_timeout || msg == CRCInput::RC_nokey) 
				{
					for(i = selected + 1; i < chanlist.size(); i++) 
					{
						char firstCharOfTitle = chanlist[i]->name.c_str()[0];
						if(tolower(firstCharOfTitle) == smsKey) 
						{
							//printf("SMS chan found was= %d selected= %d i= %d %s\n", was_sms, selected, i, chanlist[i]->channel->name.c_str());
							break;
						}
					}
					
					if(i >= chanlist.size()) 
					{
						for(i = 0; i < chanlist.size(); i++) 
						{
							char firstCharOfTitle = chanlist[i]->name.c_str()[0];
							if(tolower(firstCharOfTitle) == smsKey) 
							{
								//printf("SMS chan found was= %d selected= %d i= %d %s\n", was_sms, selected, i, chanlist[i]->channel->name.c_str());
								break;
							}
						}
					}
					
					if(i < chanlist.size()) 
					{
						int prevselected = selected;
						selected = i;

						paintItem(prevselected - liststart);
						unsigned int oldliststart = liststart;
						liststart = (selected/listmaxshow)*listmaxshow;
						if(oldliststart!=liststart) 
						{
							paint();
						} 
						else 
						{
							paintItem(selected - liststart);
						}
					}
					c_SMSKeyInput->resetOldKey();
				}
			}
		}
		else if(CRCInput::isNumeric(msg)) 
		{
			//pushback key if...
			selected = oldselected;
			g_RCInput->postMsg( msg, data );
			loop = false;
		}
		else if ( msg == CRCInput::RC_green ) //next
		{
			displayNext = !displayNext;
			paintHead(); 		// update button bar
			updateEvents();
			if(g_settings.mini_tv)
				paintMiniTV();
			paint();
		} 
		else if ( (msg == CRCInput::RC_info) )
		{
			hide();
			g_EpgData->show(chanlist[selected]->channel_id); 
			paintHead();
			if(g_settings.mini_tv)
				paintMiniTV();
			paint();
		} 
		else 
		{
			if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) 
			{
				loop = false;
				res = - 2;
			}
		}
			
#if !defined USE_OPENGL
		frameBuffer->blit();
#endif		
	}
	
	hide();
	
	// bouquets mode
	if (bShowBouquetList) 
		res = bouquetList->exec(true);
	
	CVFD::getInstance()->setMode(CVFD::MODE_TVRADIO);
	
	new_mode_active = 0;

	if(NeutrinoMessages::mode_ts == CNeutrinoApp::getInstance()->getMode())
		return -1;

	if(zapOnExit)
		res = selected;

	printf("CChannelList::show res %d\n", res);
			
	return(res);
}

void CChannelList::hide()
{
	// hide minitv box
	if(g_settings.mini_tv)
		frameBuffer->paintBackgroundBoxRel(830, y, 400, 660);
	
	frameBuffer->paintBackgroundBoxRel(x, y, width + 5, height + info_height + 5);
		
#if !defined USE_OPENGL
	frameBuffer->blit();
#endif	
        clearItem2DetailsLine();
	
	// pig
	if(g_settings.mini_tv)
		videoDecoder->Pig(-1, -1, -1, -1);
}

bool CChannelList::showInfo(int pos, int epgpos)
{
	if((pos >= (signed int) chanlist.size()) || (pos < 0))
		return false;

	CZapitChannel * chan = chanlist[pos];
	
	// channel infobar
	g_InfoViewer->showTitle(pos+1, chan->name, chan->getSatellitePosition(), chan->channel_id, true, epgpos); // UTF-8
	
	return true;
}

int CChannelList::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if ( msg == NeutrinoMessages::EVT_PROGRAMLOCKSTATUS) 
	{
		// 0x100 als FSK-Status zeigt an, dass (noch) kein EPG zu einem Kanal der NICHT angezeigt
		// werden sollte (vorgesperrt) da ist
		// oder das bouquet des Kanals ist vorgesperrt

		printf("CChannelList::handleMsg: program-lock-status: %d\n", data);

		if ((g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_ONSIGNAL) || (g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_CHANGETOLOCKED))
		{
			if ( zapProtection != NULL )
				zapProtection->fsk = data;
			else 
			{
				// require password if either
				// CHANGETOLOCK mode and channel/bouquet is pre locked (0x100)
				// ONSIGNAL mode and fsk(data) is beyond configured value
				// if programm has already been unlocked, dont require pin
				if ((data >= (neutrino_msg_data_t)g_settings.parentallock_lockage) &&
					 ((chanlist[selected]->last_unlocked_EPGid != g_RemoteControl->current_EPGid) || (g_RemoteControl->current_EPGid == 0)) &&
					 ((g_settings.parentallock_prompt != PARENTALLOCK_PROMPT_CHANGETOLOCKED) || (data >= 0x100)))
				{
					g_RemoteControl->stopvideo();
					
					zapProtection = new CZapProtection( g_settings.parentallock_pincode, data );
					
					if ( zapProtection->check() )
					{
						g_RemoteControl->startvideo();
						
						// remember it for the next time
						chanlist[selected]->last_unlocked_EPGid = g_RemoteControl->current_EPGid;
					}
					delete zapProtection;
					zapProtection = NULL;
				}
				else
					g_RemoteControl->startvideo();
			}
		}
		else
			g_RemoteControl->startvideo();

		return messages_return::handled;
	}
	else
		return messages_return::unhandled;
}

/* bToo default to true */
bool CChannelList::adjustToChannelID(const t_channel_id channel_id, bool bToo)
{
	unsigned int i;

	//printf("CChannelList::adjustToChannelID me %x list size %d channel_id %llx\n", (int) this, chanlist.size(), channel_id);fflush(stdout);
	
	for (i = 0; i < chanlist.size(); i++) 
	{
		if(chanlist[i] == NULL) 
		{
			//printf("CChannelList::adjustToChannelID REPORT BUG !! %d is NULL !!\n", i);
			continue;
		}

		if (chanlist[i]->channel_id == channel_id) 
		{
			selected = i;
			lastChList.store (selected, channel_id, false);

			tuned = i;
			if (bToo && (bouquetList != NULL)) 
			{
				//bouquetList->adjustToChannel( getActiveChannelNumber());
				//bouquetList->adjustToChannelID(channel_id);
//FIXME
				if(CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_tv) 
				{
					TVbouquetList->adjustToChannelID(channel_id);
					TVsatList->adjustToChannelID(channel_id);
					TVfavList->adjustToChannelID(channel_id);
					TVallList->adjustToChannelID(channel_id);
				} 
				else if(CNeutrinoApp::getInstance()->getMode() == NeutrinoMessages::mode_radio) 
				{
					RADIObouquetList->adjustToChannelID(channel_id);
					RADIOsatList->adjustToChannelID(channel_id);
					RADIOfavList->adjustToChannelID(channel_id);
					RADIOallList->adjustToChannelID(channel_id);
				}
			}
			
			//printf("CChannelList::adjustToChannelID me %x to %llx bToo %s OK: %d\n", (int) this, channel_id, bToo ? "yes" : "no", i);fflush(stdout);
			
			return true;
		}
	}
	
	//printf("CChannelList::adjustToChannelID me %x to %llx bToo %s FAILED\n", (int) this, channel_id, bToo ? "yes" : "no");fflush(stdout);

	return false;
}

int CChannelList::hasChannel(int nChannelNr)
{
	for (uint32_t i = 0; i<chanlist.size(); i++) 
	{
		if (getKey(i) == nChannelNr)
			return(i);
	}
	
	return(-1);
}

int CChannelList::hasChannelID(t_channel_id channel_id)
{
	for (uint32_t i = 0; i < chanlist.size(); i++) 
	{
		if (chanlist[i]->channel_id == channel_id)
			return i;
	}
	
	return -1;
}

// for adjusting bouquet's channel list after numzap or quickzap
void CChannelList::setSelected( int nChannelNr)
{
	selected = nChannelNr;
}

// -- Zap to channel with channel_id
bool CChannelList::zapTo_ChannelID(const t_channel_id channel_id)
{
	printf("CChannelList::zapTo_ChannelID %llx\n", channel_id);
	
	for (unsigned int i = 0; i < chanlist.size(); i++) 
	{
		if (chanlist[i]->channel_id == channel_id) 
		{
			zapTo(i);
			return true;
		}
	}
	
	return false;
}

// forceStoreToLastChannels defaults to false
void CChannelList::zapTo(int pos, bool forceStoreToLastChannels)
{
	// show emty channellist error msg
	if (chanlist.empty()) 
	{
		DisplayErrorMessage(g_Locale->getText(LOCALE_CHANNELLIST_NONEFOUND)); // UTF-8
		return;
	}

	if ( (pos >= (signed int) chanlist.size()) || (pos < 0) ) 
	{
		pos = 0;
	}
	
	//TEST
	/* we record when we switched away from a channel, so that the parental-PIN code can
	   check for timeout. last_unlocked_time == 0 means: the PIN was not entered
	   "tuned" is the *old* channel, before zap 
	*/
	//if (tuned < chanlist.size() && chanlist[tuned]->last_unlocked_time != 0)
		//chanlist[tuned]->last_unlocked_time = time_monotonic();

	CZapitChannel * chan = chanlist[pos];
	
	printf("CChannelList::zapTo me %s tuned %d new %d %s -> %llx\n", name.c_str(), tuned, pos, chan->name.c_str(), chan->channel_id);
	
	if ( pos != (int)tuned ) 
	{
#if ENABLE_RADIOTEXT	  
		if ((g_settings.radiotext_enable) && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoMessages::mode_radio) && (g_Radiotext))
		{
			// stop radiotext PES decoding before zapping
			g_Radiotext->radiotext_stop();
		}
#endif		
		
		tuned = pos;
		g_RemoteControl->zapTo_ChannelID(chan->channel_id, chan->name, !chan->bAlwaysLocked); // UTF-8
		
		// TODO check is it possible bouquetList is NULL ?
		if (bouquetList != NULL) 
		{
			CNeutrinoApp::getInstance()->channelList->adjustToChannelID(chan->channel_id);
		}
		
		if(new_mode_active)
			selected = pos;
	}

	if(!new_mode_active) 
	{
		selected = pos;
#if 0
		/* TODO lastChList.store also called in adjustToChannelID, which is called
		   only from "whole" channel list. Why here too ? */
		lastChList.store (selected, chan->channel_id, forceStoreToLastChannels);
#endif
		/* remove recordModeActive from infobar */
		if(g_settings.auto_timeshift && !CNeutrinoApp::getInstance()->recordingstatus) 
		{
			g_InfoViewer->handleMsg(NeutrinoMessages::EVT_RECORDMODE, 0);
		}

		g_RCInput->postMsg( NeutrinoMessages::SHOW_INFOBAR, 0 );
	}
}

// -1: channellist not found
int CChannelList::numericZap(int key)
{
	neutrino_msg_t      msg;
	neutrino_msg_data_t data;

	int res = -1;

	if (chanlist.empty()) 
	{
		DisplayErrorMessage(g_Locale->getText(LOCALE_CHANNELLIST_NONEFOUND)); // UTF-8
		return res;
	}

	// -- quickzap "0" (recall) to last seen channel...
	if (key == g_settings.key_lastchannel) 
	{
		t_channel_id channel_id = lastChList.getlast(1);
		if(channel_id) 
		{
			lastChList.clear_storedelay (); // ignore store delay
			zapTo_ChannelID(channel_id);
		}
		
		return res;
	}

	if (key == g_settings.key_zaphistory) 
	{
		// recording status
		if(!autoshift && CNeutrinoApp::getInstance()->recordingstatus) 
		{
			CChannelList * orgList = bouquetList->orgChannelList;
			CChannelList * channelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_CURRENT_TP), false, true);
			
			t_channel_id recid = rec_channel_id >> 16;
			
			// get channels from the same tp as recording channel
			for ( unsigned int i = 0 ; i < orgList->chanlist.size(); i++) 
			{
				if((orgList->chanlist[i]->channel_id >> 16) == recid) 
				{
					channelList->addChannel(orgList->chanlist[i]);
				}
			}

			if (channelList->getSize() != 0) 
			{
				channelList->adjustToChannelID(orgList->getActiveChannel_ChannelID(), false);
				
				this->frameBuffer->paintBackground();
				
#if !defined USE_OPENGL
				this->frameBuffer->blit();
#endif
				res = channelList->exec();
			}
			delete channelList;
			return res;
		}
		
		// -- zap history bouquet, similar to "0" quickzap, but shows a menue of last channels
		if (this->lastChList.size() > 1) 
		{
			CChannelList * channelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_HISTORY), true, true);

			for(unsigned int i = 1 ; i < this->lastChList.size() ; ++i) 
			{
				t_channel_id channel_id = this->lastChList.getlast(i);

				if(channel_id) 
				{
					CZapitChannel * channel = getChannel(channel_id);
					if(channel) 
						channelList->addChannel(channel);
				}
			}

			if (channelList->getSize() != 0) 
			{
				this->frameBuffer->paintBackground();

#if !defined USE_OPENGL
				this->frameBuffer->blit();
#endif
				res = channelList->exec();
			}
			delete channelList;
		}
		return res;
	}
	
	//TEST: PiP
	if( (key == g_settings.key_pip) || (key == g_settings.key_pip_subchannel) )
	{
		CChannelList * orgList = bouquetList->orgChannelList;
		CChannelList * channelList = new CChannelList(g_Locale->getText(LOCALE_CHANNELLIST_CURRENT_TP), false, true);
			
		t_channel_id pipid = live_channel_id >> 16;
			
		for ( unsigned int i = 0 ; i < orgList->chanlist.size(); i++) 
		{
			if((orgList->chanlist[i]->channel_id >> 16) == pipid) 
			{
					channelList->addChannel(orgList->chanlist[i]);
			}
		}
			
		pip_selected = true;

		if (channelList->getSize() != 0) 
		{
			channelList->adjustToChannelID(orgList->getActiveChannel_ChannelID(), false);
			this->frameBuffer->paintBackground();

#if !defined USE_OPENGL
			this->frameBuffer->blit();
#endif
			res = channelList->exec();
		}
		delete channelList;
		return res;
	}
	//

	int sx = 4 * g_Font[SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP]->getRenderWidth(widest_number) + 14;
	int sy = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP]->getHeight() + 6;

	int ox = frameBuffer->getScreenX() + (frameBuffer->getScreenWidth() - sx)/2;
	int oy = frameBuffer->getScreenY() + (frameBuffer->getScreenHeight() - sy)/2;
	char valstr[10];
	int chn = CRCInput::getNumericValue(key);
	int pos = 1;
	int lastchan= -1;
	bool doZap = true;
	bool showEPG = false;

	while(1) 
	{
		if (lastchan != chn) 
		{
			sprintf((char*) &valstr, "%d", chn);
			
			while(strlen(valstr)<4)
				strcat(valstr,"-");   //"_"

			frameBuffer->paintBoxRel(ox, oy, sx, sy, COL_INFOBAR_PLUS_0);

			for (int i = 3; i >= 0; i--) 
			{
				valstr[i+ 1]= 0;
				g_Font[SNeutrinoSettings::FONT_TYPE_CHANNEL_NUM_ZAP]->RenderString(ox+7+ i*((sx-14)>>2), oy+sy-3, sx, &valstr[i], COL_INFOBAR);
			}
			
#if !defined USE_OPENGL
			frameBuffer->blit();
#endif

			// show infobar
			showInfo(chn - 1);
			lastchan = chn;
		}

		g_RCInput->getMsg( &msg, &data, g_settings.timing[SNeutrinoSettings::TIMING_NUMERICZAP] * 10 );

		if ( msg == CRCInput::RC_timeout ) 
		{
			if ( ( chn > (int)chanlist.size() ) || (chn == 0) )
				chn = tuned + 1;
			break;
		}
		else if (CRCInput::isNumeric(msg)) 
		{
			if (pos == 4) 
			{
				chn = 0;
				pos = 1;
			} 
			else 
			{
				chn *= 10;
				pos++;
			}
			chn += CRCInput::getNumericValue(msg);
		}
		else if ( msg == CRCInput::RC_ok ) 
		{
			if ( ( chn > (signed int) chanlist.size() ) || ( chn == 0 ) ) 
			{
				chn = tuned + 1;
			}
			break;
		}
		else if ( msg == (neutrino_msg_t)g_settings.key_quickzap_down ) 
		{
			if ( chn == 1 )
				chn = chanlist.size();
			else {
				chn--;

				if (chn > (int)chanlist.size())
					chn = (int)chanlist.size();
			}
		}
		else if ( msg == (neutrino_msg_t)g_settings.key_quickzap_up ) 
		{
			chn++;

			if (chn > (int)chanlist.size())
				chn = 1;
		}
		else if ( ( msg == CRCInput::RC_home ) || ( msg == CRCInput::RC_left ) || ( msg == CRCInput::RC_right) )
		{
			doZap = false;
			break;
		}
		else if ( msg == CRCInput::RC_red ) 
		{
			if ( ( chn <= (signed int) chanlist.size() ) && ( chn != 0 ) ) 
			{
				doZap = false;
				showEPG = true;
				break;
			}
		}
		else if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all ) 
		{
			doZap = false;
			break;
		}
	}

	frameBuffer->paintBackgroundBoxRel(ox, oy, sx, sy);
	
#if !defined USE_OPENGL
	frameBuffer->blit();
#endif

	chn--;
	
	if (chn < 0)
		chn = 0;
	
	if ( doZap ) 
	{
		// kill infobar
		if(g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0)
			g_InfoViewer->killTitle();
		
		// zapto selected channel
		zapTo( chn );
	} 
	else 
	{
		// show infobar
		showInfo(tuned);
		
		// kill infobar
		g_InfoViewer->killTitle();

		if ( showEPG )
			g_EventList->exec(chanlist[chn]->channel_id, chanlist[chn]->name);
	}
	
	return res;
}

void CChannelList::virtual_zap_mode(bool up)
{
        neutrino_msg_t      msg;
        neutrino_msg_data_t data;

        if (chanlist.empty()) 
	{
                DisplayErrorMessage(g_Locale->getText(LOCALE_CHANNELLIST_NONEFOUND)); // UTF-8
                return;
        }

        int chn = getActiveChannelNumber() + (up ? 1 : -1);
        if (chn > (int)chanlist.size())
		chn = 1;
	
        if (chn == 0)
		chn = (int)chanlist.size();
	
        int lastchan = -1;
        bool doZap = true;
        bool showEPG = false;
	int epgpos = 0;

        while(1)
        {
                if (lastchan != chn || (epgpos != 0))
                {
                        showInfo(chn - 1, epgpos);
                        lastchan = chn;
                }
		epgpos = 0;
                g_RCInput->getMsg( &msg, &data, 15*10 ); // 15 seconds, not user changable
		
                //printf("%u %u %u\n", msg, NeutrinoMessages::EVT_TIMER, CRCInput::RC_timeout);

                if ( msg == CRCInput::RC_ok )
                {
                        if ( ( chn > (signed int) chanlist.size() ) || ( chn == 0 ) )
                        {
                                chn = tuned + 1;
                        }
                        break;
                }
                else if ( msg == CRCInput::RC_left )
                {
                        if ( chn == 1 )
                                chn = chanlist.size();
                        else
                        {
                                chn--;

                                if (chn > (int)chanlist.size())
                                        chn = (int)chanlist.size();
                        }
                }
                else if ( msg == CRCInput::RC_right )
                {
                        chn++;

                        if (chn > (int)chanlist.size())
                                chn = 1;
                }
                else if ( msg == CRCInput::RC_up )
                {
                        epgpos = -1;
                }
                else if ( msg == CRCInput::RC_down )
                {
                        epgpos = 1;
                }
                else if ( ( msg == CRCInput::RC_home ) || ( msg == CRCInput::RC_timeout ) )
                {
                        // Abbruch ohne Channel zu wechseln
                        doZap = false;
                        break;
                }
                else if ( msg == CRCInput::RC_red )
                {
                        // Rote Taste zeigt EPG fuer gewaehlten Kanal an
                        if ( ( chn <= (signed int) chanlist.size() ) && ( chn != 0 ) )
                        {
                                doZap = false;
                                showEPG = true;
                                break;
                        }
                }
                else if ( CNeutrinoApp::getInstance()->handleMsg( msg, data ) & messages_return::cancel_all )
                {
                        doZap = false;
                        break;
                }
        }
        
	g_InfoViewer->clearVirtualZapMode();

        chn--;
        if (chn<0)
                chn=0;
        if ( doZap )
        {
		if(g_settings.timing[SNeutrinoSettings::TIMING_INFOBAR] == 0)
			g_InfoViewer->killTitle();
                zapTo( chn );
        }
        else
        {
                showInfo(tuned);
                g_InfoViewer->killTitle();

                // Rote Taste zeigt EPG fuer gewaehlten Kanal an
                if ( showEPG )
                        g_EventList->exec(chanlist[chn]->channel_id, chanlist[chn]->name);
        }
}

void CChannelList::quickZap(int key, bool cycle)
{
        if(chanlist.size() == 0)
                return;

	if ( (key == g_settings.key_quickzap_down) || (key == CRCInput::RC_left) )
	{
                if(selected == 0)
                        selected = chanlist.size() - 1;
                else
                        selected--;
        }
	else if ((key == g_settings.key_quickzap_up) || (key == CRCInput::RC_right) )
	{
                selected = (selected+1)%chanlist.size();
        }

	printf("[neutrino] quick zap selected = %d getActiveBouquetNumber %d\n", selected, bouquetList->getActiveBouquetNumber());

	if(cycle)
		bouquetList->orgChannelList->zapTo(bouquetList->Bouquets[bouquetList->getActiveBouquetNumber()]->channelList->getKey(selected)-1);
	else
        	zapTo( selected );

	g_RCInput->clearRCMsg(); //FIXME test for n.103
}

void CChannelList::paintDetails(int index)
{
	CChannelEvent * p_event;

	if (displayNext) 
	{
		p_event = &chanlist[index]->nextEvent;
	} 
	else 
	{
		p_event = &chanlist[index]->currentEvent;
	}

	// itembox refresh
	frameBuffer->paintBoxRel(x + 2, y + height + 2, width - 4, info_height - 4, COL_MENUCONTENTDARK_PLUS_0);

	if (!p_event->description.empty()) 
	{
		char cNoch[50]; // UTF-8
		char cSeit[50]; // UTF-8

		struct tm * pStartZeit = localtime(&p_event->startTime);
		unsigned seit = ( time(NULL) - p_event->startTime ) / 60;

		if (displayNext) 
		{
			sprintf(cNoch, "(%d min)", p_event->duration / 60);
			sprintf(cSeit, g_Locale->getText(LOCALE_CHANNELLIST_START), pStartZeit->tm_hour, pStartZeit->tm_min);
		} 
		else 
		{
			sprintf(cSeit, g_Locale->getText(LOCALE_CHANNELLIST_SINCE), pStartZeit->tm_hour, pStartZeit->tm_min);
			int noch = (p_event->startTime + p_event->duration - time(NULL)) / 60;
			if ((noch < 0) || (noch >= 10000))
				noch = 0;
			sprintf(cNoch, "(%d / %d min)", seit, noch);
		}
		
		int seit_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(cSeit, true); // UTF-8
		int noch_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth(cNoch, true); // UTF-8

		std::string text1 = p_event->description;
		std::string text2 = p_event->text;

		int xstart = 10;
		if (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text1, true) > (width - 30 - seit_len) )
		{
			// zu breit, Umbruch versuchen...
			int pos;
			do 
			{
				pos = text1.find_last_of("[ -.]+");
				if ( pos!=-1 )
					text1 = text1.substr( 0, pos );
			} while ( ( pos != -1 ) && (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text1, true) > (width - 30 - seit_len) ) );

			std::string text3 = p_event->description.substr(text1.length()+ 1);

			if (!(text2.empty()))
				text3= text3+ " - ";

			xstart += g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text3, true);
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + 10, y + height + 5 + 2*fheight, width - 30 - noch_len, text3, COL_MENUCONTENTDARK, 0, true);
		}

		if (!(text2.empty())) 
		{
			while ( text2.find_first_of("[ -.+*#?=!$%&/]+") == 0 )
				text2 = text2.substr( 1 );

			text2 = text2.substr( 0, text2.find('\n') );
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x + xstart, y + height + 5 + 2*fheight, width - xstart- 20- noch_len, text2, COL_MENUCONTENTDARK, 0, true);
		}

		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + 10, y + height + 5 + fheight, width - 30 - seit_len, text1, COL_MENUCONTENTDARK, 0, true);
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString (x + width - 10 - seit_len, y+ height + 5 + fheight   , seit_len, cSeit, COL_MENUCONTENTDARK, 0, true); // UTF-8
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(x + width - 10 - noch_len, y+ height + 5 + 2*fheight - 2, noch_len, cNoch, COL_MENUCONTENTDARK, 0, true); // UTF-8
	}
}

void CChannelList::clearItem2DetailsLine ()
{  
	  paintItem2DetailsLine(-1, 0);  
}

void CChannelList::paintItem2DetailsLine(int pos, int ch_index)
{
#define ConnectLineBox_Width	16

	int xpos  = x - ConnectLineBox_Width;
	int ypos1 = y + theight+0 + pos*fheight;
	int ypos2 = y + height;
	int ypos1a = ypos1 + (fheight/2)-2;
	int ypos2a = ypos2 + (info_height/2)-2;
	fb_pixel_t col1 = COL_MENUCONTENT_PLUS_6;
	fb_pixel_t col2 = COL_MENUCONTENT_PLUS_1;

	// Clear
	frameBuffer->paintBackgroundBoxRel(xpos - 10, y - 10, ConnectLineBox_Width + 10, height+info_height + 10);

#if !defined USE_OPENGL
	frameBuffer->blit();
#endif

	// paint Line if detail info (and not valid list pos)
	if (pos >= 0) 
	{ 
		int fh = fheight > 10 ? fheight - 10: 5;
			
		frameBuffer->paintBoxRel(xpos + ConnectLineBox_Width - 4, ypos1 + 5, 4, fh, col1);
		frameBuffer->paintBoxRel(xpos + ConnectLineBox_Width - 4, ypos1 + 5, 1, fh, col2);			

		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width - 4, ypos2 + 7, 4, info_height - 14, col1);
		frameBuffer->paintBoxRel(xpos+ConnectLineBox_Width - 4, ypos2 + 7, 1, info_height - 14, col2);			

		// vertical line
		frameBuffer->paintBoxRel(xpos + ConnectLineBox_Width - 16, ypos1a, 4, ypos2a - ypos1a, col1);
		frameBuffer->paintBoxRel(xpos + ConnectLineBox_Width - 16, ypos1a, 1, ypos2a - ypos1a + 4, col2);		

		// Hline Oben
		frameBuffer->paintBoxRel(xpos + ConnectLineBox_Width - 15, ypos1a, 12,4, col1);
		frameBuffer->paintBoxRel(xpos + ConnectLineBox_Width - 16, ypos1a, 12,1, col2);
		
		// Hline Unten
		frameBuffer->paintBoxRel(xpos + ConnectLineBox_Width - 15, ypos2a, 12, 4, col1);
		frameBuffer->paintBoxRel(xpos + ConnectLineBox_Width - 12, ypos2a, 8, 1, col2);

		// untere info box lines
		frameBuffer->paintBoxRel(x, ypos2, width, info_height, col1);
	}
}

bool CChannelList::canZap(CZapitChannel * channel)
{
	bool iscurrent = true;

	if(CNeutrinoApp::getInstance()->recordingstatus) 
	{
		if(channel == NULL)
			channel = chanlist[selected];
		
		iscurrent = feCanTune(channel);
	}
	
	return iscurrent;
}

void CChannelList::paintItem(int pos)
{
	int ypos = y + theight + 0 + pos*fheight;
	uint8_t    color;
	fb_pixel_t bgcolor;
	bool iscurrent = true;
	unsigned int curr = liststart + pos;


	if(curr < chanlist.size())
		iscurrent = canZap(chanlist[curr]);
	
	if (curr == selected) 
	{
		color   = COL_MENUCONTENTSELECTED;
		bgcolor = COL_MENUCONTENTSELECTED_PLUS_0;
		
		// itemlines	
		paintItem2DetailsLine(pos, curr);		
		
		// details
		paintDetails(curr);

		// refresh logo box
		frameBuffer->paintBoxRel(x + width - 90 - PIC_W, y, PIC_W, theight, COL_MENUHEAD_PLUS_0);
	
		// paint logo
		g_PicViewer->DisplayLogo(chanlist[selected]->channel_id, x + width - 90 - PIC_W, y, PIC_W, theight);

		// infobox
		frameBuffer->paintBoxRel(x, ypos, width- 15, fheight, bgcolor);
	} 
	else 
	{
		color = iscurrent ? COL_MENUCONTENT : COL_MENUCONTENTINACTIVE;
		bgcolor = iscurrent ? COL_MENUCONTENT_PLUS_0 : COL_MENUCONTENTINACTIVE_PLUS_0;
		
		// infobox
		frameBuffer->paintBoxRel(x, ypos, width - 15, fheight, bgcolor);
	}

	//name and description
	if(curr < chanlist.size()) 
	{
		char nameAndDescription[255];
		char tmp[10];
		CZapitChannel * chan = chanlist[curr];
		int prg_offset=0;
		int title_offset=0;
		uint8_t tcolor = (liststart + pos == selected) ? color : COL_MENUCONTENTINACTIVE;
		int xtheight = fheight-2;
		
		// due to extended info
		prg_offset = 42;
		title_offset = 6;

		sprintf((char*) tmp, "%d", this->historyMode ? pos : chan->number);

		CChannelEvent * p_event = NULL;

		if (displayNext) 
		{
			p_event = &chan->nextEvent;
		} 
		else 
		{
			p_event = &chan->currentEvent;
		}

		// hd/scrambled icons
		// setup icon
		int icon_hd_w = 0;
		int icon_hd_h = 0;
		int icon_ca_w = 0;
		int icon_ca_h = 0;
		
		if (g_settings.channellist_ca)
		{
			// scrambled icon
			if(chan->scrambled) 
			{
				frameBuffer->getIconSize(NEUTRINO_ICON_SCRAMBLED2, &icon_ca_w, &icon_ca_h);
				frameBuffer->paintIcon(NEUTRINO_ICON_SCRAMBLED2, x + width - SCROLLBAR_WIDTH - 2 - icon_ca_w, ypos + (fheight - 16)/2);
			}
			
			// hd icon
			if(chan->isHD() ) 
			{
				frameBuffer->getIconSize(NEUTRINO_ICON_RESOLUTION_HD, &icon_hd_w, &icon_hd_h);
				frameBuffer->paintIcon(NEUTRINO_ICON_RESOLUTION_HD, x + width - SCROLLBAR_WIDTH - 2 - icon_ca_w - 2 - icon_hd_w, ypos + (fheight - 16)/2);
			}
		}

		int numpos = x + 5 + numwidth - g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth(tmp);
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(numpos, ypos + fheight, numwidth + 5, tmp, color, fheight);

		int l = 0;
		if (this->historyMode)
			l = snprintf(nameAndDescription, sizeof(nameAndDescription), ": %d %s", chan->number, chan->name.c_str());
		else
			l = snprintf(nameAndDescription, sizeof(nameAndDescription), "%s", chan->name.c_str());

		if (!(p_event->description.empty())) 
		{
			snprintf(nameAndDescription + l, sizeof(nameAndDescription)-l," - ");
			unsigned int ch_name_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(nameAndDescription, true);
			unsigned int ch_desc_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(p_event->description, true);

			if ( (width - numwidth - 60 - SCROLLBAR_WIDTH - prg_offset - ch_name_len - icon_ca_w - icon_hd_w - 4) < ch_desc_len ) //60:???
				ch_desc_len = (width - numwidth - 60 - SCROLLBAR_WIDTH - icon_ca_w - icon_hd_w - 4 - ch_name_len - prg_offset); //30: hd icon width
			if (ch_desc_len < 0)
				ch_desc_len = 0;
			
			// next infos
			if(displayNext)
			{
				struct tm *pStartZeit = localtime(&p_event->startTime);
			
				sprintf((char*) tmp, "%02d:%02d", pStartZeit->tm_hour, pStartZeit->tm_min);
				
				g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(x + 5 + numwidth+ 6, ypos + xtheight, width - numwidth - 20 - 15 - prg_offset, tmp, tcolor, 0, true);
			}
			else
			{
				time_t jetzt = time(NULL);
				int runningPercent = 0;
					
				if (((jetzt - p_event->startTime + 30) / 60) < 0 )
				{
					runningPercent = 0;
				}
				else
				{
					runningPercent = (jetzt-p_event->startTime) * 30 / p_event->duration;
					if (runningPercent > 30)	// this would lead to negative value in paintBoxRel
						runningPercent = 30;	// later on which can be fatal...
				}
				
				frameBuffer->paintBoxRel(x + 5 + numwidth + title_offset, ypos + fheight/4, 34, fheight/2, COL_MENUCONTENT_PLUS_3, 0);//fill passive
				frameBuffer->paintBoxRel(x + 5 + numwidth + title_offset + 2, ypos + 2 + fheight/4, 30, fheight/2 - 4, COL_MENUCONTENT_PLUS_1, 0);//frame(passive)
				
				frameBuffer->paintBoxRel(x + 5 + numwidth + title_offset + 2, ypos + 2 + fheight/4, runningPercent, fheight/2 - 4, COL_MENUCONTENT_PLUS_3, 0);//fill(active)
			}

			// name and description
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x + 5 + numwidth + 10 + prg_offset, ypos + fheight, width - numwidth - 40 - 15 - prg_offset, nameAndDescription, color, 0, true);

			// epg-txt (left)
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString(x + 5 + numwidth + 10 + ch_name_len + 5 + prg_offset, ypos + fheight, ch_desc_len, p_event->description, (curr == selected)?COL_MENUCONTENTSELECTED:(!displayNext ? COL_COLORED_EVENTS_CHANNELLIST : COL_MENUCONTENTINACTIVE) , 0, true);
		}
		else 
		{
			// extended info
			short runningPercent = 0;
			frameBuffer->paintBoxRel(x + 5 + numwidth + title_offset, ypos + fheight/4, 34, fheight/2, COL_MENUCONTENT_PLUS_3, 0);//fill passive
			frameBuffer->paintBoxRel(x + 5 + numwidth + title_offset + 2, ypos + 2 + fheight/4, 30, fheight/2 - 4, COL_MENUCONTENT_PLUS_1, 0);//frame(passive)

			frameBuffer->paintBoxRel(x + 5 + numwidth + title_offset + 2, ypos + 2 + fheight/4, runningPercent, fheight/2 - 4, COL_MENUCONTENT_PLUS_3, 0);//fill(active)
				
			frameBuffer->paintLine(x + 5 + numwidth + title_offset, ypos + fheight/4 + 1, x + 5 + numwidth + title_offset + 32, ypos + fheight/4 + fheight/2 - 3, COL_MENUCONTENT_PLUS_3);
			
			//name
			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(x+ 5+ numwidth+ 10+prg_offset, ypos+ fheight, width- numwidth- 40- 15-prg_offset, nameAndDescription, color, 0, true); // UTF-8
		}

		// show channame/event info in vfd
		if (curr == selected) 
		{
			if (!(chan->currentEvent.description.empty())) 
			{
				snprintf(nameAndDescription, sizeof(nameAndDescription), "%s - %s", chan->name.c_str(), p_event->description.c_str());				
			} 
			else
			{				
				snprintf(nameAndDescription, sizeof(nameAndDescription), "%s", chan->name.c_str() );
			}
			
			if (!CVFD::getInstance()->is4digits)
				CVFD::getInstance()->ShowText(nameAndDescription); // UTF-8
		}
	}
}

#define NUM_LIST_BUTTONS 4
struct button_label CChannelListButtons[NUM_LIST_BUTTONS] =
{
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_INFOVIEWER_EVENTLIST},
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_INFOVIEWER_NEXT},
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_BOUQUETLIST_HEAD},
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_EPGMENU_EPGPLUS },
};

#define NUM_VLIST_BUTTONS 4
const struct button_label CChannelVListButtons[NUM_VLIST_BUTTONS] =
{
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_INFOVIEWER_EVENTLIST },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_INFOVIEWER_NEXT },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_BOUQUETLIST_HEAD },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_EPGMENU_EPGPLUS }
};

void CChannelList::paintHead()
{
	// head
	frameBuffer->paintBoxRel(x, y, width, theight, COL_MENUHEAD_PLUS_0, RADIUS_MID, CORNER_TOP);//round
	
	int ButtonWidth = (width - 20) / 4;

	// foot
	if (displayNext) 
	{
		CChannelListButtons[1].locale = LOCALE_INFOVIEWER_NOW;
	} 
	else 
	{
		CChannelListButtons[1].locale = LOCALE_INFOVIEWER_NEXT;
	}

	// foot
	frameBuffer->paintBoxRel(x, y + (height - buttonHeight), width, buttonHeight - 1, COL_MENUHEAD_PLUS_0, RADIUS_MID, CORNER_BOTTOM); //round
	
	// foot buttons
	::paintButtons(frameBuffer, g_Font[SNeutrinoSettings::FONT_TYPE_INFOBAR_SMALL], g_Locale, x + 10, y + (height - buttonHeight) + 3, ButtonWidth, vlist ? NUM_VLIST_BUTTONS : NUM_LIST_BUTTONS, vlist ? CChannelVListButtons : CChannelListButtons);

	// help icon
	int icon_h_w, icon_h_h;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_HELP, &icon_h_w, &icon_h_h);
	frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_HELP, x + width - 10 - icon_h_w , y + 5 );

	// setup icon
	int icon_s_w, icon_s_h;
	if (bouquetList != NULL)
	{
		frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_DBOX, &icon_s_w, &icon_s_h);
		frameBuffer->paintIcon(NEUTRINO_ICON_BUTTON_DBOX, x + width - 10 - icon_h_w - 2 - icon_s_w, y + 5); // icon for bouquet list button
	}

	// mute zap
	int icon_z_w, icon_z_h;
	frameBuffer->getIconSize(NEUTRINO_ICON_BUTTON_MUTE_ZAP_ACTIVE, &icon_z_w, &icon_z_h);
	frameBuffer->paintIcon(new_mode_active ? NEUTRINO_ICON_BUTTON_MUTE_ZAP_ACTIVE : NEUTRINO_ICON_BUTTON_MUTE_ZAP_INACTIVE, x + width - 10 - icon_h_w - 2 - icon_s_w - 2 - icon_z_w, y + 5);
	
	// paint time/date
	int timestr_len = 0;
	char timestr[18];
	
	time_t now = time(NULL);
	struct tm * tm = localtime(&now);
	
	bool gotTime = g_Sectionsd->getIsTimeSet();

	if(gotTime)
	{
		strftime(timestr, 18, "%d.%m.%Y %H:%M", tm);
		timestr_len = g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->getRenderWidth(timestr, true); // UTF-8
		
		g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->RenderString(x + width - 10 - icon_h_w - 2 - icon_s_w - 2 - icon_z_w - 2 - PIC_W - 5 - timestr_len, y + g_Font[SNeutrinoSettings::FONT_TYPE_EVENTLIST_ITEMLARGE]->getHeight() + 5, timestr_len+1, timestr, COL_MENUHEAD, 0, true); // UTF-8 // 100 is pic_w refresh box
	}
	
	//title
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(x + 10, y + theight, width - 10 - icon_h_w - 2 - icon_s_w - 2 - icon_z_w - 2 - PIC_W - 5 - timestr_len, name, COL_MENUHEAD, 0, true); // UTF-8
}

void CChannelList::paint()
{
	liststart = (selected/listmaxshow)*listmaxshow;
	
	//FIXME do we need to find biggest chan number in list ?
	numwidth = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth("0000");
	
	// channellist boby
	frameBuffer->paintBoxRel(x, y + theight, width, height - buttonHeight - theight, COL_MENUCONTENT_PLUS_0);
	
	// paint pig
	if(g_settings.mini_tv)
	{
		frameBuffer->paintBackgroundBoxRel(835, y + theight + 5, 390, 225);	
		videoDecoder->Pig(835, y + theight + 5, 390, 225);
	}

	// paint item
	for(unsigned int count = 0; count < listmaxshow; count++) 
	{
		paintItem(count);
	}

	// scroll bar
	int ypos = y+ theight;
	int sb = fheight* listmaxshow;
	
	frameBuffer->paintBoxRel(x + width - 15, ypos, 15, sb,  COL_MENUCONTENT_PLUS_1);

	int sbc = ((chanlist.size()- 1)/ listmaxshow)+ 1;
	int sbs = (selected/listmaxshow);

	frameBuffer->paintBoxRel(x + width - 13, ypos + 2 + sbs*(sb-4)/sbc, 11, (sb - 4)/sbc, COL_MENUCONTENT_PLUS_3);
}

int CChannelList::getSize() const
{
	return this->chanlist.size();
}

int CChannelList::getSelectedChannelIndex() const
{
	return this->selected;
}

void CChannelList::paintMiniTV()
{
	// head for minitv
	frameBuffer->paintBoxRel(830, y, 400, theight, COL_MENUHEAD_PLUS_0, RADIUS_MID, CORNER_TOP );//round
	
	// paint selected channel name (only when channellist exists)
	int channelname_len = 0;
	std::string liveChanName = g_Zapit->getChannelName(live_channel_id);
	channelname_len = g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->getRenderWidth((char *)liveChanName.c_str(), true); // UTF-8
	
	// pig body
	frameBuffer->paintBoxRel(830, y + theight, 400, 225 + theight/2, COL_MENUCONTENT_PLUS_0, RADIUS_MID, CORNER_BOTTOM);
	
	//info head
	frameBuffer->paintBoxRel(830, y + theight + 255 + theight/2 + 5, 400, theight, COL_MENUCONTENTDARK_PLUS_0);
	
	// info body
	frameBuffer->paintBoxRel(830, y + theight + 255 + theight/2 + 5 + theight, 400, 660 - (y + theight + 255 + theight/2 + 5 + theight) - theight + info_height, COL_MENUCONTENT_PLUS_0);
	
	// live channel name
	g_Font[SNeutrinoSettings::FONT_TYPE_MENU_TITLE]->RenderString(830 + 10, y + theight, (channelname_len > 390? 390 : channelname_len), liveChanName.c_str(), COL_MENUHEAD, 0, true); // UTF-8
		
	// FIXME: paint logo only when we have enough place ???
	if( 390 - channelname_len >= PIC_W)
		g_PicViewer->DisplayLogo(live_channel_id, 830 + 5 + channelname_len + 5, y, PIC_W, theight);
		
	// event description
	CChannelEvent * p_event = NULL;
	if(p_event == NULL)
		p_event = &live_channel->currentEvent;;
		
	if (!p_event->description.empty()) 
	{
		char cNoch[50]; // UTF-8
		char cSeit[50]; // UTF-8

		struct tm * pStartZeit = localtime(&p_event->startTime);
		unsigned seit = ( time(NULL) - p_event->startTime ) / 60;

		sprintf(cSeit, g_Locale->getText(LOCALE_CHANNELLIST_SINCE), pStartZeit->tm_hour, pStartZeit->tm_min);
		int noch = (p_event->startTime + p_event->duration - time(NULL)) / 60;
		if ((noch < 0) || (noch >= 10000))
			noch = 0;
		sprintf(cNoch, "(%d / %d min)", seit, noch);
			
		int seit_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->getRenderWidth(cSeit, true); // UTF-8
		int noch_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->getRenderWidth(cNoch, true); // UTF-8

		std::string text1 = p_event->description;
		std::string text2 = p_event->text;
		
		int descr_len = g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth((char *)text1.c_str(), true); // UTF-8

		// description
		std::string text3 = "";
		
		if (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text1, true) > 390)
		{
			// zu breit, Umbruch versuchen...
			int pos;
			do 
			{
				pos = text1.find_last_of("[ -.]+");
			
				if ( pos != -1 )
					text1 = text1.substr( 0, pos );
			} while ( ( pos != -1 ) && (g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->getRenderWidth(text1, true) > 390) );

			text3 = p_event->description.substr(text1.length() + 1);

			g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(830 + 5, y + theight + 255 + theight/2 + 5 +theight + /*2**/fheight, 390, text3, COL_MENUCONTENTDARK, 0, true);
		}
		
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST]->RenderString(830 + 5, y + theight + 255 + theight/2 + 5 + theight, (descr_len > 390? 390 : descr_len), text1, COL_MENUCONTENTDARK, 0, true);
			
		// since
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_DESCR]->RenderString (830 + 5, y + theight + 255 + theight/2 + 5 + theight + (text3.empty()? fheight : 2*fheight)/*fheight*/, seit_len, cSeit, COL_MENUCONTENTDARK, 0, true); // UTF-8
			
		// rest/duration
		g_Font[SNeutrinoSettings::FONT_TYPE_CHANNELLIST_NUMBER]->RenderString(830 + 390 - 5 - noch_len, y + theight + 255 + theight/2 + 5 + theight + (text3.empty()? fheight : 2*fheight)/*fheight*/, noch_len, cNoch, COL_MENUCONTENTDARK, 0, true); // UTF-8
		
		// text
		epgText.clear();
		emptyLineCount = 0;
				
		if (!(text2.empty())) 
		{
			processTextToArray(text2);
					
			// recalculate
			medlineheight = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->getHeight();
			medlinecount = (660 - (y + theight + 255 + theight/2 + 5 + theight) - theight + info_height -fheight)/medlineheight;

			int textCount = epgText.size();
			int ypos = y + theight + 255 + theight/2 + 5 +theight + (text3.empty()? 2*fheight : 3*fheight);

			for(int i = 0; i < textCount && i < medlinecount; i++, ypos += medlineheight)
			{
				if ( i < epgText.size() )
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO1]->RenderString(835, ypos + medlineheight, 390, epgText[i], COL_MENUCONTENTDARK, 0, true); // UTF-8
				else
					g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->RenderString(835, ypos + medlineheight, 390, epgText[i], COL_MENUCONTENTDARK, 0, true); // UTF-8
			}
		}
	}
}

void CChannelList::addTextToArray(const std::string & text) // UTF-8
{
	//printf("line: >%s<\n", text.c_str() );
	
	if (text==" ")
	{
		emptyLineCount ++;
		
		if(emptyLineCount < 2)
		{
			epgText.push_back(text);
		}
	}
	else
	{
		emptyLineCount = 0;
		epgText.push_back(text);
	}
}

void CChannelList::processTextToArray(std::string text) // UTF-8
{
	std::string	aktLine = "";
	std::string	aktWord = "";
	int	aktWidth = 0;
	text += ' ';
	char * text_= (char *) text.c_str();

	while(*text_ != 0)
	{
		if ( (*text_ == ' ') || (*text_ == '\n') || (*text_ == '-') || (*text_== '.') )
		{
			// Houdini: if there is a newline (especially in the Premiere Portal EPGs) do not forget to add aktWord to aktLine 
			// after width check, if width check failes do newline, add aktWord to next line 
			// and "reinsert" i.e. reloop for the \n
			if(*text_!='\n')
				aktWord += *text_;

			// check the wordwidth - add to this line if size ok
			int aktWordWidth = g_Font[SNeutrinoSettings::FONT_TYPE_EPG_INFO2]->getRenderWidth(aktWord, true);
			if((aktWordWidth+aktWidth) < (390))
			{
				//space ok, add
				aktWidth += aktWordWidth;
				aktLine += aktWord;
			
				if(*text_=='\n')
				{	//enter-handler
					addTextToArray( aktLine );
					aktLine = "";
					aktWidth= 0;
				}	
				aktWord = "";
			}
			else
			{
				//new line needed
				addTextToArray( aktLine );
				aktLine = aktWord;
				aktWidth = aktWordWidth;
				aktWord = "";
				// Houdini: in this case where we skipped \n and space is too low, exec newline and rescan \n 
				// otherwise next word comes direct after aktLine
				if(*text_=='\n')
					continue;
			}
		}
		else
		{
			aktWord += *text_;
		}
		text_++;
	}
	//add the rest
	addTextToArray( aktLine + aktWord );
}


