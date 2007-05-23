// $Id$
#ifndef DUMMYSCSIDEVICE_HH
#define DUMMYSCSIDEVICE_HH

#include "SCSIDevice.hh"

namespace openmsx {

class DummySCSIDevice : public SCSIDevice
{
public:
	virtual void reset();
	virtual bool isSelected();
	virtual int executeCmd(const byte* cdb, SCSI::Phase& phase, int& blocks);
	virtual int executingCmd(SCSI::Phase& phase, int& blocks);
	virtual byte getStatusCode();
	virtual int msgOut(byte value);
	virtual byte msgIn();
	virtual void disconnect();
	virtual void busReset(); // only used in MB89352 controller

	virtual int dataIn(int& blocks);
	virtual int dataOut(int& blocks);
};

} // namespace openmsx

#endif
