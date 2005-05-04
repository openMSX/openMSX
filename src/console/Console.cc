// $Id$

#include "Console.hh"

namespace openmsx {

Console::Console()
{
}

Console::~Console()
{
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
