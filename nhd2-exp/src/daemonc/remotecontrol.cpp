/*
	Neutrino-GUI  -   DBoxII-Project
	
	$Id: remotecontrol.cpp 2013/10/12 mohousch Exp $

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

#include <daemonc/remotecontrol.h>

#include <global.h>
#include <neutrino.h>
#include <gui/infoviewer.h>
#include <driver/vcrcontrol.h>

#include <driver/encoding.h>
#include <system/debug.h>
#include <system/helpers.h>

// zapit includes
#include <bouquets.h>


extern tallchans allchans;	// defined in bouquets.h

//FIXME: auto-timeshift
extern bool autoshift;
extern uint32_t scrambled_timer;

bool sectionsd_getComponentTagsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::ComponentTagList& tags);
bool sectionsd_getLinkageDescriptorsUniqueKey(const event_id_t uniqueKey, CSectionsdClient::LinkageDescriptorList& descriptors);
bool sectionsd_getNVODTimesServiceKey(const t_channel_id uniqueServiceKey, CSectionsdClient::NVODTimesList& nvod_list);
void sectionsd_setPrivatePid(unsigned short pid);
void sectionsd_getCurrentNextServiceKey(t_channel_id uniqueServiceKey, CSectionsdClient::responseGetCurrentNextInfoChannelID& current_next );


CSubService::CSubService(const t_original_network_id anoriginal_network_id, const t_service_id aservice_id, const t_transport_stream_id atransport_stream_id, const std::string &asubservice_name)
{
	service.original_network_id = anoriginal_network_id;
	service.service_id          = aservice_id;
	service.transport_stream_id = atransport_stream_id;
	startzeit                   = 0;
	dauer                       = 0;
	subservice_name             = asubservice_name;
}

CSubService::CSubService(const t_original_network_id anoriginal_network_id, const t_service_id aservice_id, const t_transport_stream_id atransport_stream_id, const time_t astartzeit, const unsigned adauer)
{
	service.original_network_id = anoriginal_network_id;
	service.service_id          = aservice_id;
	service.transport_stream_id = atransport_stream_id;
	startzeit                   = astartzeit;
	dauer                       = adauer;
	subservice_name             = "";
}

t_channel_id CSubService::getChannelID(void) const
{
	t_satellite_position  satellitePosition = 0;
	freq_id_t freq = 0;

	for (tallchans_iterator it = allchans.begin(); it != allchans.end(); it++)
	{
		if(it->second.getServiceId() == service.service_id)
		{
			satellitePosition = it->second.getSatellitePosition();
			freq = it->second.getFreqId();
		}
	}

	return ((uint64_t) ( satellitePosition >= 0 ? satellitePosition : (uint64_t)(0xF000 + abs(satellitePosition))) << 48) |
		(uint64_t) CREATE_CHANNEL_ID(service.service_id, service.original_network_id, service.transport_stream_id);
}

CRemoteControl::CRemoteControl()
{
	current_channel_id = 	0;
	current_sub_channel_id = 0;
	current_channel_name = 	"";
	
	current_channel_number = 0;

	zap_completion_timeout = 0;

	current_EPGid =	0;
	next_EPGid = 	0;
	
	memset(&current_PIDs.PIDs, 0, sizeof(current_PIDs.PIDs) );
	
	has_ac3 = 	false;
	selected_subchannel = -1;
	needs_nvods = 	false;
	director_mode = 0;
	current_programm_timer = 0;
	is_video_started = true;
}

int CRemoteControl::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if ( zap_completion_timeout != 0 ) 
	{
    		if ( (msg == NeutrinoMessages::EVT_ZAP_COMPLETE) || 
		     (msg == NeutrinoMessages::EVT_ZAP_FAILED  ) || 
		     (msg == NeutrinoMessages::EVT_ZAP_ISNVOD  ) ) 
		{
			dprintf(DEBUG_INFO, "CRemoteControl::handleMsg: timeout EVT_ZAP current_channel_id: %llx data: %llx\n", current_channel_id, *(t_channel_id *)data);
			
			if ((*(t_channel_id *)data) != current_channel_id) 
			{
				g_InfoViewer->chanready = 0;
				g_Zapit->zapTo_serviceID_NOWAIT(current_channel_id );
				g_Sectionsd->setServiceChanged(current_channel_id &0xFFFFFFFFFFFFULL, false);

				zap_completion_timeout = getcurrenttime() + 2 * (long long) 1000000;

				return messages_return::handled;
			}
			else 
			{
				zap_completion_timeout = 0;
				g_InfoViewer->chanready = 1;
			}

			if ((!is_video_started) && (g_settings.parentallock_prompt != PARENTALLOCK_PROMPT_NEVER))
				g_RCInput->postMsg( NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false );
		}
	} 
	else 
	{
		if ( (msg == NeutrinoMessages::EVT_ZAP_COMPLETE) || 
		     (msg == NeutrinoMessages::EVT_ZAP_FAILED) || 
		     (msg == NeutrinoMessages::EVT_ZAP_ISNVOD) )
		{
			dprintf(DEBUG_INFO, "CRemoteControl::handleMsg: EVT_ZAP current_channel_id: %llx data: %llx\n", current_channel_id, *(t_channel_id *)data);
			
			g_InfoViewer->chanready = 1;

			// warte auf keine Meldung vom ZAPIT -> jemand anderer hat das zappen ausgel�st...
			if ((*(t_channel_id *)data) != current_channel_id) 
			{
				// get channel name/number
				t_channel_id new_id = *(t_channel_id *)data;
				tallchans_iterator cit = allchans.find(new_id);
				
				if ( cit != allchans.end() )
				{
					current_channel_name = cit->second.getName();
					current_channel_number = cit->second.number;
				}

				// don't show service name in standby mode
				if( CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby )
				{
					CVFD::getInstance()->showServicename(current_channel_name, true,  current_channel_number); // UTF-8						
				}
				
				current_channel_id = new_id;
				is_video_started= true;

				current_EPGid = 0;
				next_EPGid = 0;

				memset(&current_PIDs.PIDs, 0, sizeof(current_PIDs.PIDs) );

				current_PIDs.APIDs.clear();
				has_ac3 = false;

				subChannels.clear();
				selected_subchannel = -1;
				director_mode = 0;
				needs_nvods = (msg == NeutrinoMessages:: EVT_ZAP_ISNVOD);

				g_Sectionsd->setServiceChanged( current_channel_id&0xFFFFFFFFFFFFULL, true );
				CNeutrinoApp::getInstance()->channelList->adjustToChannelID(current_channel_id);
				
				// update info.				
			}

			if ((!is_video_started) && (g_settings.parentallock_prompt != PARENTALLOCK_PROMPT_NEVER))
				g_RCInput->postMsg( NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false );
		}
		else
		{
			if ( (msg == NeutrinoMessages::EVT_ZAP_SUB_COMPLETE) || 
			     (msg == NeutrinoMessages:: EVT_ZAP_SUB_FAILED ) ) 
			{
				dprintf(DEBUG_INFO, "CRemoteControl::handleMsg EVT_ZAP_SUB current_sub_channel_id: %llx data: %llx\n", current_sub_channel_id, *(t_channel_id *)data);
				
				if ((*(t_channel_id *)data) != current_sub_channel_id)
				{
					current_sub_channel_id = *(t_channel_id *)data;

					for(unsigned int i = 0; i < subChannels.size(); i++)
					{
						if (subChannels[i].getChannelID() == (*(t_channel_id *)data))
						{
							selected_subchannel = i;
							break;
						}
					}
				}
			}
		}
	}

	//
	if ( msg == NeutrinoMessages::EVT_CURRENTEPG )
	{
		if ((*(t_channel_id *)data) != (current_channel_id & 0xFFFFFFFFFFFFULL) && (*(t_channel_id *)data) != (current_sub_channel_id & 0xFFFFFFFFFFFFULL))
			return messages_return::handled;

		//
		CSectionsdClient::CurrentNextInfo info_CN;
		sectionsd_getCurrentNextServiceKey(current_channel_id&0xFFFFFFFFFFFFULL, info_CN);
		
		dprintf(DEBUG_INFO, "CRemoteControl::handleMsg got  EVT_CURRENTEPG, uniqueKey: %llx chid: %llx flags: %x\n", info_CN.current_uniqueKey, current_channel_id, info_CN.flags);

		dprintf(DEBUG_INFO, "CRemoteControl::handleMsg comparing: uniqueKey: %llx chid: %llx\n", info_CN.current_uniqueKey >> 16, current_channel_id & 0xFFFFFFFFFFFFULL);
		
		if ((info_CN.current_uniqueKey >> 16) == (current_channel_id & 0xFFFFFFFFFFFFULL) || (info_CN.current_uniqueKey >> 16) == (current_sub_channel_id & 0xFFFFFFFFFFFFULL))
		{
			//CURRENT-EPG for current channel arrived!;
			
#if defined (ENABLE_LCD)
			CVFD::getInstance()->setEPGTitle(info_CN.current_name);
#endif			
			
			if (info_CN.current_uniqueKey != current_EPGid)
			{
				if ( current_EPGid != 0 )
				{
					// ist nur ein neues Programm, kein neuer Kanal
					// PIDs neu holen
					g_Zapit->getPIDS( current_PIDs );
					
					// APID Bearbeitung neu anstossen
					has_unresolved_ctags = true;
					
					// infobar indicate on epg change
					//g_InfoViewer->showEpgInfo();
				}

				current_EPGid = info_CN.current_uniqueKey;

				if ( has_unresolved_ctags )
					processAPIDnames();

				if (selected_subchannel <= 0 && info_CN.flags & CSectionsdClient::epgflags::current_has_linkagedescriptors)
				{
					subChannels.clear();
					getSubChannels();
				}

				if ( needs_nvods )
					getNVODs();
			}

			// is_video_started is only false if channel is locked
			if ((!is_video_started) && (info_CN.current_fsk == 0 || g_settings.parentallock_prompt == PARENTALLOCK_PROMPT_CHANGETOLOCKED))
				g_RCInput->postMsg(NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false);
			else
				g_RCInput->postMsg(NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, info_CN.current_fsk, false);
		}

		return messages_return::handled;
	}
	else if ( msg == NeutrinoMessages::EVT_NEXTEPG )
	{
		if ((*(t_channel_id *)data) != current_channel_id)
			return messages_return::handled;

		if ( !is_video_started )
			g_RCInput->postMsg( NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false );
			
		return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_NOEPG_YET)
	{
		if ((*(t_channel_id *)data) == (current_channel_id&0xFFFFFFFFFFFFULL))
		{
			if ( !is_video_started )
				g_RCInput->postMsg( NeutrinoMessages::EVT_PROGRAMLOCKSTATUS, 0x100, false );
		}
		
		return messages_return::handled;
	}
	else if ( (msg == NeutrinoMessages::EVT_ZAP_COMPLETE) || (msg == NeutrinoMessages::EVT_ZAP_SUB_COMPLETE) ) 
	{
		if ((*(t_channel_id *)data) == ((msg == NeutrinoMessages::EVT_ZAP_COMPLETE) ? current_channel_id : current_sub_channel_id))
		{
			// tell sectionsd to start epg on the zapped channel
			g_Sectionsd->setServiceChanged( current_channel_id&0xFFFFFFFFFFFFULL, false );
		
			// show servicename in VFD
			// don't show service name in standby mode
			if( CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby )
			{
				// get channel number
				t_channel_id new_id = *(t_channel_id *)data;
				tallchans_iterator cit = allchans.find(new_id);
					
				if ( cit != allchans.end() )
				{
					//current_channel_name = cit->second.getName();
					current_channel_number = cit->second.number;
				}
					
				CVFD::getInstance()->showServicename(current_channel_name, true,  current_channel_number); // UTF-8						
			}
			
			// get pids
			g_Zapit->getPIDS(current_PIDs );

			t_channel_id * p = new t_channel_id;
			*p = current_channel_id;
		
			g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOTPIDS, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory

			processAPIDnames();
			
#if ENABLE_RADIOTEXT			
			if (g_settings.radiotext_enable && g_Radiotext && ((CNeutrinoApp::getInstance()->getMode()) == NeutrinoMessages::mode_radio))
			{
				g_Radiotext->setPid(current_PIDs.APIDs[current_PIDs.PIDs.selected_apid].pid);
			}
#endif
		}

	    	return messages_return::handled;
	}
	else if (msg == NeutrinoMessages::EVT_PMT_CHANGED) 
	{
		g_Zapit->getPIDS(current_PIDs);
		processAPIDnames();
		
		return messages_return::unhandled;
	}
	else if (msg == NeutrinoMessages::EVT_ZAP_ISNVOD)
	{
		if ((*(t_channel_id *)data) == current_channel_id)
		{
			needs_nvods = true;
			
			// don't show service name in standby mode
			if( CNeutrinoApp::getInstance()->getMode() != NeutrinoMessages::mode_standby )
			{
				// get channel number
				t_channel_id new_id = *(t_channel_id *)data;
				tallchans_iterator cit = allchans.find(new_id);
					
				if ( cit != allchans.end() )
				{
					//current_channel_name = cit->second.getName();
					current_channel_number = cit->second.number;
				}
								
				CVFD::getInstance()->showServicename(std::string("<") + current_channel_name + '>', true, current_channel_number); // UTF-8					
			}
				
			if ( current_EPGid != 0)
			{
				getNVODs();
				
				if (subChannels.empty())
					g_Sectionsd->setServiceChanged( current_channel_id&0xFFFFFFFFFFFFULL, true );
			}
			else
				// EVENT anfordern!
				g_Sectionsd->setServiceChanged( current_channel_id&0xFFFFFFFFFFFFULL, true );

		}
		
		return messages_return::handled;
	}
	else
		return messages_return::unhandled;
}

void CRemoteControl::getSubChannels()
{
	if ( subChannels.size() == 0 )
	{
		CSectionsdClient::LinkageDescriptorList	linkedServices;
		
		if ( sectionsd_getLinkageDescriptorsUniqueKey( current_EPGid, linkedServices ) )
		{
			if ( linkedServices.size()> 1 )
			{
				are_subchannels = true;
				for (unsigned int i=0; i< linkedServices.size(); i++)
				{
					subChannels.push_back(CSubService(
								      linkedServices[i].originalNetworkId,
								      linkedServices[i].serviceId,
								      linkedServices[i].transportStreamId,
								      linkedServices[i].name));
					if (subChannels[i].getChannelID() == (current_channel_id&0xFFFFFFFFFFFFULL))
						selected_subchannel = i;
				}
				copySubChannelsToZapit();

				t_channel_id * p = new t_channel_id;
				*p = current_channel_id;
				
				g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory
			}
		}
	}
}

void CRemoteControl::getNVODs()
{
	dprintf(DEBUG_INFO, "CRemoteControl::getNVODs: getNVODs, current_EPGid: %llx\n", current_EPGid);
	
	if ( subChannels.size() == 0 )
	{
		CSectionsdClient::NVODTimesList	NVODs;
		
		if ( sectionsd_getNVODTimesServiceKey( current_channel_id & 0xFFFFFFFFFFFFULL, NVODs ) )
		{
			are_subchannels = false;
			
			for (unsigned int i=0; i< NVODs.size(); i++)
			{
				if ( NVODs[i].zeit.dauer> 0 )
				{
					CSubService newService(
						NVODs[i].original_network_id,
						NVODs[i].service_id,
						NVODs[i].transport_stream_id,
						NVODs[i].zeit.startzeit, 
						NVODs[i].zeit.dauer);

					CSubServiceListSorted::iterator e= subChannels.begin();
					for(; e!=subChannels.end(); ++e)
					{
						if ( e->startzeit > newService.startzeit )
							break;
					}
					subChannels.insert( e, newService );
				}

			}

			copySubChannelsToZapit();

			t_channel_id * p = new t_channel_id;
			*p = current_channel_id;
			
			g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOT_SUBSERVICES, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory

			if ( selected_subchannel == -1 )
			{
				// beim ersten Holen letzten NVOD-Kanal setzen!
				setSubChannel( subChannels.size()- 1 );
			}
			else
			{
				// sollte nur passieren, wenn die aktuelle Sendung vorbei ist?!
				selected_subchannel = -1;
			}
		}
	}
}

void CRemoteControl::processAPIDnames()
{
	has_unresolved_ctags= false;
	has_ac3 = false; //FIXME what this variable suppoused to do ?? seems unused
	int pref_found = -1;
	int pref_ac3_found = -1;
	int pref_idx = -1;
	int pref_ac3_idx = -1;
	int ac3_found = -1;
	const char * desc;
	char lang[4];

	if(g_settings.auto_lang) 
	{
		// first we check prefs to find pid according to pref index
		for(int i = 0; i < 3; i++) 
		{
			for(int j = 0; j < (int) current_PIDs.APIDs.size(); j++) 
			{
				desc = current_PIDs.APIDs[j].desc;
				// In some cases AAC is the only audio system used
				// so accept it here as a 'normal' sound track
				if(strstr(desc, "(AAC)")) 
				{
					strncpy(lang, desc, 3);
					lang[3] = 0;
					desc = lang;
				}
				/* processAPIDnames called 2 times, TODO find better way to detect second call */
				if(strlen( desc ) != 3)
					continue;
				
				if(strlen(g_settings.pref_lang[i]) == 0)
					continue;

				std::string temp(g_settings.pref_lang[i]);
				std::map<std::string, std::string>::const_iterator it;
				for(it = iso639.begin(); it != iso639.end(); it++) 
				{
					if(temp == it->second && strcasecmp(desc, it->first.c_str()) == 0) 
					{
						/* remember first pref found index and pid*/
						if(pref_found < 0) 
						{
							pref_found = j;
							pref_idx = i;
						}
						
						if( current_PIDs.APIDs[j].is_ac3 && g_settings.audio_DolbyDigital && (pref_ac3_found < 0)) {
							pref_ac3_found = j;
							pref_ac3_idx = i;
						}
						break;
					}
				}
			} /* for all pids */
		} /*for all prefs*/

		/* reset pref ac3, if it have lower priority */
		if((pref_idx >= 0) && (pref_idx < pref_ac3_idx))
			pref_ac3_found = -1;
	}

	for(unsigned int count = 0; count < current_PIDs.APIDs.size(); count++)
	{
		dprintf(DEBUG_NORMAL, "CRemoteControl::processAPIDnames: apid name= %s (%s) pid= 0x%x\n", current_PIDs.APIDs[count].desc, getISO639Description( current_PIDs.APIDs[count].desc ), current_PIDs.APIDs[count].pid);
		
		if ( current_PIDs.APIDs[count].component_tag != 0xFF )
		{
			has_unresolved_ctags = true;
		}
		
		if ( strlen( current_PIDs.APIDs[count].desc ) == 3 )
		{
			// unaufgeloeste Sprache...
			strcpy( current_PIDs.APIDs[count].desc, getISO639Description( current_PIDs.APIDs[count].desc ) );
		}

		if ( current_PIDs.APIDs[count].is_ac3 )
		{
			//strncat(current_PIDs.APIDs[count].desc, " (Dolby Digital)", 25);
			has_ac3 = true;
			if((strlen( current_PIDs.APIDs[count].desc ) == 3) && g_settings.audio_DolbyDigital && (ac3_found < 0))
				ac3_found = count;
		}
	}

	if ( has_unresolved_ctags )
	{
		if ( current_EPGid != 0 )
		{
			CSectionsdClient::ComponentTagList tags;

			if ( sectionsd_getComponentTagsUniqueKey( current_EPGid, tags ) )
			{
				has_unresolved_ctags = false;

				for (unsigned int i = 0; i < tags.size(); i++)
				{
					for (unsigned int j = 0; j < current_PIDs.APIDs.size(); j++)
					{
						if ( current_PIDs.APIDs[j].component_tag == tags[i].componentTag )
						{
							// workaround for buggy ZDF ctags / or buggy sectionsd/drivers , who knows...
							if(!tags[i].component.empty())
							{
								strncpy(current_PIDs.APIDs[j].desc, tags[i].component.c_str(), 25);
							}
							current_PIDs.APIDs[j].component_tag = -1;
							break;
						}
					}
				}
			}
		}
	}
	
	dprintf(DEBUG_NORMAL, "CRemoteControl::processAPIDnames: pref_found %d pref_ac3_found %d ac3_found %d\n", pref_found, pref_ac3_found, ac3_found);
	
	if(pref_ac3_found >= 0) 
	{
		dprintf(DEBUG_NORMAL, "CRemoteControl::processAPIDnames: set apid name= %s pid= 0x%x\n", current_PIDs.APIDs[pref_ac3_found].desc, current_PIDs.APIDs[pref_ac3_found].pid);
		setAPID(pref_ac3_found);
	} 
	else if(pref_found >= 0) 
	{
		dprintf(DEBUG_INFO, "CRemoteControl::processAPIDnames: set apid name= %s pid= 0x%x\n", current_PIDs.APIDs[pref_found].desc, current_PIDs.APIDs[pref_found].pid);
		setAPID(pref_found);
	}
	else if(ac3_found >= 0) 
	{
		dprintf(DEBUG_NORMAL, "CRemoteControl::processAPIDnames: set apid name= %s pid= 0x%x\n", current_PIDs.APIDs[ac3_found].desc, current_PIDs.APIDs[ac3_found].pid);
		setAPID(ac3_found);
	}
	else if ( current_PIDs.PIDs.selected_apid >= current_PIDs.APIDs.size() )
	{
		setAPID( 0 );
	}

	t_channel_id * p = new t_channel_id;
	*p = current_channel_id;
			
	g_RCInput->postMsg(NeutrinoMessages::EVT_ZAP_GOTAPIDS, (const neutrino_msg_data_t)p, false); // data is pointer to allocated memory
}

