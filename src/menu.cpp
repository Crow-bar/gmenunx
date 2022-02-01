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
#include <unistd.h>
#include <sstream>
#include <sys/types.h>
#include <dirent.h>
#include <algorithm>
#include <math.h>
#include <fstream>

#include "linkapp.h"
#include "menu.h"
#include "debug.h"
#include "messagebox.h"
#include "powermanager.h"
#include "utilities.h"

Menu::Menu(GMenu2X *gmenu2x):
gmenu2x(gmenu2x) {
	firstDispSection = 0;

	readSections();

	setSectionIndex(0);
}

Menu::~Menu() {
	for (vector<linklist>::iterator section = links.begin(); section < links.end(); section++) {
		for (linklist::iterator link = section->begin(); link < section->end(); link++) {
			delete *link;
		}
	}
}

void Menu::readSections() {
	DIR *dirp;
	struct dirent *dptr;

	string sectiondir = home_path("sections");
	mkdir(sectiondir.c_str(), 0777);

	if ((dirp = opendir(sectiondir.c_str())) == NULL) {
		return;
	}

	sections.clear();
	links.clear();

	while ((dptr = readdir(dirp))) {
		if (dptr->d_name[0] == '.') {
			continue;
		}

		string section = sectiondir + "/" + dptr->d_name;
		if (dir_exists(section)) {
			sections.push_back((string)dptr->d_name);
			linklist ll;
			links.push_back(ll);
		}
	}

	addSection("settings");
	addSection("applications");

	closedir(dirp);
	sort(sections.begin(),sections.end(), case_less());
}

void Menu::readLinks() {
	vector<string> linkfiles;

	iLinkIndex = 0;
	firstDispLink = 0;

	DIR *dirp;
	struct dirent *dptr;

	for (uint32_t i = 0; i < links.size(); i++) {
		links[i].clear();
		linkfiles.clear();

		if ((dirp = opendir(sectionPath(i).c_str())) == NULL) {
			continue;
		}

		while ((dptr = readdir(dirp))) {
			if (dptr->d_name[0] == '.') {
				continue;
			}
			string filepath = sectionPath(i) + dptr->d_name;
			if (filepath.substr(filepath.size() - 5, 5) == "-opkg") {
				continue;
			}

			linkfiles.push_back(filepath);
		}

		sort(linkfiles.begin(), linkfiles.end(), case_less());
		for (uint32_t x = 0; x < linkfiles.size(); x++) {
			LinkApp *link = new LinkApp(gmenu2x, linkfiles[x].c_str());
			if (link->targetExists()) {
				links[i].push_back(link);
			} else {
				delete link;
			}
		}

		closedir(dirp);
	}
}

linklist *Menu::sectionLinks(int i) {
	if (i < 0 || i > (int)links.size()) {
		i = iSectionIndex;
	}

	if (i < 0 || i > (int)links.size()) {
		return NULL;
	}

	return &links[i];
}

const string Menu::getSectionName() {
	string sectionname = sections[iSectionIndex];
	string::size_type pos = sectionname.find(".");

	if (sectionname == "applications") {
		return "apps";
	}

	if (sectionname == "emulators") {
		return "emus";
	}

	if (pos != string::npos && pos > 0 && pos < sectionname.length()) {
		return sectionname.substr(pos + 1);
	}

	return sectionname;
}

int Menu::sectionNumItems() {
	if (gmenu2x->skinConfInt["sectionBar"] == SB_LEFT || gmenu2x->skinConfInt["sectionBar"] == SB_RIGHT) {
		return (gmenu2x->platform->h - 40) / gmenu2x->skinConfInt["sectionBarSize"];
	}

	if (gmenu2x->skinConfInt["sectionBar"] == SB_TOP || gmenu2x->skinConfInt["sectionBar"] == SB_BOTTOM) {
		return (gmenu2x->platform->w - 40) / gmenu2x->skinConfInt["sectionBarSize"];
	}

	return (gmenu2x->platform->w / gmenu2x->skinConfInt["sectionBarSize"]) - 1;
}

void Menu::setSectionIndex(int i) {
	if (i < 0) {
		i = sections.size() - 1;
	} else if (i >= (int)sections.size()) {
		i = 0;
	}

	int numSections = sectionNumItems();

	if (i >= (int)firstDispSection + numSections) {
		firstDispSection = i - numSections + 1;
	} else if (i < (int)firstDispSection) {
		firstDispSection = i;
	}

	iSectionIndex = i;
	iLinkIndex = 0;
	firstDispLink = 0;
}

string Menu::sectionPath(int section) {
	if (section < 0 || section > (int)sections.size()) {
		section = iSectionIndex;
	}

	return home_path("sections/") + sections[section] + "/";
}

// LINKS MANAGEMENT
bool Menu::addActionLink(uint32_t section, const string &title, fastdelegate::FastDelegate0<> action, const string &description, const string &icon) {
	if (section >= sections.size()) {
		return false;
	}

	Link *linkact = new Link(gmenu2x, action);
	linkact->setTitle(title);
	linkact->setDescription(description);
	linkact->setIcon("skin:icons/" + icon);
	linkact->setBackdrop("skin:backdrops/" + icon);

	sectionLinks(section)->push_back(linkact);
	return true;
}

