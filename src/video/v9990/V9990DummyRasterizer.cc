// $Id$

#include "V9990DummyRasterizer.hh"

namespace openmsx {

void V9990DummyRasterizer::paint()
{
}

V9990DummyRasterizer::~V9990DummyRasterizer()
{
}

const string& V9990DummyRasterizer::getName()
{
	static const string NAME = "V9990DummyRasterizer";
	return NAME;
}

void V9990DummyRasterizer::reset()
{
}

void V9990DummyRasterizer::frameStart()
{
}

void V9990DummyRasterizer::frameEnd()
{
}

void V9990DummyRasterizer::setDisplayMode(V9990DisplayMode displayMode)
{
}

void V9990DummyRasterizer::setColorMode(V9990ColorMode colorMode)
{
}

void V9990DummyRasterizer::setBackgroundColor(int index)
{
}

void V9990DummyRasterizer::setPalette(int index, byte r, byte g, byte b)
{
}

void V9990DummyRasterizer::setImageWidth(int width)
{
}

void V9990DummyRasterizer::drawBorder(int fromX, int fromY,
                                      int limitX, int limitY)
{
}

void V9990DummyRasterizer::drawDisplay(int fromX, int fromY,
                                       int displayX, int displayY,
                                       int displayWidth, int displayHeight)
{
}

} // namespace openmsx
