// $Id$

#include "DebugView.hh"
#include "DisAsmView.hh"
#include "DebugConsole.hh"
#include "CommandConsole.hh"
#include "DumpView.hh"
#include "MSXConfig.hh"

namespace openmsx {

DebugConsole::ViewStruct::~ViewStruct()
{
	delete view;
}

DebugConsole* DebugConsole::instance()
{
	if (!MSXConfig::instance()->hasConfigWithId("Debugger")) return NULL;
	static DebugConsole oneInstance;
	return &oneInstance;
}

DebugConsole::DebugConsole()
	: debuggerSetting("debugger", "turns the debugger on or off", false)
{
	for (int i = 0; i < 20; ++i) {
		lines.push_back(" ");
	}
	addView (0, 0, 50, 6, DISSASVIEW);
}

DebugConsole::~DebugConsole()
{
	for (map<unsigned, ViewStruct*>::const_iterator it = viewList.begin();
	     it != viewList.end();
	     ++it) {
		delete it->second;
	}
}

void DebugConsole::update(const SettingLeafNode *setting)
{
}

bool DebugConsole::signalEvent(SDL_Event &event) throw()
{
	if (!isVisible()) {
		return true;
	}
	return false; // don't send anything to the MSX if visible
}

unsigned DebugConsole::addView(unsigned cursorX, unsigned cursorY,
                               unsigned columns, unsigned rows, ViewType viewType)
{
	ViewStruct* viewData = new ViewStruct();
	viewData->cursorX = cursorX;
	viewData->cursorY = cursorY;
	viewData->columns = columns;
	viewData->rows = rows;
	
	// determen view to create
	switch (viewType) {
	case DUMPVIEW:
		viewData->view = new DumpView(rows, columns, true);
		break;
	case DISSASVIEW:
		viewData->view = new DisAsmView(rows, columns, true);
		break;
	default:
		assert(false);
	}
	viewData->view->fill();
	
	// finally, determine which number to take for it
	unsigned id = 0;
	while (viewList.find(id) != viewList.end()) {
		++id;
	}
	viewList[id] = viewData;
	return id;
}

void DebugConsole::removeView(unsigned id)
{
	map<unsigned, ViewStruct*>::iterator it = viewList.find(id);
	if (it == viewList.end()) {
		assert(false);
	}
	delete it->second;
	viewList.erase(it);
}

void DebugConsole::buildLayout()
{
	for (map<unsigned, ViewStruct*>::const_iterator it = viewList.begin();
	     it != viewList.end();
	     ++it) {
		ViewStruct* view = it->second;
		for (unsigned i = view->cursorY; 
		     i < (view->cursorY + view->rows - 1);
		     ++i) {
			if (i < debugRows) {
				if (lines[debugRows - 1 - i].size() <
				    view->cursorX + view->columns) {
					string temp(view->cursorX + view->columns - 
					            lines[debugRows - 1 - i].size(), ' ');
					lines[debugRows - 1 - i] += temp;
				}
				lines[debugRows - 1 - i].replace(
				  view->cursorX, view->columns,
				  view->view->getLine(i - view->cursorY));
			}
		}
	}
}

void DebugConsole::loadLayout()
{
	// TODO
}

void DebugConsole::saveLayout()
{
	// TODO
}

void DebugConsole::resizeView(unsigned cursorX, unsigned cursorY,
                              unsigned columns, unsigned rows, unsigned id)
{
	map<unsigned, ViewStruct*>::iterator it = viewList.find(id);
	if (it == viewList.end()) {
		assert(false);
	}
	it->second->view->resize(columns, rows);
	it->second->cursorX = cursorX;
	it->second->cursorY = cursorY;
	buildLayout();
}

void DebugConsole::updateViews()
{
	for (map<unsigned, ViewStruct*>::const_iterator it = viewList.begin();
	     it != viewList.end();
	     ++it) {
		it->second->view->update();
	}
}

void DebugConsole::getCursorPosition(unsigned& xPosition, unsigned& yPosition) const
{
	xPosition = 0;
	yPosition = 0;
} 

void DebugConsole::setCursorPosition(unsigned xPosition, unsigned yPosition)
{
	// ignore
}

const string& DebugConsole::getLine(unsigned line) const
{
	static string EMPTY;
	return (unsigned)line < lines.size() ? lines[line] : EMPTY;
}

bool DebugConsole::isVisible() const
{
	return debuggerSetting.getValue();
}

void DebugConsole::setConsoleDimensions(unsigned columns, unsigned rows)
{
	debugColumns = columns;
	debugRows = rows;
	buildLayout();
}

const string& DebugConsole::getId() const
{
	static const string ID("debugger");
	return ID;
}

} // namespace openmsx
