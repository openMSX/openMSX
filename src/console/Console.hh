// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#include <list>

class ConsoleRenderer;

struct CursorXY {
	unsigned x;
	unsigned y;
};

class Console
{
	public:
		Console();
		virtual ~Console();
		/** Add a renderer for this console.
		  */
		void registerConsole(ConsoleRenderer *console);
		
		/** Remove a renderer for this console.
		  */
		void unregisterConsole(ConsoleRenderer *console);
		virtual int getScrollBack()=0;
		virtual const std::string &getLine(int line)=0;
		virtual bool isVisible()=0;
		virtual void getCursorPosition(int *xPosition, int *yPosition)=0;
		virtual void setCursorPosition(int xPosition, int yPosition)=0;
		virtual void setCursorPosition(CursorXY pos)=0;
		virtual void setConsoleDimensions(int columns, int rows)=0;
	protected:
		void updateConsole();
	private:
		std::list<ConsoleRenderer*> renderers;
};





#endif
