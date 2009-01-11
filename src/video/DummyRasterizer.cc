// $Id$

#include "DummyRasterizer.hh"

namespace openmsx {

bool DummyRasterizer::isActive()
{
	return false;
}

void DummyRasterizer::reset()
{
}

void DummyRasterizer::frameStart(EmuTime::param /*time*/)
{
}

void DummyRasterizer::frameEnd()
{
}

void DummyRasterizer::setDisplayMode(DisplayMode /*mode*/)
{
}

void DummyRasterizer::setPalette(int /*index*/, int /*grb*/)
{
}

void DummyRasterizer::setBackgroundColour(int /*index*/)
{
}

void DummyRasterizer::setTransparency(bool /*enabled*/)
{
}

void DummyRasterizer::drawBorder(int /*fromX*/, int /*fromY*/,
                                 int /*limitX*/, int /*limitY*/)
{
}

void DummyRasterizer::drawDisplay(
	int /*fromX*/, int /*fromY*/,
	int /*displayX*/, int /*displayY*/,
	int /*displayWidth*/, int /*displayHeight*/)
{
}

void DummyRasterizer::drawSprites(
	int /*fromX*/, int /*fromY*/,
	int /*displayX*/, int /*displayY*/,
	int /*displayWidth*/, int /*displayHeight*/)
{
}

bool DummyRasterizer::isRecording() const
{
	return false;
}

} // namespace openmsx
