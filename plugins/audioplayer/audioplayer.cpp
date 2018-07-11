/*
  $Id: audioplayer.cpp 2018/07/10 mohousch Exp $

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

#include <plugin.h>
#include <fstream>


extern "C" void plugin_exec(void);
extern "C" void plugin_init(void);
extern "C" void plugin_del(void);

#define SHOW_FILE_LOAD_LIMIT 50
#define AUDIOPLAYER_CHECK_FOR_DUPLICATES

const long int GET_PLAYLIST_TIMEOUT = 10;

class CMP3Player : public CMenuTarget
{
	private:
		CFrameBuffer* frameBuffer;

		ClistBox* alist;
		CMenuItem* item;

		CAudioPlayerGui tmpAudioPlayerGui;
		CFileFilter fileFilter;
		CFileList filelist;
		CAudioPlayList playlist;
		std::string Path;
		int selected;

		void loadPlaylist(bool reload = true);

		//
		bool shufflePlaylist(void);
		
	public:
		CMP3Player();
		~CMP3Player();
		int exec(CMenuTarget* parent, const std::string& actionKey);
		void hide();
		void showMenu(bool reload = true);
};

CMP3Player::CMP3Player()
{
	frameBuffer = CFrameBuffer::getInstance();

	alist = NULL;
	item = NULL;

	selected = 0;

	fileFilter.addFilter("cdr");
	fileFilter.addFilter("mp3");
	fileFilter.addFilter("m2a");
	fileFilter.addFilter("mpa");
	fileFilter.addFilter("mp2");
	fileFilter.addFilter("ogg");
	fileFilter.addFilter("wav");
	fileFilter.addFilter("flac");
	fileFilter.addFilter("aac");
	fileFilter.addFilter("dts");
	fileFilter.addFilter("m4a");

	CAudioPlayer::getInstance()->init();
}

CMP3Player::~CMP3Player()
{
	playlist.clear();
}

void CMP3Player::hide()
{
	frameBuffer->paintBackground();
	frameBuffer->blit();
}

bool CMP3Player::shufflePlaylist(void)
{
	dprintf(DEBUG_NORMAL, "CMP3Player::shufflePlaylist\n");
	
	RandomNumber rnd;
	bool result = false;
	
	if (!(playlist.empty()))
	{
		if (selected > 0)
		{
			std::swap(playlist[0], playlist[selected]);
			selected = 0;
		}

		std::random_shuffle((selected != 0) ? playlist.begin() : playlist.begin() + 1, playlist.end(), rnd);

		selected = 0;

		result = true;
	}
	
	return(result);
}

void CMP3Player::loadPlaylist(bool reload)
{
	Path = g_settings.network_nfs_audioplayerdir;

	if(g_settings.audioplayer_read_playlist_at_start || reload)
	{
		if(CFileHelpers::getInstance()->readDir(Path, &filelist, &fileFilter))
		{		
			CFileList::iterator files = filelist.begin();
			for(; files != filelist.end() ; files++)
			{
				if ( (files->getExtension() == CFile::EXTENSION_CDR)
					||  (files->getExtension() == CFile::EXTENSION_MP3)
					||  (files->getExtension() == CFile::EXTENSION_WAV)
					||  (files->getExtension() == CFile::EXTENSION_FLAC))
				{
					CAudiofileExt audiofile(files->Name, files->getExtension());
				
					playlist.push_back(audiofile);
				}
			}
		}
	}
}

int CMP3Player::exec(CMenuTarget* parent, const std::string& actionKey)
{
	dprintf(DEBUG_NORMAL, "CMP3Player::exec: actionKey:%s\n", actionKey.c_str());
	
	if(parent)
		hide();

	if(actionKey == "aplay")
	{
		selected = alist->getSelected();

		for (unsigned int i = 0; i < playlist.size(); i++)
		{
			tmpAudioPlayerGui.addToPlaylist(playlist[i]);
		}

		tmpAudioPlayerGui.setCurrent(selected);
		tmpAudioPlayerGui.exec(NULL, "");
	}
	else if(actionKey == "RC_setup")
	{
		CAudioPlayerSettings * audioPlayerSettingsMenu = new CAudioPlayerSettings();
		audioPlayerSettingsMenu->exec(this, "");
		delete audioPlayerSettingsMenu;
		audioPlayerSettingsMenu = NULL;						
	}
	else if(actionKey == "RC_red")
	{
		CAudioPlayList::iterator p = playlist.begin() + alist->getSelected();
		playlist.erase(p);

		if (selected >= playlist.size())
			selected = playlist.size() - 1;

		showMenu();
		return menu_return::RETURN_EXIT_ALL;
	}
	else if(actionKey == "RC_green")
	{
		playlist.clear();

		CFileBrowser filebrowser((g_settings.filebrowser_denydirectoryleave) ? g_settings.network_nfs_picturedir : "");

		filebrowser.Multi_Select = true;
		filebrowser.Dirs_Selectable = true;
		filebrowser.Filter = &fileFilter;

		if (filebrowser.exec(Path.c_str()))
		{
			Path = filebrowser.getCurrentDir();
			CFileList::const_iterator files = filebrowser.getSelectedFiles().begin();
			for(; files != filebrowser.getSelectedFiles().end(); files++)
			{
				if ( (files->getExtension() == CFile::EXTENSION_CDR)
					||  (files->getExtension() == CFile::EXTENSION_MP3)
					||  (files->getExtension() == CFile::EXTENSION_WAV)
					||  (files->getExtension() == CFile::EXTENSION_FLAC))
				{
					CAudiofileExt audiofile(files->Name, files->getExtension());
							
					playlist.push_back(audiofile);
				}
			}
		}

		showMenu();
		return menu_return::RETURN_EXIT_ALL;
	}
	else if(actionKey == "RC_yellow")
	{
		playlist.clear();
		showMenu(false);
		return menu_return::RETURN_EXIT_ALL;
	}
	else if(actionKey == "RC_blue")
	{
		shufflePlaylist();
		showMenu();
		return menu_return::RETURN_EXIT_ALL;
	}
	
	return menu_return::RETURN_REPAINT;
}

#define HEAD_BUTTONS_COUNT 2
const struct button_label HeadButtons[HEAD_BUTTONS_COUNT] =
{
	{ NEUTRINO_ICON_BUTTON_SETUP, NONEXISTANT_LOCALE, NULL },
	{ NEUTRINO_ICON_BUTTON_HELP, NONEXISTANT_LOCALE, NULL }
};

#define FOOT_BUTTONS_COUNT 4
const struct button_label AudioPlayerButtons[FOOT_BUTTONS_COUNT] =
{
	{ NEUTRINO_ICON_BUTTON_RED, LOCALE_AUDIOPLAYER_DELETE, NULL },
	{ NEUTRINO_ICON_BUTTON_GREEN, LOCALE_AUDIOPLAYER_ADD, NULL },
	{ NEUTRINO_ICON_BUTTON_YELLOW, LOCALE_AUDIOPLAYER_DELETEALL, NULL },
	{ NEUTRINO_ICON_BUTTON_BLUE, LOCALE_AUDIOPLAYER_SHUFFLE, NULL }
};

void CMP3Player::showMenu(bool reload)
{
	alist = new ClistBox(LOCALE_AUDIOPLAYER_HEAD, NEUTRINO_ICON_MP3, w_max ( (frameBuffer->getScreenWidth() / 20 * 17), (frameBuffer->getScreenWidth() / 20 )), h_max ( (frameBuffer->getScreenHeight() / 20 * 16), (frameBuffer->getScreenHeight() / 20)));

	//
	loadPlaylist(reload);

	for(unsigned int i = 0; i < playlist.size(); i++)
	{
		std::string title;
		std::string artist;
		std::string genre;
		std::string date;
		char duration[9] = "";

		// read metadata
		int ret = CAudioPlayer::getInstance()->readMetaData(&playlist[i], true);

		if (!ret || (playlist[i].MetaData.artist.empty() && playlist[i].MetaData.title.empty() ))
		{
			//remove extension (.mp3)
			std::string tmp = playlist[i].Filename.substr(playlist[i].Filename.rfind('/') + 1);
			tmp = tmp.substr(0, tmp.length() - 4);	//remove extension (.mp3)

			std::string::size_type j = tmp.rfind(" - ");
		
			if(j != std::string::npos)
			{ 
				title = tmp.substr(0, j);
				artist = tmp.substr(j + 3);
			}
			else
			{
				j = tmp.rfind('-');
				if(j != std::string::npos)
				{
					title = tmp.substr(0, j);
					artist = tmp.substr(j + 1);
				}
				else
					title = tmp;
			}
		}
		else
		{
			title = playlist[i].MetaData.title;
			artist = playlist[i].MetaData.artist;
			genre = playlist[i].MetaData.genre;	
			date = playlist[i].MetaData.date;

			snprintf(duration, 8, "(%ld:%02ld)", playlist[i].MetaData.total_time / 60, playlist[i].MetaData.total_time % 60);
		}

		//
		item = new ClistBoxItem(title.c_str(), true, "", this, "aplay");
			
		item->setOptionInfo(duration);
		item->setNumber(i + 1);

		// details Box
		item->setInfo1(title.c_str());
		item->setOptionInfo1(genre.c_str());
		item->setInfo2(artist.c_str());
		item->setOptionInfo2(date.c_str());

		alist->addItem(item);
	}

	//plist->setTimeOut(g_settings.timing[SNeutrinoSettings::TIMING_CHANLIST]);
	alist->setSelected(selected);

	alist->setHeaderButtons(HeadButtons, HEAD_BUTTONS_COUNT);
	alist->setFooterButtons(AudioPlayerButtons, FOOT_BUTTONS_COUNT);
	
	alist->enableFootInfo();
	alist->enablePaintDate();

	alist->addKey(CRCInput::RC_setup, this, CRCInput::getSpecialKeyName(CRCInput::RC_setup));
	alist->addKey(CRCInput::RC_red, this, CRCInput::getSpecialKeyName(CRCInput::RC_red));
	alist->addKey(CRCInput::RC_green, this, CRCInput::getSpecialKeyName(CRCInput::RC_green));
	alist->addKey(CRCInput::RC_yellow, this, CRCInput::getSpecialKeyName(CRCInput::RC_yellow));
	alist->addKey(CRCInput::RC_blue, this, CRCInput::getSpecialKeyName(CRCInput::RC_blue));

	alist->exec(NULL, "");
	//alist->hide();
	delete alist;
	alist = NULL;
}

void plugin_init(void)
{
	dprintf(DEBUG_NORMAL, "CMP3Player: plugin_init\n");
}

void plugin_del(void)
{
	dprintf(DEBUG_NORMAL, "CMP3Player: plugin_del\n");
}

void plugin_exec(void)
{
	CMP3Player* audioPlayerHandler = new CMP3Player();
	
	audioPlayerHandler->showMenu();
	
	delete audioPlayerHandler;
	audioPlayerHandler = NULL;
}