bool Menu::addLink(string exec) {
	string section = getSection();
	string title = base_name(exec, true);
	string linkpath = unique_filename(home_path("sections/") + section + "/" + title, ".lnk");

	// Reduce title length to fit the link width
	if ((int)gmenu2x->font->getTextWidth(title) > linkWidth) {
		while ((int)gmenu2x->font->getTextWidth(title + "..") > linkWidth) {
			title = title.substr(0, title.length() - 1);
		}
		title += "..";
	}

	INFO("Adding link: '%s'", linkpath.c_str());

	LinkApp *link = new LinkApp(gmenu2x, linkpath.c_str());
	if (!exec.empty()) link->setExec(exec);
	if (!title.empty()) link->setTitle(title);
	link->save();

	int iSectionIndex = find(sections.begin(), sections.end(), section) - sections.begin();
	if (iSectionIndex >= 0 && iSectionIndex < (int)sections.size()) {
		links[iSectionIndex].push_back(link);
		setLinkIndex(links[iSectionIndex].size() - 1);
	}

	return true;
}

bool Menu::addSection(const string &sectionName) {
	string sectiondir = home_path("sections/") + sectionName;
	if (mkdir(sectiondir.c_str(), 0777) == 0) {
		sections.push_back(sectionName);
		linklist ll;
		links.push_back(ll);
		return true;
	}
	string tmpfile = sectiondir + "/.section";
	FILE *fp = fopen(tmpfile.c_str(), "wb+");
	if (fp) fclose(fp);
	return false;
}

void Menu::deleteSelectedLink() {
	string iconpath = getLink()->getIconPath();

	INFO("Deleting link '%s'", getLink()->getTitle().c_str());

	if (getLinkApp() != NULL) unlink(getLinkApp()->getFile().c_str());
	sectionLinks()->erase(sectionLinks()->begin() + iLinkIndex);
	setLinkIndex(iLinkIndex);

	for (uint32_t i = 0; i < sections.size(); i++) {
		for (uint32_t j = 0; j < sectionLinks(i)->size(); j++) {
			if (iconpath == sectionLinks(i)->at(j)->getIconPath()) {
				return; // icon in use by another link; return here.
			}
		}
	}

	gmenu2x->sc.del(iconpath);
}

void Menu::deleteSelectedSection() {
	INFO("Deleting section '%s'", getSection().c_str());

	string iconpath = home_path("sections/") + getSection() + ".png";

	links.erase(links.begin() + iSectionIndex);
	sections.erase(sections.begin() + iSectionIndex);
	setSectionIndex(0); //reload sections

	for (uint32_t i = 0; i < sections.size(); i++) {
		if (iconpath == getSectionIcon(i)) {
			return; // icon in use by another section; return here.
		}
	}

	gmenu2x->sc.del(iconpath);
}

bool Menu::linkChangeSection(uint32_t linkIndex, uint32_t oldSectionIndex, uint32_t newSectionIndex) {
	if (oldSectionIndex < sections.size() && newSectionIndex < sections.size() && linkIndex < sectionLinks(oldSectionIndex)->size()) {
		sectionLinks(newSectionIndex)->push_back(sectionLinks(oldSectionIndex)->at(linkIndex));
		sectionLinks(oldSectionIndex)->erase(sectionLinks(oldSectionIndex)->begin() + linkIndex);
		// Select the same link in the new position
		setSectionIndex(newSectionIndex);
		setLinkIndex(sectionLinks(newSectionIndex)->size() - 1);
		return true;
	}
	return false;
}

void Menu::pageUp() {
	// PAGEUP with left
	int i = iLinkIndex - linkRows;
	if (i < 0) i = 0;
	setLinkIndex(i);
}

void Menu::pageDown() {
	// PAGEDOWN with right
	int i = iLinkIndex + linkRows;
	if (i >= sectionLinks()->size()) i = sectionLinks()->size() - 1;
	setLinkIndex(i);
}

void Menu::linkLeft() {
	setLinkIndex(iLinkIndex - 1);
}

void Menu::linkRight() {
	setLinkIndex(iLinkIndex + 1);
}

void Menu::linkUp() {
	int l = iLinkIndex - linkCols;
	if (l < 0) {
		uint32_t rows = (uint32_t)ceil(sectionLinks()->size() / (double)linkCols);
		l += (rows * linkCols);
		if (l >= (int)sectionLinks()->size()) {
			l -= linkCols;
		}
	}
	setLinkIndex(l);
}

void Menu::linkDown() {
	uint32_t l = iLinkIndex + linkCols;
	if (l >= sectionLinks()->size()) {
		uint32_t rows = (uint32_t)ceil(sectionLinks()->size() / (double)linkCols);
		uint32_t curCol = (uint32_t)ceil((iLinkIndex+1) / (double)linkCols);
		if (rows > curCol) {
			l = sectionLinks()->size() - 1;
		} else {
			l %= linkCols;
		}
	}
	setLinkIndex(l);
}

Link *Menu::getLink() {
	if (sectionLinks()->size() == 0) {
		return NULL;
	}

	return sectionLinks()->at(iLinkIndex);
}

LinkApp *Menu::getLinkApp() {
	return dynamic_cast<LinkApp*>(getLink());
}

void Menu::setLinkIndex(int i) {
	if (i < 0) {
		i = sectionLinks()->size() - 1;
	} else if (i >= (int)sectionLinks()->size()) {
		i = 0;
	}

	int perPage = linkCols * linkRows;
	int page = i / linkCols;
	if (i < firstDispLink) {
		firstDispLink = page * linkCols;
	} else if (i >= firstDispLink + perPage) {
		firstDispLink = page * linkCols - linkCols * (linkRows - 1);
	}

	if (firstDispLink < 0) {
		firstDispLink = 0;
	}

	iLinkIndex = i;
}

void Menu::renameSection(int index, const string &name) {
	// section directory doesn't exists
	string oldsection = home_path("sections/") + getSection();
	string newsection = home_path("sections/") + name;

	if (oldsection != newsection && rename(oldsection.c_str(), newsection.c_str()) == 0) {
		sections[index] = name;
	}

}

