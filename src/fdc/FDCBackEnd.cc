#include "FDCBackEnd.hh"

FDCBackEnd::FDCBackEnd(MSXConfig::Device *config )
{
  PRT_DEBUG("Creating an FDC back-end");
}

FDCBackEnd::~FDCBackEnd()
{
  PRT_DEBUG("Destroying an FDC back-end");
}

byte* FDCBackEnd::read(int track, int sector, int side, int size)
{
  PRT_DEBUG("FDCBackEnd::read(track "<<track<<", sector "<<sector<<", side "<<side<<", size "<<size<<")");
  assert(true);
  return NULL;

}
void FDCBackEnd::write(int track, int sector, int side, int size, byte* buf)
{
  PRT_DEBUG("FDCBackEnd::write(track "<<track<<", sector "<<sector<<", side "<<side<<", size "<<size<<", buf "<<std::hex<<buf<<std::dec<<")");
  assert(true);
}

