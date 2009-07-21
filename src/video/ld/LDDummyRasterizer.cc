// $Id$

#include "LDDummyRasterizer.hh"

namespace openmsx {

void LDDummyRasterizer::frameStart(EmuTime::param /*time*/)
{
}

void LDDummyRasterizer::drawBlank(int /*r*/, int /*g*/, int /*b*/)
{
}

void LDDummyRasterizer::drawBitmap(const byte* /*bitmap*/)
{
}

} // namespace openmsx
