// $Id$

#ifndef __FDC_HH__
#define __FDC_HH__

#include "openmsx.hh"

//forward declarations
class EmuTime;


class FDC
{
	public:
		virtual void reset() = 0;
		virtual byte getStatusReg(const EmuTime &time) = 0;
		virtual byte getTrackReg(const EmuTime &time) = 0;
		virtual byte getSectorReg(const EmuTime &time) = 0;
		virtual byte getDataReg(const EmuTime &time) = 0;
		virtual byte getSideSelect(const EmuTime &time) = 0;
		virtual byte getDriveSelect(const EmuTime &time) = 0;
		virtual byte getMotor(const EmuTime &time) = 0;
		virtual byte getIRQ(const EmuTime &time) = 0;
		virtual byte getDTRQ(const EmuTime &time) = 0;
		virtual void setCommandReg(byte value,const EmuTime &time) = 0;
		virtual void setTrackReg(byte value,const EmuTime &time) = 0;
		virtual void setSectorReg(byte value,const EmuTime &time) = 0;
		virtual void setDataReg(byte value,const EmuTime &time) = 0;
		virtual void setSideSelect(byte value,const EmuTime &time) = 0;
		virtual void setDriveSelect(byte value,const EmuTime &time) = 0;
		virtual void setMotor(byte value,const EmuTime &time) = 0;
};

#endif