int Menu::getSectionIndexByName(const string &name) {
	return distance(sections.begin(), find(sections.begin(), sections.end(), name));
}

const string Menu::getSectionIcon(int i) {
	if (i < 0) i = iSectionIndex;

	bool fallback_to_default = gmenu2x->confStr["skin"] == home_path("skins/Default");

	string sectionIcon = gmenu2x->sc.getSkinFilePath("sections/" + sections[i] + ".png", fallback_to_default);
	if (!sectionIcon.empty()) {
		return sectionIcon;
	}

	string::size_type pos = sections[i].rfind(".");
	if (pos != string::npos) {
		sectionIcon = gmenu2x->sc.getSkinFilePath("sections/" + sections[i].substr(pos) + ".png", fallback_to_default);
		if (!sectionIcon.empty()) {
			return sectionIcon;
		}
	}

	pos = sections[i].find(".");
	if (pos != string::npos) {
		sectionIcon = gmenu2x->sc.getSkinFilePath("sections/" + sections[i].substr(0, pos) + ".png", fallback_to_default);
		if (!sectionIcon.empty()) {
			return sectionIcon;
		}
	}

	return gmenu2x->sc.getSkinFilePath("icons/section.png");
}

void Menu::initLayout() {
	// LINKS rect
	gmenu2x->linksRect = (SDL_Rect){0, 0, gmenu2x->platform->w, gmenu2x->platform->h};
	gmenu2x->sectionBarRect = (SDL_Rect){0, 0, gmenu2x->platform->w, gmenu2x->platform->h};

	if (gmenu2x->skinConfInt["sectionBar"]) {
		if (gmenu2x->skinConfInt["sectionBar"] == SB_LEFT || gmenu2x->skinConfInt["sectionBar"] == SB_RIGHT) {
			gmenu2x->sectionBarRect.x = (gmenu2x->skinConfInt["sectionBar"] == SB_RIGHT) * (gmenu2x->platform->w - gmenu2x->skinConfInt["sectionBarSize"]);
			gmenu2x->sectionBarRect.w = gmenu2x->skinConfInt["sectionBarSize"];
			gmenu2x->linksRect.w = gmenu2x->platform->w - gmenu2x->skinConfInt["sectionBarSize"];

			if (gmenu2x->skinConfInt["sectionBar"] == SB_LEFT) {
				gmenu2x->linksRect.x = gmenu2x->skinConfInt["sectionBarSize"];
			}
		} else {
			gmenu2x->sectionBarRect.y = (gmenu2x->skinConfInt["sectionBar"] == SB_BOTTOM) * (gmenu2x->platform->h - gmenu2x->skinConfInt["sectionBarSize"]);
			gmenu2x->sectionBarRect.h = gmenu2x->skinConfInt["sectionBarSize"];
			gmenu2x->linksRect.h = gmenu2x->platform->h - gmenu2x->skinConfInt["sectionBarSize"];

			if (gmenu2x->skinConfInt["sectionBar"] == SB_TOP || gmenu2x->skinConfInt["sectionBar"] == SB_CLASSIC) {
				gmenu2x->linksRect.y = gmenu2x->skinConfInt["sectionBarSize"];
			}
			if (gmenu2x->skinConfInt["sectionBar"] == SB_CLASSIC) {
				gmenu2x->linksRect.h -= gmenu2x->skinConfInt["bottomBarHeight"];
			}
		}
	}

	gmenu2x->listRect = (SDL_Rect){0, gmenu2x->skinConfInt["sectionBarSize"], gmenu2x->platform->w, gmenu2x->platform->h - gmenu2x->skinConfInt["bottomBarHeight"] - gmenu2x->skinConfInt["sectionBarSize"]};
	gmenu2x->bottomBarRect = (SDL_Rect){0, gmenu2x->platform->h - gmenu2x->skinConfInt["bottomBarHeight"], gmenu2x->platform->w, gmenu2x->skinConfInt["bottomBarHeight"]};

	// WIP
	linkCols = gmenu2x->skinConfInt["linkCols"];
	linkRows = gmenu2x->skinConfInt["linkRows"];

	linkWidth  = (gmenu2x->linksRect.w - (linkCols + 1) * linkSpacing) / linkCols;
	linkHeight = (gmenu2x->linksRect.h - (linkCols > 1) * (linkRows + 1) * linkSpacing) / linkRows;
}

string Menu::getBatteryIcon(uint8_t level) {
	if (level <= 5) {
		string path = gmenu2x->sc.getSkinFilePath("imgs/battery/" +  std::to_string(level) + ".png");
		if (!path.empty()) return path;
	}
 	return gmenu2x->sc.getSkinFilePath("imgs/battery/ac.png");
}

string Menu::getBrightnessIcon(uint8_t level) {
	level /= 20;
	if (level <= 4) {
		string path = gmenu2x->sc.getSkinFilePath("imgs/brightness/" +  std::to_string(level) + ".png");
		if (!path.empty()) {
			return path;
		}
	}
	return gmenu2x->sc.getSkinFilePath("imgs/brightness.png");
}

string Menu::getVolumeIcon(uint8_t level) {
	switch (level) {
		case VOLUME_MODE_MUTE:
			return gmenu2x->sc.getSkinFilePath("imgs/mute.png");
		case VOLUME_MODE_PHONES:
			return gmenu2x->sc.getSkinFilePath("imgs/phones.png");
	}
	return gmenu2x->sc.getSkinFilePath("imgs/volume.png");
}

