#ifndef __FDCBACKEND__HH__
#define __FDCBACKEND__HH__

#include "MSXConfig.hh"


class FDCBackEnd 
{
	public:
		/**
		 * Constructor
		 */
		FDCBackEnd(MSXConfig::Device *config);

		/**
		 * Destructor
		 */
		virtual ~FDCBackEnd();
		
		virtual byte* read(int track, int sector, int side, int size);
		virtual void write(int track, int sector, int side, int size, byte* buf);
};
#endif
