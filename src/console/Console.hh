// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#include "EventListener.hh"
#include <string>

using std::string;

namespace openmsx {

class OSDConsoleRenderer;

class Console: public EventListener
{
public:
	Console();
	virtual ~Console();

	void setColumns(unsigned columns);
	unsigned getColumns() const;

	void setRows(unsigned rows);
	unsigned getRows() const;
	
	virtual unsigned getScrollBack() const = 0;
	virtual const string& getLine(unsigned line) const = 0;
	virtual void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const = 0;
	virtual void setCursorPosition(unsigned xPosition, unsigned yPosition) = 0;
	virtual void setConsoleDimensions(unsigned columns, unsigned rows) = 0;

private:
	unsigned columns;
	unsigned rows;
};

} // namespace openmsx

#endif