void Menu::drawIcon(int i, int ix, int iy, bool selected) {
	Surface *icon = gmenu2x->sc[sectionLinks()->at(i)->getIconPath()];

	if (icon == NULL) {
		icon = gmenu2x->sc["skin:icons/generic.png"];
	}

	if (icon->width() > linkWidth || icon->height() > linkHeight) {
		icon->softStretch(linkWidth, linkHeight, SScaleFit);
	}

	if (selected) {
		if (iconBGon != NULL && icon->width() <= iconBGon->width() && icon->height() <= iconBGon->height()) {
			iconBGon->blit(gmenu2x->s, ix + (linkWidth + iconPadding) / 2, iy + (linkHeight + iconPadding) / 2, HAlignCenter | VAlignMiddle, 50);
		} else {
			gmenu2x->s->box(ix + (linkWidth - min(linkWidth, icon->width())) / 2 - 4, iy + (linkHeight - min(linkHeight, icon->height())) / 2 - 4, min(linkWidth, icon->width()) + 8, min(linkHeight, icon->height()) + 8, gmenu2x->skinConfColor["selectionBg"]);
		}

	} else if (iconBGoff != NULL && icon->width() <= iconBGoff->width() && icon->height() <= iconBGoff->height()) {
		iconBGoff->blit(gmenu2x->s, {ix + iconPadding/2, iy + iconPadding/2, linkWidth - iconPadding, linkHeight - iconPadding}, HAlignCenter | VAlignMiddle);
	}

	icon->blit(gmenu2x->s, {ix + iconPadding / 2, iy + iconPadding/2, linkWidth - iconPadding, linkHeight - iconPadding}, HAlignCenter | VAlignMiddle);

	if (gmenu2x->skinConfInt["linkLabel"]) {
		SDL_Rect labelRect;
		labelRect.x = ix + 2 + linkWidth / 2;
		labelRect.y = iy + (linkHeight + min(linkHeight, icon->height())) / 2;
		labelRect.w = linkWidth - iconPadding;
		labelRect.h = linkHeight - iconPadding;
		gmenu2x->s->write(gmenu2x->font, gmenu2x->tr[sectionLinks()->at(i)->getTitle()], labelRect, HAlignCenter | VAlignMiddle);
	}
}

void Menu::drawCoverFlow() {
	int iy = gmenu2x->linksRect.y;

	for (int x = linkCols/2 - 1, i = iLinkIndex - 1; x > -1 ; x--, i--) {
		if (sectionLinks()->size() <= 1) continue;

		if (i < 0) i = sectionLinks()->size() - 1;
		else if (i >= (int)sectionLinks()->size()) i = 0;

		int ix = (gmenu2x->linksRect.w + gmenu2x->linksRect.x) / 2
				- (linkWidth + linkSpacing) / 2
				- ((linkCols / 2)) * (linkWidth + linkSpacing)
				+ x * linkWidth
				+ (x + 1) * linkSpacing;

		drawIcon(i, ix, iy, false);
	}

	for (int x = 0, i = iLinkIndex; x <= linkCols/2; x++, i++) {
		if (i < 0) i = sectionLinks()->size() - 1;
		else if (i >= (int)sectionLinks()->size()) i = 0;

		int ix = (gmenu2x->linksRect.w + gmenu2x->linksRect.x) / 2
				- (linkWidth + linkSpacing) / 2
				// - ((linkCols / 2)) * (linkWidth + linkSpacing)
				+ x * linkWidth
				+ (x + 1) * linkSpacing;

		Surface *icon = gmenu2x->sc[sectionLinks()->at(i)->getIconPath()];

		drawIcon(i, ix, iy, (x == 0));

		if (sectionLinks()->size() <= 1) return;
	}

	gmenu2x->drawScrollBar(1, sectionLinks()->size(), iLinkIndex, gmenu2x->linksRect, VAlignBottom);
}

void Menu::drawList() {
	int ix = gmenu2x->linksRect.x;

	for (int y = 0, i = firstDispLink; y < linkRows && i < sectionLinks()->size(); y++, i++) {
		int iy = gmenu2x->linksRect.y + y * linkHeight;

		if (i == iLinkIndex) {
			gmenu2x->s->box(ix, iy, gmenu2x->linksRect.w, linkHeight, gmenu2x->skinConfColor["selectionBg"]);
		}

		Surface *icon = gmenu2x->sc[sectionLinks()->at(i)->getIconPath()];
		if (icon == NULL) {
			icon = gmenu2x->sc["skin:icons/generic.png"];
		}

		if (icon->width() > 32 || icon->height() > linkHeight - 4) {
			icon->softStretch(32, linkHeight - 4, SScaleFit);
		}

		icon->blit(gmenu2x->s, {ix + 2, iy + 2, 32, linkHeight - 4}, HAlignCenter | VAlignMiddle);
		gmenu2x->s->write(gmenu2x->titlefont, gmenu2x->tr[sectionLinks()->at(i)->getTitle()], ix + linkSpacing + 36, iy + gmenu2x->titlefont->height()/2, VAlignMiddle);
		gmenu2x->s->write(gmenu2x->font, gmenu2x->tr[sectionLinks()->at(i)->getDescription()], ix + linkSpacing + 36, iy + linkHeight - linkSpacing/2, VAlignBottom);
	}

	if (sectionLinks()->size() > linkRows) {
		gmenu2x->drawScrollBar(1, sectionLinks()->size(), iLinkIndex, gmenu2x->linksRect, HAlignRight);
	}
}

