// $Id$

#include "FDC.hh"


FDC::FDC(MSXConfig::Device *config)
{
  PRT_DEBUG("instantiating an FDC object..");
}

FDC::~FDC()
{
  PRT_DEBUG("Destructing an FDC object..");
}

void FDC::reset()
{
  PRT_DEBUG("Direct FDC object method call!");
  assert(true);
}

void FDC::setCommandReg(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
}
void FDC::setTrackReg(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
}
void FDC::setSectorReg(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
}
void FDC::setDataReg(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
}
void FDC::setSideSelect(byte value, const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
}
byte FDC::getIRQ(const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
  return 1;
}
byte FDC::getDTRQ(const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
  return 1;
}
byte FDC::getSideSelect(const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
  return 0;
}
void FDC::setDriveSelect(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
}

byte FDC::getDriveSelect(const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
  return 0;	// avoid warning
}

void FDC::setMotor(byte value,const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
}

byte FDC::getMotor(const EmuTime &time)
{
  PRT_DEBUG("Direct FDC object method call!");
  return 0;	// avoid warning
}

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
