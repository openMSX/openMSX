#include "FDC.hh"

FDC::FDC(MSXConfig::Device *config)
{
  PRT_DEBUG("instantiating an FDC object..");
};

FDC::~FDC()
{
  PRT_DEBUG("Destructing an FDC object..");
};

void FDC::reset()
{
  PRT_DEBUG("Direct FDC object method call!");
  assert(true);
};

void FDC::setCommandReg(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
};
void FDC::setTrackReg(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
};
void FDC::setSectorReg(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
};
void FDC::setDataReg(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
};

byte FDC::getStatusReg(const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
  return 0;
}
byte FDC::getTrackReg(const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
  return 0;
}
byte FDC::getSectorReg(const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
  return 0;
}
byte FDC::getDataReg(const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
  return 0;
}
