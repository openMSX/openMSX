// $Id$

#ifndef __CASSETTEIMAGE_HH__
#define __CASSETTEIMAGE_HH__

namespace openmsx {

class EmuTime;

class CassetteImage
{
public:
	virtual ~CassetteImage() {}
	virtual short getSampleAt(const EmuTime &time) = 0;
};

} // namespace openmsx

#endif
