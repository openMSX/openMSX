// $Id$

#ifndef __FDC_HH__
#define __FDC_HH__

#include "MSXConfig.hh"
#include "FDCBackEnd.hh"


class FDC
{
	protected:
		FDC(MSXConfig::Device *config);
		virtual ~FDC();
	
		FDCBackEnd* getBackEnd(int drive);
	
	private:
		std::string driveName[2];
		int numDrives;	// number of connected drives (1 or 2)
};

#endif
