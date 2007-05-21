// $Id$
/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/wd33c93.h,v
** Revision: 1.6
** $Date$
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, Ricardo Bittencourt, white cat
*/

#ifndef WD33C93_HH
#define WD33C93_HH

#include "SCSIDevice.hh" // needed for type SCSI_PHASE
#include "openmsx.hh"
#include <memory>

namespace openmsx {

class XMLElement;

class WD33C93
{
public:
	WD33C93(const XMLElement& config);
	~WD33C93();

	void reset(bool scsireset);

	byte readAuxStatus();
	byte readCtrl();
	byte peekAuxStatus() const;
	byte peekCtrl() const;
	void writeAdr(byte value);
	void writeCtrl(byte value);

private:
	void disconnect();
	void execCmd(byte value);

	int         myId;
	int         targetId;
	byte       latch;
	byte       regs[32];
        std::auto_ptr<SCSIDevice> dev[8];
	int         maxDev;
	SCSI_PHASE  phase;
	int         counter;
	int         blockCounter;
	int         tc;
	bool         devBusy;
	byte*      pBuf;
	byte*      buffer;
};

} // namespace openmsx

#endif
