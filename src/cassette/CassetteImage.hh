// $Id$

#ifndef CASSETTEIMAGE_HH
#define CASSETTEIMAGE_HH

namespace openmsx {

class EmuTime;

class CassetteImage
{
public:
	virtual ~CassetteImage() {}
	virtual short getSampleAt(const EmuTime& time) = 0;
};

} // namespace openmsx

#endif
