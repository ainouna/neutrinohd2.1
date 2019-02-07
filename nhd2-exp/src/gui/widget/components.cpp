/*
 * $Id: components.cpp 07.02.2019 mohousch Exp $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <global.h>

#include <gui/widget/components.h>


void CComponents::paintBoxRel(const int x, const int y, const int dx, const int dy, fb_pixel_t col, int radius, int type, int mode)
{
	CFrameBuffer::getInstance()->paintBoxRel(x, y, dx, dy, col, radius, type, mode);
}

bool CComponents::paintIcon(const std::string & filename, const int x, const int y, const int h, bool paint, int width, int height)
{
	CFrameBuffer::getInstance()->paintIcon(filename, x, y, h, paint, width, height);
}

bool CComponents::displayImage(const std::string & name, int posx, int posy, int width, int height, CFrameBuffer::ScalingMode scaling, int x_pan, int y_pan, bool clearfb)
{
	CFrameBuffer::getInstance()->displayImage(name, posx, posy, width, height, scaling, x_pan, y_pan, clearfb);
}

bool CComponents::displayLogo(t_channel_id channel_id, int posx, int posy, int width, int height, bool upscale, bool center_x, bool center_y)
{
	CFrameBuffer::getInstance()->displayLogo(channel_id, posx, posy, width, height, upscale, center_x, center_y);
}

void CComponents::paintBackground()
{
	CFrameBuffer::getInstance()->paintBackground();
}

void CComponents::paintBackgroundBoxRel(int x, int y, int dx, int dy)
{
	CFrameBuffer::getInstance()->paintBackgroundBoxRel(x, y, dx, dy);
}

bool CComponents::loadBackgroundPic(const std::string& filename, bool show)
{
	CFrameBuffer::getInstance()->loadBackgroundPic(filename, show);
}

void CComponents::paintVLineRel(int x, int y, int dy, const fb_pixel_t col)
{
	CFrameBuffer::getInstance()->paintVLineRel(x, y, dy, col);
}

void CComponents::paintHLineRel(int x, int dx, int y, const fb_pixel_t col)
{
	CFrameBuffer::getInstance()->paintHLineRel(x, dx, y, col);
}

void CComponents::paintFrameBox(const int x, const int y, const int dx, const int dy, const fb_pixel_t col)
{
	CFrameBuffer::getInstance()->paintFrameBox(x, y, dx, dy, col);
}

void CComponents::saveBackgroundImage(void)
{
	CFrameBuffer::getInstance()->saveBackgroundImage();
}

void CComponents::restoreBackgroundImage(void)
{
	CFrameBuffer::getInstance()->restoreBackgroundImage();
}

void CComponents::saveScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	CFrameBuffer::getInstance()->saveScreen(x, y, dx, dy, memp);
}

void CComponents::restoreScreen(int x, int y, int dx, int dy, fb_pixel_t * const memp)
{
	CFrameBuffer::getInstance()->restoreScreen(x, y, dx, dy, memp);
}










