// $Id$

#ifndef __DEBUGVIEW_HH__
#define __DEBUGVIEW_HH__

#include "openmsx.hh"
#include <string>
#include <vector>

using std::string;
using std::vector;

namespace openmsx {

class DebugView
{
	public:
		DebugView(unsigned rows, unsigned columns, bool border);
		virtual ~DebugView();

		const string& getLine(unsigned line) const;

		virtual void resize(unsigned rows, unsigned columns);
		virtual void update() = 0;
		virtual void fill() = 0;
		
	protected:
		void setCursor(unsigned rows, unsigned columns);
		void getCursor(unsigned& rows, unsigned& columns);
		void clear();
		void deleteLine();
		void addLine(const string& message = "");

		vector<string> lines;
		unsigned rows;
		unsigned columns;
		bool border;
	
	private:
		unsigned cursorX;
		unsigned cursorY;
};

} // namespace openmsx

#endif