void CRemoteControl::copySubChannelsToZapit(void)
{
	CZapitClient::subServiceList zapitList;

	for (CSubServiceListSorted::const_iterator e = subChannels.begin(); e != subChannels.end(); e++)
		zapitList.push_back(e->getAsZapitSubService());

	g_Zapit->setSubServices(zapitList);
}

void CRemoteControl::setAPID( uint32_t APID )
{
	if ((current_PIDs.PIDs.selected_apid == APID ) || (APID >= current_PIDs.APIDs.size()))
		return;

	current_PIDs.PIDs.selected_apid = APID;
	g_Zapit->setAudioChannel( APID );
	
	// needed for auto audio select
	CVFD::getInstance()->ShowIcon(VFD_ICON_DOLBY, current_PIDs.APIDs[current_PIDs.PIDs.selected_apid].is_ac3? true : false);
}

static const std::string empty_string;

const std::string & CRemoteControl::setSubChannel(const int numSub, const bool force_zap)
{
	if ((numSub < 0) || (numSub >= (int)subChannels.size()))
		return empty_string;

	if ((selected_subchannel == numSub ) && (!force_zap))
		return empty_string;

	selected_subchannel = numSub;
	current_sub_channel_id = subChannels[numSub].getChannelID();
	g_InfoViewer->chanready = 0;

	if(scrambled_timer) 
	{
		g_RCInput->killTimer(scrambled_timer);
		scrambled_timer = 0;
	}

	g_Zapit->zapTo_subServiceID_NOWAIT( current_sub_channel_id );
	
	// Houdini: to restart reading the private EPG when switching to a new option
	g_Sectionsd->setServiceChanged( current_sub_channel_id , true );

	return subChannels[numSub].subservice_name;
}

