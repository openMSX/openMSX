// $Id$

#include "DebugView.hh"
#include <cstdio>

namespace openmsx {

DebugView::DebugView(unsigned rows_, unsigned columns_, bool border_)
	: rows(rows_), columns(columns_), border(border_)
{
	cursorX = cursorY = 0;
}

DebugView::~DebugView()
{
}

void DebugView::resize(unsigned rows_, unsigned columns_)
{
	if ((rows_ < 1) || (columns_ < 1)) {
		return;
	}
	while (rows < rows_) {
		deleteLine();
	}
	while (rows > rows_) {
		addLine();
	}
	if (columns_ < columns) {
		for (unsigned i = 0; i < rows; ++i) {
			lines[i] = lines[i].substr(0, columns_);
		}
	}
	columns = columns_;
	cursorX = (cursorX > columns ? columns : cursorX);
	cursorY = (cursorY > rows ? rows : cursorY);
	fill();
}

void DebugView::getCursor(unsigned& cursorx, unsigned& cursory)
{
	cursorx = cursorX;
	cursory = cursorY;
}

void DebugView::setCursor(unsigned cursorx, unsigned cursory)
{
	cursorX = cursorx;
	cursorY = cursory;
}

void DebugView::clear()
{
	for (unsigned i = 0; i < rows; ++i) {
		lines[i] = "";
	}
}

void DebugView::deleteLine()
{
	lines.pop_back();
}

void DebugView::addLine(const string& message)
{
	lines.push_back(message);
}

const string& DebugView::getLine(unsigned line) const
{
	return lines[line];
}

} // namespace openmsx
