// $Id$

#ifndef __CASSETTEIMAGE_HH__
#define __CASSETTEIMAGE_HH__

class EmuTime;

class CassetteImage
{
	public:
		virtual short getSampleAt(const EmuTime &time) = 0;
};

#endif
