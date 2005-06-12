// $Id$

#ifndef MSXMOTHERBOARD_HH
#define MSXMOTHERBOARD_HH

#include <vector>
#include <memory>

namespace openmsx {

class CliComm;
class MSXDevice;
class XMLElement;

class MSXMotherBoard
{
public:
	MSXMotherBoard();
	virtual ~MSXMotherBoard();

	void execute();

	void block();
	void unblock();

	void powerUpMSX();
	void powerDownMSX();

	/**
	 * This will reset all MSXDevices (the reset() method of
	 * all registered MSXDevices is called)
	 */
	void resetMSX();

private:
	/**
	 * All MSXDevices should be registered by the MotherBoard.
	 * This method should only be called at start-up
	 */
	void addDevice(std::auto_ptr<MSXDevice> device);

	void createDevices(const XMLElement& elem);

	typedef std::vector<MSXDevice*> Devices;
	Devices availableDevices;

	int blockedCounter;

	CliComm& output;
};

} // namespace openmsx

#endif
