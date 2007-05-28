// $Id$
/* Ported from:
** Source: /cvsroot/bluemsx/blueMSX/Src/IoDevice/MB89352.h,v 
** Revision: 1.4
** Date: 2007/03/28 17:35:35
**
** More info: http://www.bluemsx.com
**
** Copyright (C) 2003-2007 Daniel Vik, white cat
*/

#ifndef MB89352_HH
#define MB89352_HH

#include "SCSI.hh"
#include <memory>
#include <vector>

namespace openmsx {

class SCSIDevice;
class XMLElement;
class MSXMotherBoard;

class MB89352 {
public:
	MB89352(MSXMotherBoard& motherBoard, const XMLElement& config);
	~MB89352();

	void reset(int scsireset);
	byte readRegister(byte reg);
	byte peekRegister(byte reg);
	byte readDREG();
	void writeRegister(byte reg, byte value);
	void writeDREG(byte value);

private:
	void disconnect();
	void softReset();
	void setACKREQ(byte* value);
	void resetACKREQ();
	byte getSSTS();

	int myId;                       // SPC SCSI ID 0..7
	int targetId;                   // SCSI Device target ID 0..7
	int regs[16];                   // SPC register
	int rst;                        // SCSI bus reset signal
	int atn;                        // SCSI bus attention signal
	SCSI::Phase phase;              //
	SCSI::Phase nextPhase;          // for message system
	int isEnabled;                  // spc enable flag
	int isBusy;                     // spc now working
	int isTransfer;                 // hardware transfer mode
	int msgin;                      // Message In flag
	int counter;                    // read and written number of bytes
	                                // within the range in the buffer
	int blockCounter;               // Number of blocks outside buffer
	                                // (512bytes / block)
	int tc;                         // counter for hardware transfer
	//TODO: int devBusy;            // CDROM busy (buffer conflict prevention)
	std::auto_ptr<SCSIDevice> dev[8];
	byte* pCdb;                     // cdb pointer
	unsigned bufIdx;                // buffer index
	byte  cdb[12];                  // Command Descripter Block
	std::vector<byte> buffer;       // buffer for transfer
};

} // namespace openmsx

#endif