const std::string & CRemoteControl::subChannelUp(void)
{
 	// if there are any NVOD/subchannels switch these else switch audio channel (if any)
  	if (subChannels.size() > 0 )
  	{
  		return setSubChannel((subChannels.size() == 0) ? -1 : (int)((selected_subchannel + 1) % subChannels.size()));
  	}
  	else
  	{
  		if (current_PIDs.APIDs.size() > 0)
  		{
  			setAPID((current_PIDs.PIDs.selected_apid + 1) % current_PIDs.APIDs.size());
  		}
  		return (empty_string);
  	}
}

const std::string & CRemoteControl::subChannelDown(void)
{
	// if there are any NVOD/subchannels switch these else switch audio channel (if any)
  	if (subChannels.size() > 0 )
  	{
  		return setSubChannel((selected_subchannel <= 0) ? (subChannels.size() - 1) : (selected_subchannel - 1));
  	}
  	else
  	{
  		if (current_PIDs.APIDs.size() > 0)
  		{
  			if (current_PIDs.PIDs.selected_apid <= 0)
  				setAPID(current_PIDs.APIDs.size() - 1);
  			else
  				setAPID((current_PIDs.PIDs.selected_apid - 1));
  		}
  		return (empty_string);
  	}
}

void stopAutoRecord();
extern int abort_zapit;

