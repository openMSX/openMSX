// $Id$

#include "DebugView.hh"
#include <iostream>
#include <cstdio>

namespace openmsx {

DebugView::DebugView (int rows_, int columns_, bool border_):
			rows(rows_),columns(columns_),border(border_)
{
	cursorX = cursorY = 0;
}

DebugView::~DebugView ()
{
}

void DebugView::resize(int rows_,int columns_)
{
	if ((rows_ < 1) || (columns_ < 1)) return;
	while (rows < rows_){
		deleteLine();
	}
	while (rows > rows_){
		addLine();
	}
	if (columns_ < columns){
		for (int i=0;i<rows;i++){
			lines[i]=lines[i].substr(0,columns_);
		}
	}
		columns = columns_;
	cursorX = (cursorX > columns ? columns : cursorX);
	cursorY = (cursorY > rows ? rows : cursorY);
	fill();
}

void DebugView::getCursor (int *cursorx, int *cursory)
{
	*cursorx = cursorX;
	*cursory = cursorY;
}

void DebugView::setCursor (const int cursorx, const int cursory)
{
	cursorX = cursorx;
	cursorY = cursory;
}

void DebugView::clear ()
{
	for (int i=0;i<rows;i++){
			lines[i]="";
	}
}

void DebugView::deleteLine()
{
	lines.pop_back();
}

void DebugView::addLine(std::string message)
{
	lines.push_back(message);
}

std::string DebugView::getLine (int line)
{
	return lines[line];
}

} // namespace openmsx
