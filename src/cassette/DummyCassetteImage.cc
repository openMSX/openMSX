// $Id$

#include "DummyCassetteImage.hh"

namespace openmsx {

DummyCassetteImage::DummyCassetteImage()
{
}

DummyCassetteImage::~DummyCassetteImage()
{
}

short DummyCassetteImage::getSampleAt(const EmuTime &time)
{
	return 0;
}

} // namespace openmsx