void CRemoteControl::zapTo_ChannelID(const t_channel_id channel_id, const std::string & channame, const bool start_video) // UTF-8
{
	current_channel_id = channel_id;
	current_channel_name = channame;
	
	dprintf(DEBUG_NORMAL, "CRemoteControl::zapTo_ChannelID:%llx\n", channel_id);
	
	if (start_video)
		startvideo();
	else
		stopvideo();

	current_sub_channel_id = 0;
	current_EPGid = 0;
	next_EPGid = 0;

	memset(&current_PIDs.PIDs, 0, sizeof(current_PIDs.PIDs) );

	current_PIDs.APIDs.clear();
	has_ac3 = false;

	subChannels.clear();
	selected_subchannel = -1;
	needs_nvods = false;
	director_mode = 0;

	unsigned long long now = getcurrenttime();
	if ( zap_completion_timeout < now )
	{
		g_InfoViewer->chanready = 0;		
		
		if(autoshift) 
		{
			stopAutoRecord();
			CNeutrinoApp::getInstance()->recordingstatus = 0;
		}
		
		if(scrambled_timer) 
		{
			g_RCInput->killTimer(scrambled_timer);
			scrambled_timer = 0;
		}
		
		abort_zapit = 1;

		// zap
		g_Zapit->zapTo_serviceID_NOWAIT(channel_id);

		// getEventsFromHTTP / localtv
		if(g_settings.epg_enable_online_epg)
		{
			getEventsFromHTTP(channel_id);
		}
		
		// xmltv FIXME:
		//getEventsFromXMLTV(channel_id);
		
		abort_zapit = 0;

		zap_completion_timeout = now + 2 * (long long) 1000000;
		
		if ( current_programm_timer != 0 )
		{
			g_RCInput->killTimer( current_programm_timer );
			current_programm_timer = 0;
		}
	}
}

