// $Id$

#ifndef IDEHD_HH
#define IDEHD_HH

#include "HD.hh"
#include "AbstractIDEDevice.hh"
#include "serialize_meta.hh"
#include "noncopyable.hh"

namespace openmsx {

class MSXMotherBoard;
class XMLElement;
class DiskManipulator;

class IDEHD : public HD, public AbstractIDEDevice, private noncopyable
{
public:
	IDEHD(MSXMotherBoard& motherBoard, const XMLElement& config,
	      const EmuTime& time);
	virtual ~IDEHD();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// AbstractIDEDevice:
	virtual bool isPacketDevice();
	virtual const std::string& getDeviceName();
	virtual void fillIdentifyBlock(byte* buffer);
	virtual unsigned readBlockStart(byte* buffer, unsigned count);
	virtual void writeBlockComplete(byte* buffer, unsigned count);
	virtual void executeCommand(byte cmd);

	DiskManipulator& diskManipulator;
	unsigned transferSectorNumber;
};

REGISTER_POLYMORPHIC_INITIALIZER(IDEDevice, IDEHD, "IDEHD");

} // namespace openmsx

#endif
