// $Id$

#ifndef __DEBUGVIEW_HH__
#define __DEBUGVIEW_HH__

#include "openmsx.hh"
#include <string>
#include <vector>

namespace openmsx {

class DebugView
{
	public:
		DebugView (int rows_, int columns_, bool border_);
		std::string getLine (int line);
		virtual ~DebugView ();
		virtual void resize (int rows, int columns);
		virtual void update()=0;
		virtual void fill ()=0;
	private:
		int cursorX;
		int cursorY;
	protected:
		std::vector<std::string> lines;
		int rows;
		int columns;
		bool border;
		void setCursor (int rows_, int columns_);
		void getCursor (int *rows_, int *columns_);
		void clear ();
		void deleteLine ();
		void addLine (std::string message="");

};

} // namespace openmsx

#endif
