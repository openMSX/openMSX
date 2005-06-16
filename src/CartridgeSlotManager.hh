// $Id$

#ifndef CARTRIDGESLOTMANAGER_HH
#define CARTRIDGESLOTMANAGER_HH

namespace openmsx {

class MSXMotherBoard;
class HardwareConfig;

class CartridgeSlotManager
{
public:
	CartridgeSlotManager(MSXMotherBoard& motherBoard);
	~CartridgeSlotManager();

	void readConfig();

	void reserveSlot(int slot);
	void getSlot(int slot, int& ps, int& ss);
	void getSlot(int& ps, int& ss);
	void getSlot(int& ps);

	static int getSlotNum(const std::string& slot);

private:
	void createExternal(unsigned ps);
	void createExternal(unsigned ps, unsigned ss);

	int slots[16];
	int slotCounter;
	MSXMotherBoard& motherBoard;
	HardwareConfig& hardwareConfig;
};

} // namespace openmsx

#endif
