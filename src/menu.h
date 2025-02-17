/***************************************************************************
 *   Copyright (C) 2006 by Massimiliano Torromeo   *
 *   massimiliano.torromeo@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#ifndef MENU_H
#define MENU_H

#include <string>
#include <vector>
#include "link.h"

using std::string;
using std::vector;

class LinkApp;
class GMenu2X;

typedef vector<Link*> linklist;

/**
Handles the menu structure

	@author Massimiliano Torromeo <massimiliano.torromeo@gmail.com>
*/
class Menu {
private:
	GMenu2X *gmenu2x;
	int iSectionIndex, iLinkIndex;
	int32_t firstDispSection, firstDispLink;
	vector<string> sections;
	vector<linklist> links;

	const int iconPadding = 4;
	uint32_t section_changed, icon_changed;

	Surface *iconBGoff, *iconBGon;
	string iconDescription = "";

	SDL_TimerID sectionChangedTimer = NULL;
	SDL_TimerID iconChangedTimer = NULL;

	void drawIcon(int i, int ix, int iy, bool selected);
	void drawList();
	void drawGrid();
	void drawCoverFlow();
	void drawSectionBar();
	void drawStatusBar();
	void drawIconTray();

public:
	Menu(GMenu2X *gmenu2x);
	~Menu();
	uint32_t linkCols, linkRows, linkWidth, linkHeight, linkSpacing = 4;

	linklist *sectionLinks(int i = -1);

	int sectionNumItems();

	const string getSectionName();
	void setSectionIndex(int i);

	void readSections();
	void readLinks();
	bool addActionLink(uint32_t section, const string &title, fastdelegate::FastDelegate0<> action, const string &description="", const string &icon="");
	bool addLink(string exec);
	bool addSection(const string &sectionName);
	void deleteSelectedLink();
	void deleteSelectedSection();

	void loadIcons();
	bool linkChangeSection(uint32_t linkIndex, uint32_t oldSectionIndex, uint32_t newSectionIndex);

	Link *getLink();
	LinkApp *getLinkApp();
	void pageUp();
	void pageDown();
	void linkLeft();
	void linkRight();
	void linkUp();
	void linkDown();
	void setLinkIndex(int i);

	string sectionPath(int section = -1);

	const vector<string> &getSections() { return sections; }
	void renameSection(int index, const string &name);
	int getSectionIndexByName(const string &name);
	const string getSectionIcon(int i = -1);

	void initLayout();
	void exec();

	int getSectionIndex() { return iSectionIndex; }
	int getLinkIndex() { return iLinkIndex; }
	const string &getSection() { return sections[iSectionIndex]; }

	string getBatteryIcon(uint8_t level);
	string getBrightnessIcon(uint8_t level);
	string getVolumeIcon(uint8_t level);
};

#endif
