#include "FDC_DSK.hh"

FDC_DSK::FDC_DSK(MSXConfig::Device *config ) : FDCBackEnd(config)
{
  PRT_DEBUG("Creating an FDC_DSK object...");
}

FDC_DSK::~FDC_DSK()
{
  PRT_DEBUG("Destroying an FDC_DSK object...");
}

byte* FDC_DSK::read(int track, int sector, int side, int size)
{
  PRT_DEBUG("FDC_DSK::read(track "<<track<<", sector "<<sector<<", side "<<side<<", size "<<size<<")");
  return NULL;
}
void FDC_DSK::write(int track, int sector, int side, int size, byte* buf)
{
  PRT_DEBUG("FDC_DSK::write(track "<<track<<", sector "<<sector<<", side "<<side<<", size "<<size<<", buf "<<std::hex<<buf<<std::dec<<")");
}
