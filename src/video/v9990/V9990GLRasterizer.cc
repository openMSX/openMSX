// $Id$

#include "V9990GLRasterizer.hh"

namespace openmsx {

V9990GLRasterizer::V9990GLRasterizer(V9990& /*vdp*/)
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

void V9990GLRasterizer::frameEnd(const EmuTime& /*time*/)
{
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
	int /*fromX*/, int /*fromY*/, int /*displayX*/,
	int /*displayY*/, int /*displayYA*/, int /*displayYB*/,
	int /*displayWidth*/, int /*displayHeight*/)
{
}

bool V9990GLRasterizer::isRecording() const
{
	return false;
}

} // namespace openmsx