void Menu::drawGrid() {
	int i = firstDispLink;

	for (int y = 0; y < linkRows; y++) {
		for (int x = 0; x < linkCols && i < sectionLinks()->size(); x++, i++) {
			int ix = gmenu2x->linksRect.x + x * linkWidth  + (x + 1) * linkSpacing;
			int iy = gmenu2x->linksRect.y + y * linkHeight + (y + 1) * linkSpacing;

			drawIcon(i, ix, iy, (i == iLinkIndex));
		}
	}

	gmenu2x->drawScrollBar(1, sectionLinks()->size()/linkCols + 1, iLinkIndex/linkCols, gmenu2x->linksRect, HAlignRight);
}

void Menu::drawSectionBar() {
	string barbg = "";

	switch (gmenu2x->skinConfInt["sectionBar"]) {
		case SB_LEFT:
			barbg = "imgs/leftbar.png";
			break;
		case SB_BOTTOM:
			barbg = "imgs/bottombar.png";
			break;
		case SB_RIGHT:
			barbg = "imgs/rightbar.png";
			break;
		case SB_TOP:
		case SB_CLASSIC:
			barbg = "imgs/topbar.png";
			break;
	}

	Surface *bar = gmenu2x->sc[gmenu2x->sc.getSkinFilePath(barbg, false)];
	if (bar != NULL) {
		bar->blit(gmenu2x->s, gmenu2x->sectionBarRect, HAlignCenter | VAlignMiddle);
	} else {
		gmenu2x->s->box(gmenu2x->sectionBarRect, gmenu2x->skinConfColor["topBarBg"]);
	}

	int ix = 0, iy = 0, sy = 0;
	int x = gmenu2x->sectionBarRect.x;
	int y = gmenu2x->sectionBarRect.y;
	int sx = (iSectionIndex - firstDispSection) * gmenu2x->skinConfInt["sectionBarSize"];

	if (gmenu2x->skinConfInt["sectionBar"] == SB_CLASSIC) {
		ix = (gmenu2x->platform->w - gmenu2x->skinConfInt["sectionBarSize"] * min(sectionNumItems(), getSections().size())) / 2;
	}

	for (int i = firstDispSection; i < getSections().size() && i < firstDispSection + sectionNumItems(); i++) {
		if (gmenu2x->skinConfInt["sectionBar"] == SB_LEFT || gmenu2x->skinConfInt["sectionBar"] == SB_RIGHT) {
			y = (i - firstDispSection) * gmenu2x->skinConfInt["sectionBarSize"];
		} else {
			x = (i - firstDispSection) * gmenu2x->skinConfInt["sectionBarSize"] + ix;
		}

		if (i == iSectionIndex) {
			sx = x;
			sy = y;
			gmenu2x->s->box(sx, sy, gmenu2x->skinConfInt["sectionBarSize"], gmenu2x->skinConfInt["sectionBarSize"], gmenu2x->skinConfColor["selectionBg"]);
		}

		gmenu2x->sc[getSectionIcon(i)]->blit(gmenu2x->s, {x, y, gmenu2x->skinConfInt["sectionBarSize"], gmenu2x->skinConfInt["sectionBarSize"]}, HAlignCenter | VAlignMiddle);
	}

	if (gmenu2x->skinConfInt["sectionLabel"] && SDL_GetTicks() - section_changed < 1400) {
		if (gmenu2x->skinConfInt["sectionBar"] == SB_LEFT) {
			gmenu2x->s->write(gmenu2x->font, gmenu2x->tr[getSectionName()], sx, sy + gmenu2x->skinConfInt["sectionBarSize"], HAlignLeft | VAlignBottom);
		} else if (gmenu2x->skinConfInt["sectionBar"] == SB_RIGHT) {
			gmenu2x->s->write(gmenu2x->font, gmenu2x->tr[getSectionName()], sx + gmenu2x->skinConfInt["sectionBarSize"], sy + gmenu2x->skinConfInt["sectionBarSize"], HAlignRight | VAlignBottom);
		} else {
			gmenu2x->s->write(gmenu2x->font, gmenu2x->tr[getSectionName()], sx + gmenu2x->skinConfInt["sectionBarSize"] / 2 , sy + gmenu2x->skinConfInt["sectionBarSize"], HAlignCenter | VAlignBottom);
		}
	} else {
		if(sectionChangedTimer)
			SDL_RemoveTimer(sectionChangedTimer);
		sectionChangedTimer = NULL;
	}

	if (gmenu2x->skinConfInt["sectionBar"] == SB_CLASSIC) {
		string iconL = gmenu2x->sc.getSkinFilePath("imgs/section-l.png", false);
		if (!iconL.empty()) gmenu2x->sc[iconL]->blit(gmenu2x->s, 0, 0, HAlignLeft | VAlignTop);

		string iconR = gmenu2x->sc.getSkinFilePath("imgs/section-r.png", false);
		if (!iconR.empty()) gmenu2x->sc[iconR]->blit(gmenu2x->s, gmenu2x->platform->w, 0, HAlignRight | VAlignTop);
	}
}

