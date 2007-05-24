// $Id$
#ifndef SCSIDEVICE_HH
#define SCSIDEVICE_HH

#include "SCSI.hh"

namespace openmsx {

class SCSIDevice
{
public:
	static const int BIT_SCSI2          = 0x0001;
	static const int BIT_SCSI2_ONLY     = 0x0002;
	static const int BIT_SCSI3          = 0x0004;

	static const int MODE_SCSI1         = 0x0000;
	static const int MODE_SCSI2         = 0x0003;
	static const int MODE_SCSI3         = 0x0005;
	static const int MODE_UNITATTENTION = 0x0008; // report unit attention
	static const int MODE_MEGASCSI      = 0x0010; // report disk change when call of
	                                              // 'test unit ready'
	static const int MODE_FDS120        = 0x0020; // change of inquiry when inserted
	                                              // floppy image
	static const int MODE_CHECK2        = 0x0040; // mask to diskchange when
	                                              // load state
	static const int MODE_REMOVABLE     = 0x0080;
	static const int MODE_NOVAXIS       = 0x0100;

	static const unsigned BUFFER_SIZE   = 0x10000; // 64KB

	virtual ~SCSIDevice() {};

	virtual void reset() = 0;
	virtual bool isSelected() = 0;
	virtual int executeCmd(const byte* cdb, SCSI::Phase& phase, int& blocks) = 0;
	virtual int executingCmd(SCSI::Phase& phase, int& blocks) = 0;
	virtual byte getStatusCode() = 0;
	virtual int msgOut(byte value) = 0;
	virtual byte msgIn() = 0;
	virtual void disconnect() = 0;
	virtual void busReset() = 0; // only used in MB89352 controller

	virtual int dataIn(int& blocks) = 0;
	virtual int dataOut(int& blocks) = 0;
};

} // namespace openmsx

#endif
