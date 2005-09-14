// $Id$

#include "RawFrame.hh"
#include <cstring>
#include <cstdlib>

namespace openmsx {

RawFrame::RawFrame(unsigned bytesPerPixel, unsigned maxWidth_, unsigned height_)
	: maxWidth(maxWidth_)
	, height(height_)
{
	lineWidth = new unsigned[height];
	
	// Allocate memory, make sure each line begins at a 64 byte boundary
	//  SSE instruction need 16 byte alligned data
	//  cache line size on athlon and P4 CPUs is 64 bytes
	pitch = ((bytesPerPixel * maxWidth) + 63) & ~63;
	unallignedData = malloc(pitch * height + 63);
	data = (char*)(((unsigned)unallignedData + 63) & ~63);

	// Start with a black frame.
	init(FIELD_NONINTERLACED);
	for (unsigned line = 0; line < height; line++) {
		// Make first two pixels black.
		memset(getLinePtrImpl(line), 0, bytesPerPixel * 2);
		// Mark line as border.
		lineWidth[line] = 0;
	}
}

RawFrame::~RawFrame()
{
	free(unallignedData);
	delete[] lineWidth;
}

RawFrame::FieldType RawFrame::getField()
{
	return field;
}

unsigned RawFrame::getLineWidth(unsigned line)
{
	assert(line < height);
	return lineWidth[line];
}

void RawFrame::init(FieldType field)
{
	this->field = field;
}

void* RawFrame::getLinePtrImpl(unsigned line)
{
	return data + line * pitch;
}

} // namespace openmsx
