#ifndef __FDC_DSK__HH__
#define __FDC_DSK__HH__

#include "FDCBackEnd.hh"

class FDC_DSK  : public FDCBackEnd
{
  public: 
  	FDC_DSK(MSXConfig::Device *config );
	virtual ~FDC_DSK();
	virtual byte* read(int track, int sector, int side, int size);
	virtual void write(int track, int sector, int side, int size, byte* buf);
};
#endif
  

