#include "FDCBackEnd.hh"

FDCBackEnd::FDCBackEnd(MSXConfig::Device *config )
{
  PRT_DEBUG("Creating an FDC back-end");
}

FDCBackEnd::~FDCBackEnd()
{
  PRT_DEBUG("Destroying an FDC back-end");
}

void FDCBackEnd::read(byte phystrack, byte track, byte sector, byte side, int size, byte* buf)
{
  PRT_DEBUG("FDCBackEnd::read(track "<<track<<", sector "<<sector<<", side "<<side<<", size "<<size<<")");
  assert(true);
}
void FDCBackEnd::write(byte phystrack, byte track, byte sector, byte side, int size, byte* buf)
{
  PRT_DEBUG("FDCBackEnd::write(track "<<track<<", sector "<<sector<<", side "<<side<<", size "<<size<<", buf "<<std::hex<<buf<<std::dec<<")");
  assert(true);
}
void FDCBackEnd::getTrackHeader(byte phystrack, byte track, byte side, byte* buf)
{
  PRT_DEBUG("Routine to get the TrackHeader from the current track");
}
void FDCBackEnd::getSectorHeader(byte phystrack, byte track, byte sector, byte side, byte* buf)
{
  PRT_DEBUG("Routine to get the sectorHeader from the current sector");
}

