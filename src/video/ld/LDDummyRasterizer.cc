// $Id$

#include "LDDummyRasterizer.hh"

namespace openmsx {

bool LDDummyRasterizer::isActive()
{
	return false;
}

void LDDummyRasterizer::reset()
{
}

void LDDummyRasterizer::frameStart(EmuTime::param /*time*/)
{
}

void LDDummyRasterizer::frameEnd()
{
}

bool LDDummyRasterizer::isRecording() const
{
	return false;
}

void LDDummyRasterizer::drawBlank(int /*r*/, int /*g*/, int /*b*/)
{
}

void LDDummyRasterizer::drawBitmap(byte* /*bitmap*/)
{
}

} // namespace openmsx
