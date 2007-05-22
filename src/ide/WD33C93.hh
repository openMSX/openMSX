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

#include "SCSI.hh"
#include <memory>

namespace openmsx {

class SCSIDevice;
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

	std::auto_ptr<SCSIDevice> dev[8];
	byte* pBuf;
	byte* buffer;
	int counter;
	int blockCounter;
	int tc;
	SCSI::Phase phase;
	byte myId;
	byte targetId;
	byte regs[32];
	byte latch;
	bool devBusy;
};

} // namespace openmsx

#endif