void Menu::drawStatusBar() {
	int iconTrayShift = 0;

	Surface *bar = gmenu2x->sc[gmenu2x->sc.getSkinFilePath("imgs/bottombar.png", false)];
	if (bar != NULL) {
		bar->blit(gmenu2x->s, gmenu2x->bottomBarRect, HAlignCenter | VAlignBottom);
	} else {
		gmenu2x->s->box(gmenu2x->bottomBarRect, gmenu2x->skinConfColor["bottomBarBg"]);
	}

	const int iconWidth = 16, pctWidth = gmenu2x->font->getTextWidth("100");
	char buf[32]; int x = 0;

	if (!iconDescription.empty() && SDL_GetTicks() - icon_changed < 300) {
		x = iconPadding;
		gmenu2x->sc["skin:imgs/manual.png"]->blit(gmenu2x->s, x, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, VAlignMiddle);
		x += iconWidth + iconPadding;
		gmenu2x->s->write(gmenu2x->font, iconDescription.c_str(), x, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, VAlignMiddle, gmenu2x->skinConfColor["fontAlt"], gmenu2x->skinConfColor["fontAltOutline"]);
	} else {
		if(iconChangedTimer)
			SDL_RemoveTimer(iconChangedTimer);
		iconChangedTimer = NULL;

		// TODO: use drawButton(gmenu2x->s, iconVolume[volume_mode], confInt["globalVolume"], x);

		// Volume indicator
		if (gmenu2x->platform->volume) {
			x = iconPadding; // 1 * (iconWidth + 2 * iconPadding) + iconPadding + 1 * pctWidth;
			gmenu2x->sc[getVolumeIcon(gmenu2x->input->volume_mode)]->blit(gmenu2x->s, x, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, VAlignMiddle);
			x += iconWidth + iconPadding;
			gmenu2x->s->write(gmenu2x->font, std::to_string(gmenu2x->confInt["globalVolume"]), x, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, VAlignMiddle, gmenu2x->skinConfColor["fontAlt"], gmenu2x->skinConfColor["fontAltOutline"]);
		}

		// Brightness indicator
		x += iconPadding + pctWidth;
		gmenu2x->sc[getBrightnessIcon(gmenu2x->confInt["backlight"])]->blit(gmenu2x->s, x, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, VAlignMiddle);
		x += iconWidth + iconPadding;
		gmenu2x->s->write(gmenu2x->font, std::to_string(gmenu2x->confInt["backlight"]), x, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, VAlignMiddle, gmenu2x->skinConfColor["fontAlt"], gmenu2x->skinConfColor["fontAltOutline"]);

		// // Menu indicator
		// iconMenu->blit(gmenu2x->s, iconPadding, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, VAlignMiddle);
		// sc["skin:imgs/debug.png"]->blit(gmenu2x->s, gmenu2x->bottomBarRect.w - iconTrayShift * (iconWidth + iconPadding) - iconPadding, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, HAlignRight | VAlignMiddle);


		// Battery indicator
		if (gmenu2x->platform->battery) {
			gmenu2x->sc[getBatteryIcon(gmenu2x->input->battery)]->blit(gmenu2x->s, gmenu2x->bottomBarRect.w - iconTrayShift * (iconWidth + iconPadding) - iconPadding, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, HAlignRight | VAlignMiddle);
			iconTrayShift++;
		}

		// SD Card indicator
		if (gmenu2x->input->mmc == MMC_INSERT) {
			gmenu2x->sc["skin:imgs/sd.png"]->blit(gmenu2x->s, gmenu2x->bottomBarRect.w - iconTrayShift * (iconWidth + iconPadding) - iconPadding, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, HAlignRight | VAlignMiddle);
			iconTrayShift++;
		}

		// Network indicator
		if (!gmenu2x->inetIcon.empty()) {
			gmenu2x->sc[gmenu2x->inetIcon]->blit(gmenu2x->s, gmenu2x->bottomBarRect.w - iconTrayShift * (iconWidth + iconPadding) - iconPadding, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, HAlignRight | VAlignMiddle);
			iconTrayShift++;
		}

		if (getLink() != NULL) {
			if (getLinkApp() != NULL) {
				if (!getLinkApp()->getManualPath().empty()) {
					// Manual indicator
					gmenu2x->sc["skin:imgs/manual.png"]->blit(gmenu2x->s, gmenu2x->bottomBarRect.w - iconTrayShift * (iconWidth + iconPadding) - iconPadding, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, HAlignRight | VAlignMiddle);
				}

				if (gmenu2x->platform->cpu_max != gmenu2x->platform->cpu_min) {
					// CPU indicator
					x += iconPadding + pctWidth;
					gmenu2x->sc["skin:imgs/cpu.png"]->blit(gmenu2x->s, x, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, VAlignMiddle);
					x += iconWidth + iconPadding;
					gmenu2x->s->write(gmenu2x->font, std::to_string(getLinkApp()->getCPU()) + "MHz", x, gmenu2x->bottomBarRect.y + gmenu2x->bottomBarRect.h / 2, VAlignMiddle, gmenu2x->skinConfColor["fontAlt"], gmenu2x->skinConfColor["fontAltOutline"]);
				}
			}
		}
	}
}

