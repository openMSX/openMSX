// $Id$

#ifndef DUMMYCASSETTEIMAGE_HH
#define DUMMYCASSETTEIMAGE_HH

#include "CassetteImage.hh"

namespace openmsx {

class DummyCassetteImage : public CassetteImage
{
public:
	DummyCassetteImage();
	virtual ~DummyCassetteImage();

	virtual short getSampleAt(const EmuTime &time);
};

} // namespace openmsx

#endif
