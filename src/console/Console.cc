// $Id$

#include "Console.hh"
#include "OSDConsoleRenderer.hh"

namespace openmsx {

Console::Console ()
{
}

Console::~Console()
{
}

void Console::registerConsole(OSDConsoleRenderer *console)
{
	renderers.push_back(console);
}

void Console::unregisterConsole(OSDConsoleRenderer *console)
{
	renderers.remove(console);
}

void Console::updateConsole()
{
	for (list<OSDConsoleRenderer*>::iterator it = renderers.begin();
	     it != renderers.end();
	     ++it) {
		(*it)->updateConsole();
	}
}

void Console::setFont(const string& font)
{
	this->font = font;
}

const string& Console::getFont() const
{
	return font;
}

void Console::setBackground(const string& background)
{
	this->background = background;
}

const string& Console::getBackground() const
{
	return background;
}

void Console::setPlacement(Placement placement)
{
	this->placement = placement;
}

Console::Placement Console::getPlacement() const
{
	return placement;
}

void Console::setColumns(unsigned columns)
{
	this->columns = columns;
}

unsigned Console::getColumns() const
{
	return columns;
}

void Console::setRows(unsigned rows)
{
	this->rows = rows;
}

unsigned Console::getRows() const
{
	return rows;
}



} // namespace openmsx