void Menu::drawIconTray() {
	int iconTrayShift = 0;

	// TRAY DEBUG
	// s->box(sectionBarRect.x + gmenu2x->sectionBarRect.w - 38 + 0 * 20, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 18,16,16, strtorgba("ffff00ff"));
	// s->box(sectionBarRect.x + gmenu2x->sectionBarRect.w - 38 + 1 * 20, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 18,16,16, strtorgba("00ff00ff"));
	// s->box(sectionBarRect.x + gmenu2x->sectionBarRect.w - 38, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 38,16,16, strtorgba("0000ffff"));
	// s->box(sectionBarRect.x + gmenu2x->sectionBarRect.w - 18, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 38,16,16, strtorgba("ff00ffff"));

	// TRAY 0,0
	// Volume indicator
	if (gmenu2x->platform->volume) {
		gmenu2x->sc[getVolumeIcon(gmenu2x->input->volume_mode)]->blit(gmenu2x->s, gmenu2x->sectionBarRect.x + gmenu2x->sectionBarRect.w - 38, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 38);
	}

	// Battery indicator
	if (gmenu2x->platform->battery) {
		// TRAY 1,0
		gmenu2x->sc[getBatteryIcon(gmenu2x->input->battery)]->blit(gmenu2x->s, gmenu2x->sectionBarRect.x + gmenu2x->sectionBarRect.w - 18, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 38);
	}

	// TRAY iconTrayShift,1
	if (gmenu2x->input->mmc == MMC_INSERT) {
		gmenu2x->sc["skin:imgs/sd.png"]->blit(gmenu2x->s, gmenu2x->sectionBarRect.x + gmenu2x->sectionBarRect.w - 38 + iconTrayShift * 20, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 18);
		iconTrayShift++;
	}

	if (getLink() != NULL) {
		if (getLinkApp() != NULL) {
			if (!getLinkApp()->getManualPath().empty() && iconTrayShift < 2) {
				// Manual indicator
				gmenu2x->sc["skin:imgs/manual.png"]->blit(gmenu2x->s, gmenu2x->sectionBarRect.x + gmenu2x->sectionBarRect.w - 38 + iconTrayShift * 20, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 18);
				iconTrayShift++;
			}

			if (gmenu2x->platform->cpu_max != gmenu2x->platform->cpu_min) {
				if (getLinkApp()->getCPU() != gmenu2x->platform->cpu_menu && iconTrayShift < 2) {
					// CPU indicator
					gmenu2x->sc["skin:imgs/cpu.png"]->blit(gmenu2x->s, gmenu2x->sectionBarRect.x + gmenu2x->sectionBarRect.w - 38 + iconTrayShift * 20, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 18);
					iconTrayShift++;
				}
			}
		}
	}

	if (iconTrayShift < 2) {
		gmenu2x->sc[getBrightnessIcon(gmenu2x->confInt["backlight"])]->blit(gmenu2x->s, gmenu2x->sectionBarRect.x + gmenu2x->sectionBarRect.w - 38 + iconTrayShift * 20, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 18);
		iconTrayShift++;
	}

	if (iconTrayShift < 2) {
		// Menu indicator
		gmenu2x->sc["skin:imgs/menu.png"]->blit(gmenu2x->s, gmenu2x->sectionBarRect.x + gmenu2x->sectionBarRect.w - 38 + iconTrayShift * 20, gmenu2x->sectionBarRect.y + gmenu2x->sectionBarRect.h - 18);
		iconTrayShift++;
	}
}

