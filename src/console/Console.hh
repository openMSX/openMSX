// $Id$

#ifndef __CONSOLE_HH__
#define __CONSOLE_HH__

#include <list>
#include <string>

namespace openmsx {

class OSDConsoleRenderer;

using std::list;
using std::string;


class Console
{
public:
	enum Placement {
		CP_TOPLEFT,    CP_TOP,    CP_TOPRIGHT,
		CP_LEFT,       CP_CENTER, CP_RIGHT,
		CP_BOTTOMLEFT, CP_BOTTOM, CP_BOTTOMRIGHT
	};

	Console();
	virtual ~Console();

	/** Add a renderer for this console.
	  */
	void registerConsole(OSDConsoleRenderer *console);
	
	/** Remove a renderer for this console.
	  */
	void unregisterConsole(OSDConsoleRenderer *console);

	void setFont(const string& font);
	const string& getFont() const;

	void setBackground(const string& background);
	const string& getBackground() const;

	void setPlacement(Placement placement);
	Placement getPlacement() const;

	void setColumns(unsigned columns);
	unsigned getColumns() const;

	void setRows(unsigned rows);
	unsigned getRows() const;
	
	virtual unsigned getScrollBack() const = 0;
	virtual const string& getLine(unsigned line) const = 0;
	virtual bool isVisible() const = 0;
	virtual void getCursorPosition(unsigned& xPosition, unsigned& yPosition) const = 0;
	virtual void setCursorPosition(unsigned xPosition, unsigned yPosition) = 0;
	virtual void setConsoleDimensions(unsigned columns, unsigned rows) = 0;
	virtual const string& getId() const = 0;

protected:
	void updateConsole();

private:
	string font;
	string background;
	Placement placement;
	unsigned columns;
	unsigned rows;
	
	list<OSDConsoleRenderer*> renderers;
};

} // namespace openmsx

#endif
