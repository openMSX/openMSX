// $Id$

#include "V9990GLRasterizer.hh"
#include "V9990.hh"

using std::string;

namespace openmsx {

V9990GLRasterizer::V9990GLRasterizer(V9990& vdp_)
	: vdp(vdp_), vram(vdp.getVRAM())
{
	PRT_DEBUG("V9990GLRasterizer::V9990GLRasterizer()");
}

V9990GLRasterizer::~V9990GLRasterizer()
{
	PRT_DEBUG("V9990GLRasterizer::~V9990GLRasterizer()");
}

bool V9990GLRasterizer::isActive()
{
	return false;
}

void V9990GLRasterizer::reset()
{
	PRT_DEBUG("V9990GLRasterizer::reset()");
}

void V9990GLRasterizer::frameStart()
{
	PRT_DEBUG("V9990GLRasterizer::frameStart()");
}

void V9990GLRasterizer::frameEnd()
{
	PRT_DEBUG("V9990GLRasterizer::frameEnd()");
}

void V9990GLRasterizer::setDisplayMode(V9990DisplayMode mode)
{
	if (mode); // avoid warning
	PRT_DEBUG("V9990GLRasterizer::setDisplayMode(" << std::dec <<
	          (int) mode << ")");
}

void V9990GLRasterizer::setColorMode(V9990ColorMode mode)
{
	if (mode); // avoid warning
	PRT_DEBUG("V9990GLRasterizer::setColorMode(" << std::dec <<
	          (int) mode << ")");
}

void V9990GLRasterizer::setPalette(int index, byte r, byte g, byte b)
{
	if (index | r | g | b); // avoid warning
	PRT_DEBUG("V9990GLRasterizer::setPalette(" << std::dec << index
	          << "," << (int) r << "," << (int) g << "," << (int) b << ")");
}

void V9990GLRasterizer::drawBorder(int fromX, int fromY, int toX, int toY)
{
	if (fromX | fromY | toX | toY); // avoid warning
	PRT_DEBUG("V9990GLRasterizer::drawBorder(" << std::dec <<
	          fromX << "," << fromY << "," << toX << "," << toY << ")");
}

void V9990GLRasterizer::drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight
		)
{
	if (fromX | fromY | displayX | displayY |
	    displayWidth  | displayHeight); // avoid warning
	PRT_DEBUG("V9990GLRasterizer::drawDisplay(" << std::dec <<
	          fromX << "," << fromY << "," <<
	          displayX << "," << displayY << "," <<
	          displayWidth << "," << displayHeight << ")");
}

} // namespace openmsx


