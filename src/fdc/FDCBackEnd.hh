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
		
		virtual bool read( byte phystrack, byte track, byte sector, byte side, int size, byte* buf);
		virtual bool write(byte phystrack, byte track, byte sector, byte side, int size, byte* buf);
		virtual void getSectorHeader(byte phystrack, byte track, byte sector, byte side, byte* buf);
		virtual void getTrackHeader(byte phystrack, byte track, byte side, byte* buf);
};
#endif
