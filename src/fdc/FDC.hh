#ifndef __FDC_HH__
#define __FDC_HH__

#include "openmsx.hh"
#include "MSXConfig.hh"

class EmuTime;

class FDC
{
  public:
    virtual ~FDC();

    virtual void reset();
    virtual byte getStatusReg(const EmuTime &time);
    virtual byte getTrackReg(const EmuTime &time);
    virtual byte getSectorReg(const EmuTime &time);
    virtual byte getDataReg(const EmuTime &time);
    virtual byte getSideSelect(const EmuTime &time);
    virtual byte getDriveSelect(const EmuTime &time);
    virtual byte getIRQ(const EmuTime &time);
    virtual byte getDTRQ(const EmuTime &time);
    virtual void setCommandReg(byte value,const EmuTime &time);
    virtual void setTrackReg(byte value,const EmuTime &time);
    virtual void setSectorReg(byte value,const EmuTime &time);
    virtual void setDataReg(byte value,const EmuTime &time);
    virtual void setSideSelect(byte value,const EmuTime &time);
    virtual void setDriveSelect(byte value,const EmuTime &time);

    FDC(MSXConfig::Device *config);
};

#endif