void Menu::exec() {
	bool inputAction = false;
	icon_changed = SDL_GetTicks();
	section_changed = icon_changed;

	sectionChangedTimer = SDL_AddTimer(2000, gmenu2x->input->wakeUp, (void*)false);
	iconChangedTimer = SDL_AddTimer(1000, gmenu2x->input->wakeUp, (void*)false);

	iconBGoff = gmenu2x->sc[gmenu2x->sc.getSkinFilePath("imgs/iconbg_off.png", false)];
	iconBGon = gmenu2x->sc[gmenu2x->sc.getSkinFilePath("imgs/iconbg_on.png", false)];

	while (true) {
		// BACKGROUND
		gmenu2x->currBackdrop = gmenu2x->confStr["wallpaper"];
		if (gmenu2x->confInt["skinBackdrops"] & BD_MENU) {
			if (getLink() != NULL && !getLink()->getBackdropPath().empty()) {
				gmenu2x->currBackdrop = getLink()->getBackdropPath();
			} else if (getLinkApp() != NULL && !getLinkApp()->getBackdropPath().empty()) {
				gmenu2x->currBackdrop = getLinkApp()->getBackdropPath();
			}
		}
		gmenu2x->setBackground(gmenu2x->s, gmenu2x->currBackdrop);

		// SECTIONS
		if (gmenu2x->skinConfInt["sectionBar"]) {
			drawSectionBar();

			if (gmenu2x->skinConfInt["sectionBar"] == SB_CLASSIC) {
				drawStatusBar();
			} else {
				drawIconTray();
			}
		}

		// LINKS
		gmenu2x->s->setClipRect(gmenu2x->linksRect);
		gmenu2x->s->box(gmenu2x->linksRect, gmenu2x->skinConfColor["listBg"]);
		if (!sectionLinks()->size()) {
			MessageBox mb(gmenu2x, gmenu2x->tr["This section is empty"]);
			mb.setAutoHide(1);
			mb.setBgAlpha(0);
			mb.exec();
		} else if (linkCols == 1 && linkRows > 1) {
			drawList(); // LIST
		} else if (linkRows == 1) {
			drawCoverFlow(); // COVER FLOW
		} else {
			drawGrid(); // CLASSIC
		}
		gmenu2x->s->clearClipRect();

		if (!gmenu2x->powerManager->suspendActive && !gmenu2x->input->combo()) {
			gmenu2x->s->flip();
		}

		do {
			inputAction = gmenu2x->input->update();
		} while (!inputAction);

		if (gmenu2x->input->combo()) {
			gmenu2x->skinConfInt["sectionBar"] = ((gmenu2x->skinConfInt["sectionBar"]) % 5) + 1;
			initLayout();
			MessageBox mb(gmenu2x, "CHEATER! ;)");
			mb.setBgAlpha(0);
			mb.setAutoHide(200);
			mb.exec();
			gmenu2x->input->dropEvents();
			SDL_AddTimer(350, gmenu2x->input->wakeUp, (void*)false);
		} else if (!gmenu2x->powerManager->suspendActive) {
			gmenu2x->s->flip();
		}

		if (gmenu2x->inputCommonActions(inputAction)) {
			continue;
		}

		if (gmenu2x->input->isActive(CANCEL) || gmenu2x->input->isActive(CONFIRM) || gmenu2x->input->isActive(SETTINGS) || gmenu2x->input->isActive(MENU)) {
			if(sectionChangedTimer)
				SDL_RemoveTimer(sectionChangedTimer);
			sectionChangedTimer = NULL;
			if(iconChangedTimer)
				SDL_RemoveTimer(iconChangedTimer);
			iconChangedTimer = NULL;
			icon_changed = section_changed = 0;
		}

		if (gmenu2x->input->isActive(CONFIRM) && getLink() != NULL) {
			if (gmenu2x->confInt["skinBackdrops"] & BD_DIALOG) {
				gmenu2x->setBackground(gmenu2x->bg, gmenu2x->currBackdrop);
			} else {
				gmenu2x->setBackground(gmenu2x->bg, gmenu2x->confStr["wallpaper"]);
			}

			getLink()->run();
		}
		else if (gmenu2x->input->isActive(CANCEL))	continue;
		else if (gmenu2x->input->isActive(SETTINGS))	gmenu2x->settings();
		else if (gmenu2x->input->isActive(MENU))		gmenu2x->contextMenu();

		// LINK NAVIGATION
		else if (gmenu2x->input->isActive(LEFT)  && linkCols == 1 && linkRows > 1) pageUp();
		else if (gmenu2x->input->isActive(RIGHT) && linkCols == 1 && linkRows > 1) pageDown();
		else if (gmenu2x->input->isActive(LEFT))	linkLeft();
		else if (gmenu2x->input->isActive(RIGHT))	linkRight();
		else if (gmenu2x->input->isActive(UP))	linkUp();
		else if (gmenu2x->input->isActive(DOWN))	linkDown();

		// SECTION
		else if (gmenu2x->input->isActive(SECTION_PREV)) setSectionIndex(iSectionIndex - 1);
		else if (gmenu2x->input->isActive(SECTION_NEXT)) setSectionIndex(iSectionIndex + 1);

		// SELLINKAPP SELECTED
		else if (gmenu2x->input->isActive(MANUAL) && getLinkApp() != NULL && !getLinkApp()->getManualPath().empty()) gmenu2x->showManual();
		// On Screen Help
		// else if (gmenu2x->input->isActive(MANUAL)) {
		// 	s->box(10,50,300,162, gmenu2x->skinConfColors[COLOR_MESSAGE_BOX_BG]);
		// 	s->rectangle(12,52,296,158, gmenu2x->skinConfColors[COLOR_MESSAGE_BOX_BORDER]);
		// 	int line = 60; s->write(gmenu2x->font, gmenu2x->tr["CONTROLS"], 20, line);
		// 	line += font->height() + 5; s->write(gmenu2x->font, gmenu2x->tr["A: Select"], 20, line);
		// 	line += font->height() + 5; s->write(gmenu2x->font, gmenu2x->tr["B: Cancel"], 20, line);
		// 	line += font->height() + 5; s->write(gmenu2x->font, gmenu2x->tr["Y: Show manual"], 20, line);
		// 	line += font->height() + 5; s->write(gmenu2x->font, gmenu2x->tr["L, R: Change section"], 20, line);
		// 	line += font->height() + 5; s->write(gmenu2x->font, gmenu2x->tr["Start: Settings"], 20, line);
		// 	line += font->height() + 5; s->write(gmenu2x->font, gmenu2x->tr["Select: Menu"], 20, line);
		// 	line += font->height() + 5; s->write(gmenu2x->font, gmenu2x->tr["Select+Start: Save screenshot"], 20, line);
		// 	line += font->height() + 5; s->write(gmenu2x->font, gmenu2x->tr["Select+L: Adjust volume level"], 20, line);
		// 	line += font->height() + 5; s->write(gmenu2x->font, gmenu2x->tr["Select+R: Adjust backlight level"], 20, line);
		// 	s->flip();
		// 	bool close = false;
		// 	while (!close) {
		// 		gmenu2x->input->update();
		// 		if (gmenu2x->input->isActive(MODIFIER) || gmenu2x->input->isActive(CONFIRM) || gmenu2x->input->isActive(CANCEL)) close = true;
		// 	}
		// }

		iconDescription = "";
		if (getLinkApp() != NULL) {
			iconDescription = getLinkApp()->getDescription();
		} else if (getLink() != NULL) {
			iconDescription = getLink()->getDescription();
		}

		if (
			!iconDescription.empty() &&
			(gmenu2x->input->isActive(LEFT) || gmenu2x->input->isActive(RIGHT) || gmenu2x->input->isActive(LEFT) || gmenu2x->input->isActive(RIGHT) || gmenu2x->input->isActive(UP) || gmenu2x->input->isActive(DOWN) || gmenu2x->input->isActive(SECTION_PREV) || gmenu2x->input->isActive(SECTION_NEXT))
		) {
			icon_changed = SDL_GetTicks();
			if(iconChangedTimer)
				SDL_RemoveTimer(iconChangedTimer);
			iconChangedTimer = SDL_AddTimer(1000, gmenu2x->input->wakeUp, (void*)false);
		}

		if (gmenu2x->skinConfInt["sectionLabel"] && (gmenu2x->input->isActive(SECTION_PREV) || gmenu2x->input->isActive(SECTION_NEXT))) {
			section_changed = SDL_GetTicks();
			if(sectionChangedTimer)
				SDL_RemoveTimer(sectionChangedTimer);
			sectionChangedTimer = SDL_AddTimer(2000, gmenu2x->input->wakeUp, (void*)false);
		}
	}
}
