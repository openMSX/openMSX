// $Id$

#ifndef __DUMMYCASSETTEIMAGE_HH__
#define __DUMMYCASSETTEIMAGE_HH__

#include "CassetteImage.hh"

class DummyCassetteImage : public CassetteImage
{
	public:
		DummyCassetteImage();
		virtual ~DummyCassetteImage();
	
		virtual short getSampleAt(float pos);
};

#endif