void CRemoteControl::startvideo()
{
	if ( !is_video_started )
	{
		is_video_started = true;
		g_Zapit->startPlayBack();
	}
}

void CRemoteControl::stopvideo()
{
	if ( is_video_started )
	{
		is_video_started= false;
		g_Zapit->stopPlayBack();
	}
}

void CRemoteControl::radioMode()
{
	dprintf(DEBUG_NORMAL, "CRemoteControl::radioMode\n");
	
	g_Zapit->setMode( CZapitClient::MODE_RADIO );
	
	CVFD::getInstance()->ShowIcon(VFD_ICON_RADIO, true);
	CVFD::getInstance()->ShowIcon(VFD_ICON_TV, false);
}

void CRemoteControl::tvMode()
{
	dprintf(DEBUG_NORMAL, "CRemoteControl::tvMode\n");
	
	g_Zapit->setMode( CZapitClient::MODE_TV );
	
	CVFD::getInstance()->ShowIcon(VFD_ICON_RADIO, false);
	CVFD::getInstance()->ShowIcon(VFD_ICON_TV, true);
}

// defined in sectionsd.cpp
void insertEventsfromHTTP(std::string& url, t_original_network_id _onid, t_transport_stream_id _tsid, t_service_id _sid);

// online epg get events
void CRemoteControl::getEventsFromHTTP(t_channel_id chid)
{
	dprintf(DEBUG_NORMAL, "CRemoteControl::getEventsFromHTTP: channelID: %llx\n", chid);
	
	//
	t_channel_id epgid = 0;
	CZapitChannel *chan = NULL;
	
	for (tallchans_iterator it = allchans.begin(); it != allchans.end(); it++)
	{
		if(it->second.getChannelID() == chid)
		{
			chan = &it->second;
		}
	}
	
	if (chan) epgid = chan->getEPGID();
	
	dprintf(DEBUG_NORMAL, "CRemoteControl::getEventsFromHTTP: epgid: %llx\n", epgid);
	
	// localtv
	std::string evUrl;

	if(g_settings.epg_serverbox_gui == SNeutrinoSettings::SATIP_SERVERBOX_GUI_ENIGMA2)
	{
		evUrl = "http://";
		evUrl += g_settings.epg_serverbox_ip;
		evUrl += "/web/epgservice?sRef=1:0:"; 

		evUrl += to_hexstring(1);
		evUrl += ":";
		evUrl += to_hexstring(GET_SERVICE_ID_FROM_CHANNEL_ID(epgid)); //sid
		evUrl += ":";
		evUrl += to_hexstring(GET_TRANSPORT_STREAM_ID_FROM_CHANNEL_ID(epgid)); //tsid
		evUrl += ":";
		evUrl += to_hexstring(GET_ORIGINAL_NETWORK_ID_FROM_CHANNEL_ID(epgid)); //onid
		evUrl += ":";

		if(g_settings.epg_serverbox_type == DVB_C)
		{
			evUrl += "FFFF"; // namenspace for cable
		}
		else if (g_settings.epg_serverbox_type == DVB_T)
		{
			evUrl += "EEEE"; // namenspace for terrestrial
		}
		else if (g_settings.epg_serverbox_type == DVB_S)
		{
			// namenspace for sat
			evUrl += to_hexstring(GET_SATELLITEPOSITION_FROM_CHANNEL_ID(epgid)); //satpos
		}

		evUrl += "0000";
		evUrl += ":";
		evUrl += "0:0:0:";
	}
	else if( (g_settings.epg_serverbox_gui == SNeutrinoSettings::SATIP_SERVERBOX_GUI_NMP) || (g_settings.epg_serverbox_gui == SNeutrinoSettings::SATIP_SERVERBOX_GUI_NHD2) )
	{
		evUrl = "http://";
		evUrl += g_settings.epg_serverbox_ip;
		evUrl += "/control/epg?channelid=";

         	evUrl += to_hexstring(epgid);

		evUrl += "&xml=true&details=true";
	}

	//
	insertEventsfromHTTP(evUrl, GET_ORIGINAL_NETWORK_ID_FROM_CHANNEL_ID(chid), GET_TRANSPORT_STREAM_ID_FROM_CHANNEL_ID(chid), GET_SERVICE_ID_FROM_CHANNEL_ID(chid));
}

//
// defined in sectionsd.cpp
void insertEventsUnikKeyfromXMLTV(std::string& url, std::string& epgid, t_original_network_id _onid, t_transport_stream_id _tsid, t_service_id _sid);

// online epg get events
void CRemoteControl::getEventsFromXMLTV(t_channel_id chid)
{
	dprintf(DEBUG_NORMAL, "CRemoteControl::getEventsFromXMLTV: channelID: %llx\n", chid);
	
	//
	CZapitChannel *chan = NULL;
	
	for (tallchans_iterator it = allchans.begin(); it != allchans.end(); it++)
	{
		if(it->second.getChannelID() == chid)
		{
			chan = &it->second;
		}
	}
	
	// localtv
	std::string evUrl;
	std::string epg_id;

	//
	if (chan)
	{
		evUrl = chan->getEPGUrl();
		epg_id = chan->getEPGIDName();
	
		insertEventsUnikKeyfromXMLTV(evUrl, epg_id, GET_ORIGINAL_NETWORK_ID_FROM_CHANNEL_ID(chid), GET_TRANSPORT_STREAM_ID_FROM_CHANNEL_ID(chid), GET_SERVICE_ID_FROM_CHANNEL_ID(chid));
	}
}

