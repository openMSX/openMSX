// $Id$

#include "V9990DummyRasterizer.hh"

using std::string;

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

void V9990DummyRasterizer::frameStart(const V9990DisplayPeriod *horTiming,
                                      const V9990DisplayPeriod *verTiming)
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
