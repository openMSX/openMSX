// $Id$

#ifndef __DEBUGCONSOLE_HH__
#define __DEBUGCONSOLE_HH__

#include <map>
#include "EventListener.hh"
#include "Settings.hh"
#include "Console.hh"

using std::map;

namespace openmsx {

class DebugView;


class DebugConsole : public Console, private EventListener,
                     private SettingListener
{
	public:
		~DebugConsole();
		static DebugConsole* instance();
		
		struct ViewStruct {
			~ViewStruct();
			unsigned cursorX;
			unsigned cursorY;
			unsigned columns;
			unsigned rows;
			DebugView* view;
		};
		enum ViewType {DUMPVIEW, DISSASVIEW};
		
		unsigned addView(unsigned cursorX, unsigned cursorY,
		                 unsigned columns, unsigned rows, ViewType viewType);
		void removeView(unsigned id);
		void buildLayout();
		void loadLayout();
		void saveLayout();
		void resizeView(unsigned cursorX, unsigned cursorY,
		                unsigned columns, unsigned rows, unsigned id);
		void updateViews();

		virtual unsigned getScrollBack() const { return 0; }
		virtual const string& getLine(unsigned line) const;
		virtual bool isVisible() const;
		virtual void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const;
		virtual void setCursorPosition(unsigned xPosition, unsigned yPosition);
		virtual void setConsoleDimensions(unsigned columns, unsigned rows);
		virtual const string& getId() const;

	private:
		DebugConsole();
		void update(const SettingLeafNode *setting);
		bool signalEvent(SDL_Event &event);

		map<unsigned, ViewStruct*> viewList;
		vector<string> lines;
		BooleanSetting debuggerSetting;
		unsigned debugColumns;
		unsigned debugRows;
};

} // namespace openmsx

#endif
