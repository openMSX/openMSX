#ifndef __FDC_DSK__HH__
#define __FDC_DSK__HH__

#include "FDCBackEnd.hh"
#include "MSXDiskRomPatch.hh"

class FDC_DSK  : public FDCBackEnd
{
  public: 
  	FDC_DSK(MSXConfig::Device *config );
	virtual ~FDC_DSK();
	virtual void read(byte phystrack, byte track, byte sector, byte side, int size, byte* buf);
	virtual void write(byte phystrack, byte track, byte sector, byte side, int size, byte* buf);
  private:
  	MSXDiskRomPatch::DiskImage *disk[1];
};
#endif
  

