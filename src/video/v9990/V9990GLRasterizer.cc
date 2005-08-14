// $Id$

#include "V9990GLRasterizer.hh"
#include "V9990.hh"

using std::string;

namespace openmsx {

V9990GLRasterizer::V9990GLRasterizer(V9990& vdp_)
	: vdp(vdp_), vram(vdp.getVRAM())
{
}

V9990GLRasterizer::~V9990GLRasterizer()
{
}

bool V9990GLRasterizer::isActive()
{
	return false;
}

void V9990GLRasterizer::reset()
{
}

void V9990GLRasterizer::frameStart()
{
}

void V9990GLRasterizer::frameEnd()
{
	PRT_DEBUG("V9990GLRasterizer::frameEnd()");
}

void V9990GLRasterizer::setDisplayMode(V9990DisplayMode /*mode*/)
{
}

void V9990GLRasterizer::setColorMode(V9990ColorMode /*mode*/)
{
}

void V9990GLRasterizer::setPalette(int /*index*/, byte /*r*/, byte /*g*/, byte /*b*/)
{
}

void V9990GLRasterizer::drawBorder(int /*fromX*/, int /*fromY*/, int /*toX*/, int /*toY*/)
{
}

void V9990GLRasterizer::drawDisplay(
	int /*fromX*/, int /*fromY*/, int /*displayX*/, int /*displayY*/,
	int /*displayWidth*/, int /*displayHeight*/)
{
}

} // namespace openmsx


