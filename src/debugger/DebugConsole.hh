// $Id$

#ifndef __DEBUGCONSOLE_HH__
#define __DEBUGCONSOLE_HH__

#include <map>
#include "EventListener.hh"
#include "Settings.hh"
#include "Console.hh"

#include "DumpView.hh"
#include <map>



class DebugView;


class DebugConsole : public Console, private EventListener,
                     private SettingListener
{
	public:
		~DebugConsole ();
		static DebugConsole* instance();
		
		struct ViewStruct {
			int cursorX;
			int cursorY;
			int columns;
			int rows;
			DebugView* view;
		};
		enum ViewType {DUMPVIEW, DISSASVIEW};
		
		int addView(int cursorX, int cursorY, int columns, int rows,
		            ViewType viewType);
		bool removeView(int id);
		void buildLayout();
		void loadLayout();
		void saveLayout();
		void resizeView(int cursorX, int cursorY, int columns,
		                int rows, int id);
		void updateViews();
		int getScrollBack() { return 0; }
		const string& getLine(unsigned line);
		bool isVisible();
		void getCursorPosition(int *xPosition, int *yPosition);
		void setCursorPosition(int xPosition, int yPosition);
		void setCursorPosition(CursorXY pos);
		void setConsoleDimensions(int columns, int rows);
		std::string getId();

	private:
		DebugConsole();
		void update(const SettingLeafNode *setting);
		bool signalEvent(SDL_Event &event);

		map<int, ViewStruct*> viewList;
		vector<string> lines;
		BooleanSetting debuggerSetting;
		int debugColumns;
		int debugRows;
};

#endif
