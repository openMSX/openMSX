// $Id$

#ifndef __DEBUGCONSOLE_HH__
#define __DEBUGCONSOLE_HH__

#include "EventListener.hh"
#include "Settings.hh"
#include "Console.hh"
#include "Views.hh"
#include <map>

class DebugConsole : public Console, private EventListener, private SettingListener
{
	public:
		static DebugConsole * instance();
		~DebugConsole ();
		struct ViewStruct
		{
			int cursorX;
			int cursorY;
			int columns;
			int rows;
			DebugView * view;
		};
		enum ViewType {DUMPVIEW};
		int addView (int cursorX, int cursorY, int columns, int rows, DebugConsole::ViewType viewType);
		bool removeView (int id);
		void buildLayout ();
		void loadLayout();
		void saveLayout();
		void resizeView (int cursorX, int cursorY, int columns, int rows, int id);
		void updateViews();
		int getScrollBack(){return 0;}
		const std::string &getLine(int line);
		bool isVisible();
		void getCursorPosition(int *xPosition, int *yPosition);
		void setCursorPosition(int xPosition, int yPosition);
		void setCursorPosition(CursorXY pos);
		void setConsoleDimensions(int columns, int rows);

	private:
		DebugConsole ();
		void notify(Setting *setting);
		bool signalEvent(SDL_Event &event);
		std::map <int, struct DebugConsole::ViewStruct *> viewList;
		std::vector<std::string> lines;
		BooleanSetting debuggerSetting;
		int debugColumns;
		int debugRows;
};













#endif
