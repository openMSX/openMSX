/*****************************************************************************
** $Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/wd33c93.h,v $
**
** $Revision: 1.6 $
**
** $Date$
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, Ricardo Bittencourt, white cat
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
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

	void disconnect();

private:
	void xferCb(int length);
	void execCmd(byte value);

	int         myId;
	int         targetId;
	byte       latch;
	byte       regs[32];
        std::auto_ptr<SCSIDevice> dev[8];
	int         maxDev;
	SCSI_PHASE  phase;
	//SCSI_PHASE  nextPhase;
	//BoardTimer* timer;
	//unsigned    timeout;
	//int       timerRunning;
	int         counter;
	int         blockCounter;
	int         tc;
	int         devBusy;
	byte*      pBuf;
	byte*      buffer;
};

} // namespace openmsx

#endif
